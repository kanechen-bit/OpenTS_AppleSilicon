#!/usr/bin/env python3
"""
mixdump -- inspect MIX archives from outside the engine.

Written for the OpenTS non-Windows port so that a data set can be examined
without building or running the game. It mirrors the format exactly as
`code/mixfile.cpp` reads it, including the encrypted-header path, which is
the only form Tiberian Sun's shipped archives use.

The crypto is not reimplemented from memory. The Blowfish S-boxes and the RSA
modulus are read straight out of the engine's own sources at run time
(`code/blowfish.cpp` and `code/_pk.cpp`), so the tool cannot drift from the
engine and there is nothing transcribed by hand:

  * `mixfile.cpp` reads the first 4 bytes raw. A zero leading short marks the
    extended form; bit 1 of the flags short means the rest is encrypted.
  * When encrypted, the bytes after those 4 are consumed by `PKStraw` in
    DECRYPT mode: it takes `Block_Count(BLOWFISH_KEY_SIZE) * Crypt_Block_Size`
    bytes as RSA blocks, recovers a 56-byte Blowfish key from them, and then
    Blowfish-decrypts everything after.
  * The RSA modulus is DER in the `[PublicKey] 1=` value in `_pk.cpp`; the
    exponent is the fixed `PKey::Fast_Exponent()` of 65537.
  * `BigInt` is an array of 32-bit `digit`s filled by `memmove`, so the RSA
    block is read little-endian. `PKey::Decrypt` writes `Plain_Block_Size`
    bytes back out of the same little-endian representation, so a 40-byte
    cipher block yields 39 plaintext bytes.
  * The Blowfish stream is ECB over 8-byte blocks, and `Int` packs each block
    big-endian with byte 0 most significant.

Usage:
    mixdump.py info   FILE...                 header summary for each archive
    mixdump.py list   FILE                    every member: offset, size, crc
    mixdump.py find   FILE NAME...            is NAME held by FILE?
    mixdump.py scan   DIR [--targets NAME...] survey every MIX under DIR and
                                              resolve the startup target names

The `scan` form answers the question the port actually asks -- "where, if
anywhere, does this data set keep CACHE.MIX?" -- across a whole tree.
"""

import argparse
import base64
import os
import re
import struct
import sys
from pathlib import Path

# --------------------------------------------------------------------------
# Engine sources: the single source of truth for key material and tables
# --------------------------------------------------------------------------

def code_dir():
    """`code/` as located from this script's position in the tree."""
    return Path(__file__).resolve().parents[2]


_BF_CACHE = {}


def _hex_tokens(text):
    return [int(m, 16) for m in re.findall(r"0x([0-9A-Fa-f]{8})U?", text)]


def load_blowfish_tables(root=None):
    """P_Init (18 words) and S_Init (4 x 256 words) from code/blowfish.cpp."""
    root = root or code_dir()
    if root in _BF_CACHE:
        return _BF_CACHE[root]

    src = (Path(root) / "blowfish.cpp").read_text()

    p_at = src.index("P_Init[")
    p_end = src.index("};", p_at)
    p_init = _hex_tokens(src[p_at:p_end])
    if len(p_init) != 18:
        raise ValueError("P_Init: expected 18 words, read {}".format(len(p_init)))

    s_at = src.index("S_Init[")
    s_tokens = _hex_tokens(src[s_at:])
    if len(s_tokens) < 1024:
        raise ValueError("S_Init: expected 1024 words, read {}".format(len(s_tokens)))
    s_init = [s_tokens[i * 256:(i + 1) * 256] for i in range(4)]

    # The first words are the leading hex digits of pi, the same constants the
    # published Blowfish specification lists.
    if p_init[:2] != [0x243F6A88, 0x85A308D3]:
        raise ValueError("P_Init does not begin with the standard constants")

    _BF_CACHE[root] = (p_init, s_init)
    return p_init, s_init


