#!/usr/bin/env python3
"""Check whether two Macs can exchange the UDP traffic LAN discovery needs.

OpenTS discovers hosted games by broadcasting on UDP port 1234. macOS 15 and
newer can block that, and so can the application firewall or a network that
suppresses broadcasts. This tool tests the network without the game.

    python3 lan_probe.py listen
    python3 lan_probe.py send <other-Mac-IP>

Run `listen` on one Mac and `send` on the other, then swap the roles. The
sender emits both a broadcast and a direct unicast to the address given, so the
listener's output distinguishes "broadcasts do not arrive" from "no UDP arrives
at all". Neither command needs the game data or administrator rights.
"""

import argparse
import socket
import sys
import time

PORT = 1234


def _socket_for_listening():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    # SO_REUSEPORT lets a second copy of the game hold port 1234 as well.
    if hasattr(socket, "SO_REUSEPORT"):
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    sock.bind(("", PORT))
    sock.settimeout(1.0)
    return sock


def listen():
    sock = _socket_for_listening()
    local_ips = _local_addresses()
    print(f"Listening on UDP {PORT} (this Mac: {', '.join(local_ips) or 'unknown'})")
    print("Waiting for datagrams. Run 'lan_probe.py send <this-Mac-IP>' on the other Mac.")
    print("Press Ctrl-C to stop.\n")
    counts = {}
    try:
        while True:
            try:
                data, addr = sock.recvfrom(2048)
            except socket.timeout:
                continue
            text = data.decode("utf-8", errors="replace").strip()
            kind = text.split("|", 1)[0] if "|" in text else "?"
            counts[kind] = counts.get(kind, 0) + 1
            print(f"  RECEIVED from {addr[0]}:{addr[1]}  {text}")
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        sock.close()
    if not counts:
        print("No datagrams. Check Local Network permission, the firewall, and")
        print("whether both Macs are on the same subnet with broadcasts allowed.")
    else:
        print("Totals: " + ", ".join(f"{k}={v}" for k, v in sorted(counts.items())))
        if "unicast" in counts and "broadcast" not in counts:
            print("Unicast works but broadcasts do not: Local Network permission or")
            print("wireless/AP isolation is the likely cause.")
        elif "unicast" not in counts and "broadcast" in counts:
            print("Broadcasts work but unicast does not: check the firewall on the sender.")


def send(target):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

    # The engine broadcasts to the interface's directed address (for example
    # 192.168.1.255), not only to the all-ones 255.255.255.255. Send both, plus
    # a unicast, so a listener can tell which path the network allows.
    octets = target.split(".")
    directed = ".".join(octets[:3] + ["255"]) if len(octets) == 4 else None
    destinations = [("unicast", target)]
    if directed:
        destinations.append(("broadcast", directed))
    destinations.append(("broadcast", "255.255.255.255"))

    print(f"Sending on UDP {PORT} every 2s: " + ", ".join(f"{k}=>{a}" for k, a in destinations))
    print("Press Ctrl-C to stop.\n")
    seq = 0
    try:
        while True:
            seq += 1
            for kind, address in destinations:
                payload = f"{kind}|seq={seq}|via={address}|from={_primary_ip()}".encode()
                try:
                    sock.sendto(payload, (address, PORT))
                except OSError as exc:
                    print(f"  {kind} -> {address}: FAILED ({exc})")
            print(f"  sent seq={seq} to {len(destinations)} destinations")
            time.sleep(2)
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        sock.close()


def _local_addresses():
    """Local IPv4 addresses, without importing anything outside the stdlib."""
    result = []
    try:
        import subprocess
        out = subprocess.run(["/sbin/ifconfig"], capture_output=True, text=True, timeout=5).stdout
        for line in out.splitlines():
            line = line.strip()
            if line.startswith("inet ") and "127.0.0.1" not in line:
                result.append(line.split()[1])
    except (OSError, subprocess.SubprocessError):
        pass
    return result


def _primary_ip():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.connect(("8.8.8.8", 80))
        return sock.getsockname()[0]
    except OSError:
        return "unknown"
    finally:
        sock.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="mode", required=True)
    sub.add_parser("listen", help="print UDP datagrams received on port 1234")
    send_parser = sub.add_parser("send", help="send broadcast and unicast probes")
    send_parser.add_argument("target", help="IP address of the other Mac")
    args = parser.parse_args()
    if args.mode == "listen":
        listen()
    else:
        send(args.target)


if __name__ == "__main__":
    try:
        main()
    except PermissionError:
        print("Permission denied binding UDP port 1234.", file=sys.stderr)
        sys.exit(1)