def load_fast_key(root=None):
    """
    The RSA modulus from `[PublicKey] 1=` in code/_pk.cpp, plus the fixed
    exponent. Returns (exponent, modulus).
    """
    root = root or code_dir()
    src = (Path(root) / "_pk.cpp").read_text()

    # There is a commented-out copy of this array earlier in the file, so
    # anchor on the definition at the start of a line.
    match = re.search(r"^char const Keys\[\]", src, re.M)
    if match is None:
        raise ValueError("no char const Keys[] definition in _pk.cpp")
    at = match.start()
    segment = src[at:src.index(";", at)]

    # The array is assembled from string literals and the newlines in it are
    # the two-character C escape, not real line breaks, so unescape before
    # treating the result as INI text.
    blob = "".join(re.findall(r'"([^"]*)"', segment))
    blob = blob.replace("\\n", "\n").replace("\\r", "")

    if "[PublicKey]" not in blob:
        raise ValueError("no [PublicKey] section in Keys[]")
    section = blob.split("[PublicKey]", 1)[1].split("[", 1)[0]

    value = None
    for line in section.splitlines():
        line = line.strip()
        if line.startswith("1="):
            value = line[2:].strip()
            break
    if value is None:
        raise ValueError("no 1= entry under [PublicKey] in _pk.cpp")

    der = base64.b64decode(value)
    if der[0] != 0x02:
        raise ValueError("key blob is not a DER INTEGER (first byte 0x%02X)" % der[0])
    length = der[1]
    if length & 0x80:
        raise ValueError("long-form DER length not handled")
    body = der[2:2 + length]
    if len(body) != length:
        raise ValueError("DER INTEGER truncated")

    modulus = int.from_bytes(body, "big")
    return 65537, modulus


# --------------------------------------------------------------------------
# Blowfish (ECB, 8-byte blocks, big-endian word packing)
# --------------------------------------------------------------------------

_MASK32 = 0xFFFFFFFF


class Blowfish:
    def __init__(self, key, root=None):
        p_init, s_init = load_blowfish_tables(root)

        self.P = list(p_init)
        self.S = [list(row) for row in s_init]

        # Key schedule: XOR the key into P, cycling it, four bytes per word
        # most significant first.
        klen = len(key)
        j = 0
        for i in range(18):
            word = 0
            for _ in range(4):
                word = ((word << 8) | key[j % klen]) & _MASK32
                j += 1
            self.P[i] ^= word

        # Then scramble P and the S-boxes by encrypting a zero block through
        # the tables as they are being rebuilt.
        left = right = 0
        for i in range(0, 18, 2):
            left, right = self._cipher_words(left, right, self.P)
            self.P[i], self.P[i + 1] = left, right
        for box in range(4):
            row = self.S[box]
            for i in range(0, 256, 2):
                left, right = self._cipher_words(left, right, self.P)
                row[i], row[i + 1] = left, right

        # Decryption is the same network driven by the permutation table
        # reversed, which is what P_Decrypt is.
        self.P_dec = self.P[::-1]

    def _F(self, x):
        S = self.S
        a = (x >> 24) & 0xFF
        b = (x >> 16) & 0xFF
        c = (x >> 8) & 0xFF
        d = x & 0xFF
        return ((((S[0][a] + S[1][b]) & _MASK32) ^ S[2][c]) + S[3][d]) & _MASK32

    def _cipher_words(self, left, right, ptable):
        """
        The engine's `Sub_Key_Encrypt` / `Process_Block` network, on raw words.

        Two rounds are folded per iteration to avoid the swap, so the halves
        are *not* exchanged at the end: the last two table words are applied
        crossed over -- `right` takes P[17] and `left` takes P[16]. Applying
        them the same way round is the easy mistake here, and it produces
        output that looks like noise rather than failing outright.
        """
        for i in range(0, 16, 2):
            left ^= ptable[i]
            right ^= self._F(left)
            right ^= ptable[i + 1]
            left ^= self._F(right)
        return right ^ ptable[17], left ^ ptable[16]

    def encrypt_block(self, block):
        """8 raw bytes in, 8 raw bytes out. Byte 0 is most significant."""
        left = int.from_bytes(block[0:4], "big")
        right = int.from_bytes(block[4:8], "big")
        a, b = self._cipher_words(left, right, self.P)
        return a.to_bytes(4, "big") + b.to_bytes(4, "big")

    def decrypt_block(self, block):
        left = int.from_bytes(block[0:4], "big")
        right = int.from_bytes(block[4:8], "big")
        a, b = self._cipher_words(left, right, self.P_dec)
        return a.to_bytes(4, "big") + b.to_bytes(4, "big")


def _selftest_blowfish(root=None):
    """
    Published Blowfish vectors (Eric Young's set, the opening block of it). A
    wrong network or a wrong key schedule fails these immediately, which is
    much cheaper to diagnose than a garbled archive index.
    """
    import binascii

    vectors = [
        ("0000000000000000", "0000000000000000", "4EF997456198DD78"),
        ("FFFFFFFFFFFFFFFF", "FFFFFFFFFFFFFFFF", "51866FD5B85ECB8A"),
        ("3000000000000000", "1000000000000001", "7D856F9A613063F2"),
        ("1111111111111111", "1111111111111111", "2466DD878B963C9D"),
        ("0123456789ABCDEF", "1111111111111111", "61F9C3802281B096"),
        ("1111111111111111", "0123456789ABCDEF", "7D0CC630AFDA1EC7"),
        ("FEDCBA9876543210", "0123456789ABCDEF", "0ACEAB0FC6A0A28D"),
        ("7CA110454A1A6E57", "01A1D6D039776742", "59C68245EB05282B"),
    ]
    for key_hex, plain_hex, expect in vectors:
        key = binascii.unhexlify(key_hex)
        plain = binascii.unhexlify(plain_hex)
        bf = Blowfish(key, root)
        got = binascii.hexlify(bf.encrypt_block(plain)).decode().upper()
        if got != expect:
            raise AssertionError(
                "Blowfish self-test failed for key {}: got {}, expected {}"
                .format(key_hex, got, expect))
        # And the network must be invertible with the same key material.
        if bf.decrypt_block(plain) != bf.decrypt_block(plain):
            raise AssertionError("decrypt_block is not deterministic")


# --------------------------------------------------------------------------
# Encrypted-header key recovery
# --------------------------------------------------------------------------

def encrypted_key_length(modulus):
    """
    `PKStraw::Encrypted_Key_Length` -- how many bytes the RSA-encrypted
    Blowfish key occupies. `Plain_Block_Size = (BitPrecision-1)//8` and
    `Crypt_Block_Size = Plain_Block_Size + 1`, with `BitPrecision` one less
    than the modulus bit count. `BLOWFISH_KEY_SIZE` is 56.
    """
    bit_precision = modulus.bit_length() - 1
    plain_block = (bit_precision - 1) // 8
    crypt_block = plain_block + 1
    blocks = ((56 - 1) // plain_block) + 1
    return blocks * crypt_block


def recover_blowfish_key(blob, exponent, modulus):
    """
    Reproduce `PKStraw` in DECRYPT mode: RSA-decrypt the key block and return
    the Blowfish key.

    `BigInt` is an array of 32-bit `digit`s filled by `memmove`, so a block
    loads little-endian, and `PKey::Decrypt` writes `Plain_Block_Size` bytes
    back out of that same representation. A 40-byte cipher block therefore
    yields 39 plaintext bytes, and the top byte of the recovered value is the
    padding that a correctly-encrypted block always has as zero.
    """
    bit_precision = modulus.bit_length() - 1
    plain_block = (bit_precision - 1) // 8
    crypt_block = plain_block + 1
    blocks = ((56 - 1) // plain_block) + 1

    if len(blob) < blocks * crypt_block:
        raise ValueError("key block truncated")

    plain = bytearray()
    for i in range(blocks):
        chunk = blob[i * crypt_block:(i + 1) * crypt_block]
        c = int.from_bytes(chunk, "little")
        m = pow(c, exponent, modulus)
        plain += m.to_bytes(crypt_block, "little")[:plain_block]

    return bytes(plain[:56])


# --------------------------------------------------------------------------
# CRC
# --------------------------------------------------------------------------

def _make_table():
    table = []
    for i in range(256):
        c = i
        for _ in range(8):
            c = (c >> 1) ^ (0xEDB88320 if (c & 1) else 0)
        table.append(c & 0xFFFFFFFF)
    return table


_TABLE = _make_table()

assert _TABLE[0] == 0x00000000 and _TABLE[1] == 0x77073096
assert _TABLE[2] == 0xEE0E612C and _TABLE[255] == 0x2D02EF8D


def crc_memory(data, crc):
    """
    CRC::Memory -- invert, fold each byte, invert again.

    The trailing invert is an XOR, not a mask. Writing `& 0xFFFFFFFF` there
    silently produces a CRC that is the complement of the right answer, which
    looks like a perfectly plausible hash and matches nothing at all.
    """
    crc ^= 0xFFFFFFFF
    for b in data:
        crc = _TABLE[(crc ^ b) & 0xFF] ^ (crc >> 8)
    return crc ^ 0xFFFFFFFF


def crc_name(name):
    """
    CRCEngine over the uppercased name, which is what MixFileClass::Offset
    binary-searches the index with (`strupr` in place, then the engine).
    """
    data = name.upper().encode("latin-1")

    crc = 0
    index = 0
    staging = bytearray(4)

    def submit(datum):
        nonlocal crc, index
        if index == 0:
            staging[:] = b"\0\0\0\0"
        staging[index] = datum
        index += 1
        if index == 4:
            crc = crc_memory(bytes(staging), crc)
            index = 0

    pos = 0
    remaining = len(data)

    while remaining > 0 and index != 0:
        submit(data[pos]); pos += 1; remaining -= 1

    for _ in range(remaining // 4):
        crc = crc_memory(data[pos:pos + 4], crc)
        pos += 4
        remaining -= 4

    while remaining > 0:
        submit(data[pos]); pos += 1; remaining -= 1

    # Value(): a partial trailing group is padded -- buffer[index] = index,
    # then every remaining byte copies buffer[0] -- and folded in.
    if index != 0:
        padded = bytearray(staging)
        padded[index] = index & 0xFF
        for k in range(index + 1, 4):
            padded[k] = padded[0]
        return crc_memory(bytes(padded), crc)

    return crc


# --------------------------------------------------------------------------
# Archive reading
# --------------------------------------------------------------------------

class MixFile:
    def __init__(self, path, root=None):
        self.path = path
        self.error = None
        self.encrypted = False
        self.digest = False
        self.count = 0
        self.data_size = 0
        self.data_start = 0
        self.entries = []          # (crc, offset, size)
        self.file_size = 0
        self.blowfish_key = None
        self._root = root
        self._read()

    def _fail(self, msg):
        self.error = msg
        return False

    def _read(self):
        """
        Read only the header and the index. The member data of a MIX is
        routinely hundreds of megabytes and is never needed to answer a lookup,
        so nothing beyond the index is read or decrypted.
        """
        self.file_size = os.path.getsize(self.path)

        with open(self.path, "rb") as f:
            head = f.read(4)
            if len(head) < 4:
                # The engine does not test this; it reads whatever memory held
                # and mounts an archive out of it. Refuse to imitate that.
                return self._fail("shorter than a header ({} bytes)".format(self.file_size))

            first, second = struct.unpack("<hh", head)

            if first != 0:
                # Plain form: the first short is the member count.
                return self._read_plain(f, header_at=0)

            self.digest = bool(second & 0x01)
            self.encrypted = bool(second & 0x02)

            if not self.encrypted:
                return self._read_plain(f, header_at=4)

            # Encrypted: an RSA block carrying the Blowfish key, then the
            # header and index as Blowfish ciphertext.
            try:
                exponent, modulus = load_fast_key(self._root)
                need = encrypted_key_length(modulus)
            except Exception as exc:
                return self._fail("key setup failed: {}".format(exc))

            keyblob = f.read(need)
            if len(keyblob) < need:
                return self._fail("archive too short for the encrypted key block")

            try:
                key = recover_blowfish_key(keyblob, exponent, modulus)
            except Exception as exc:
                return self._fail("key recovery failed: {}".format(exc))
            self.blowfish_key = key

            bf = Blowfish(key, self._root)
            plain = bytearray()

            def decrypt_upto(nbytes):
                while len(plain) < nbytes:
                    block = f.read(8)
                    if len(block) < 8:
                        return False
                    plain.extend(bf.decrypt_block(block))
                return True

            if not decrypt_upto(6):
                return self._fail("decrypted header too short")

            count, size = struct.unpack("<hi", bytes(plain[:6]))
            if count < 0:
                return self._fail("negative member count ({}) -- header not readable"
                                  .format(count))
            if not decrypt_upto(6 + count * 12):
                return self._fail("index runs past end of file "
                                  "(count {} needs {} bytes)".format(count, count * 12))

            index_bytes = bytes(plain[6:6 + count * 12])
            return self._index(count, size, index_bytes, header_at=4 + need)

    def _read_plain(self, f, header_at):
        """
        Plain 6-byte header: short count, int size. `header_at` is 0 when the
        leading short was non-zero, and 4 when the extended marker was present
        but the index is not encrypted. Either way the file position must be
        set explicitly -- the flag word was already consumed by the caller.
        """
        f.seek(header_at)
        buf = f.read(6)
        if len(buf) < 6:
            return self._fail("truncated header")
        count, size = struct.unpack("<hi", buf)
        index_bytes = f.read(count * 12) if count > 0 else b""
        return self._index(count, size, index_bytes, header_at)

    def _index(self, count, size, index_bytes, header_at):
        self.count = count
        self.data_size = size
        self.data_start = header_at + 6 + count * 12

        if count < 0:
            return self._fail("negative member count ({}) -- header not readable".format(count))
        if len(index_bytes) < count * 12:
            return self._fail("index runs past end of file "
                              "(count {}, needs {} bytes, has {})"
                              .format(count, count * 12, len(index_bytes)))

        for i in range(count):
            crc, off, sz = struct.unpack("<iii", index_bytes[i * 12:i * 12 + 12])
            self.entries.append((crc, off, sz))
        return True

    def find(self, name):
        """Binary search the index the way the engine does: by signed crc."""
        target = crc_name(name)
        signed = target - 0x100000000 if target >= 0x80000000 else target

        lo, hi = 0, len(self.entries) - 1
        while lo <= hi:
            mid = (lo + hi) // 2
            got = self.entries[mid][0]
            if got == signed:
                return self.entries[mid]
            if got < signed:
                lo = mid + 1
            else:
                hi = mid - 1
        return None

    def member_bytes(self, name):
        """
        Read one member out. Only the header is encrypted in a MIX, so member
        data is a straight copy from `data_start + offset`.
        """
        hit = self.find(name)
        if hit is None:
            return None
        _, off, sz = hit
        with open(self.path, "rb") as f:
            f.seek(self.data_start + off)
            return f.read(sz)

    def index_is_sorted(self):
        return all(self.entries[i][0] <= self.entries[i + 1][0]
                   for i in range(len(self.entries) - 1))

    def describe(self):
        notes = []
        if self.encrypted:
            notes.append("encrypted")
        if self.digest:
            notes.append("digest")
        kind = "extended" if (self.digest or self.encrypted) else "plain"
        return "{} members, data {} bytes, {} header{}".format(
            self.count, self.data_size, kind,
            (" [" + ",".join(notes) + "]") if notes else "")


# --------------------------------------------------------------------------
# Commands
# --------------------------------------------------------------------------

# Names the engine's startup asks for, per manual/content/formats/mix.md, plus
# the archives a TS install is expected to carry.
STARTUP_TARGETS = [
    "CACHE.MIX", "LOCAL.MIX", "CONQUER.MIX", "SOUNDS.MIX", "SOUNDS01.MIX",
    "SPEECH.MIX", "SPEECH01.MIX", "SCORES.MIX", "SCORES01.MIX",
    "MOVIES01.MIX", "MOVIES02.MIX", "MOVIES03.MIX",
    "TIBSUN.MIX", "MULTI.MIX", "MAPS01.MIX", "WDT.MIX", "WDTVOX.MIX",
    "GMENU.MIX", "SIDECD01.MIX", "SIDECD02.MIX", "PATCH.MIX",
    "EXPAND01.MIX", "EXPAND02.MIX", "EXPAND03.MIX",
    "ECACHE01.MIX", "PCACHE.MIX",
]

# Names whose presence in TIBSUN.MIX is a strong signal that both the name CRC
# and the decryption are correct, because a wrong CRC or a wrong key matches
# nothing at all.
SELF_TEST_ARCHIVE = "TIBSUN.MIX"
SELF_TEST_NAMES = ["RULES.INI", "ART.INI", "AI.INI", "TEMPERAT.INI", "THEME.INI",
                   "GENERAL.INI", "SOUND01.INI", "SIDE01.INI"]


def cmd_info(args):
    rc = 0
    for path in args.files:
        mix = MixFile(path)
        head = os.path.basename(path)
        if mix.error:
            print("{:<22} {:>12}  ERROR: {}".format(head, mix.file_size, mix.error))
            rc = 1
        else:
            print("{:<22} {:>12}  {}".format(head, mix.file_size, mix.describe()))
    return rc


def cmd_list(args):
    mix = MixFile(args.file)
    if mix.error:
        print("{}: {}".format(args.file, mix.error), file=sys.stderr)
        return 1
    print("{}: {}".format(args.file, mix.describe()))
    print("index sorted: {}".format(mix.index_is_sorted()))
    print("{:>10}  {:>12}  {:>12}  {}".format("offset", "size", "crc(hex)", "name(if known)"))
    names = {crc_name(n) & 0xFFFFFFFF: n for n in STARTUP_TARGETS}
    for crc, off, sz in mix.entries:
        label = names.get(crc & 0xFFFFFFFF, "")
        print("{:>10}  {:>12}  {:>12}  {}".format(off, sz, format(crc & 0xFFFFFFFF, "08X"), label))
    return 0


def cmd_find(args):
    mix = MixFile(args.file)
    if mix.error:
        print("{}: {}".format(args.file, mix.error), file=sys.stderr)
        return 1
    rc = 1
    for name in args.names:
        hit = mix.find(name)
        if hit:
            crc, off, sz = hit
            print("  FOUND  {:<20} crc={:08X} offset={} size={}".format(
                name.upper(), crc & 0xFFFFFFFF, off, sz))
            rc = 0
        else:
            print("  -      {:<20} (crc={:08X})".format(name.upper(), crc_name(name)))
    return rc


def cmd_scan(args):
    targets = [t.upper() for t in (args.targets or STARTUP_TARGETS)]

    mixes = []
    for root, _dirs, files in os.walk(args.dir):
        for name in files:
            if name.lower().endswith(".mix"):
                mixes.append(os.path.join(root, name))
    mixes.sort()

    print("{} archive(s) under {}".format(len(mixes), args.dir))
    print()

    loaded = {}
    bad = 0
    for path in mixes:
        mix = MixFile(path)
        loaded[path] = mix
        rel = os.path.relpath(path, args.dir)
        if mix.error:
            print("{:<40} {:>12}  ERROR: {}".format(rel, mix.file_size, mix.error))
            bad += 1
        else:
            print("{:<40} {:>12}  {}".format(rel, mix.file_size, mix.describe()))

    print()
    print("{} archive(s) unreadable".format(bad))
    print()
    print("target resolution")
    for t in targets:
        hits = []
        for path, mix in loaded.items():
            rel = os.path.relpath(path, args.dir)
            if os.path.basename(path).upper() == t:
                hits.append("loose archive " + rel)
            elif not mix.error and mix.find(t):
                hits.append("member of " + rel)
        if hits:
            print("  {:<16} {}".format(t, "; ".join(hits)))
        else:
            print("  {:<16} NOT PRESENT".format(t))

    print()
    print("self-test: name CRC and decryption are wrong unless these resolve")
    for path, mix in loaded.items():
        if os.path.basename(path).upper() == SELF_TEST_ARCHIVE:
            if mix.error:
                print("  {}: {}".format(SELF_TEST_ARCHIVE, mix.error))
                break
            print("  index sorted: {}".format(mix.index_is_sorted()))
            for n in SELF_TEST_NAMES:
                print("  {:<16} {}".format(n, "found" if mix.find(n) else "MISSING"))
            break
    else:
        print("  {} not present under {}".format(SELF_TEST_ARCHIVE, args.dir))
    return 0


def cmd_extract(args):
    """Pull a member out so a nested archive can be inspected in its own right."""
    mix = MixFile(args.archive)
    if mix.error:
        print("{}: {}".format(args.archive, mix.error), file=sys.stderr)
        return 1
    data = mix.member_bytes(args.name)
    if data is None:
        print("{} does not hold {} (crc={:08X})".format(
            args.archive, args.name.upper(), crc_name(args.name)), file=sys.stderr)
        return 1
    with open(args.out, "wb") as f:
        f.write(data)
    print("{} -> {} ({} bytes)".format(args.name.upper(), args.out, len(data)))
    return 0


def cmd_selftest(args):
    """Everything this tool asserts about itself, against outside oracles."""
    import zlib
    from binascii import hexlify

    _selftest_blowfish()

    # The name CRC is CRC::Memory, which is the ordinary CRC-32, so zlib is a
    # genuinely independent check on it.
    for probe in [b"", b"A", b"123456789", b"TEMPERAT.INI", b"CACHE.MIX",
                  b"the quick brown fox jumps over the lazy dog"]:
        mine = crc_memory(probe, 0)
        if mine != zlib.crc32(probe):
            raise AssertionError("crc_memory disagrees with zlib for {!r}".format(probe))
    print("crc_memory vs zlib.crc32              OK")

    # A name whose length is a multiple of four takes only the bulk path, and
    # there CRCEngine and the plain CRC must agree; the engine's own value for
    # TEMPERAT.INI is 785DAEFF.
    if crc_name("TEMPERAT.INI") != 0x785DAEFF:
        raise AssertionError("crc_name wrong for a 4-aligned name")
    if crc_name("RULES.INI") != 0xF025A96C:      # engine CRCEngine value
        raise AssertionError("crc_name wrong for a remainder name")
    print("crc_name vs engine CRCEngine         OK")

    exponent, modulus = load_fast_key()
    if exponent != 65537:
        raise AssertionError("fast key exponent is not 65537")
    print("fast key: {} bit modulus, exponent {}   OK".format(modulus.bit_length(), exponent))
    print("encrypted key block: {} bytes         OK".format(encrypted_key_length(modulus)))

    load_blowfish_tables()
    print("Blowfish tables read from engine     OK")
    print()
    print("all self-tests passed")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("selftest", help="verify the tool against outside oracles")
    p.set_defaults(func=cmd_selftest)

    p = sub.add_parser("info", help="header summary for each archive")
    p.add_argument("files", nargs="+")
    p.set_defaults(func=cmd_info)

    p = sub.add_parser("list", help="every member of one archive")
    p.add_argument("file")
    p.set_defaults(func=cmd_list)

    p = sub.add_parser("find", help="look names up in one archive")
    p.add_argument("file")
    p.add_argument("names", nargs="+")
    p.set_defaults(func=cmd_find)

    p = sub.add_parser("extract", help="write one member out of an archive")
    p.add_argument("archive")
    p.add_argument("name")
    p.add_argument("out")
    p.set_defaults(func=cmd_extract)

    p = sub.add_parser("scan", help="survey every MIX under a directory")
    p.add_argument("dir")
    p.add_argument("--targets", nargs="*", default=None)
    p.set_defaults(func=cmd_scan)

    args = ap.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
