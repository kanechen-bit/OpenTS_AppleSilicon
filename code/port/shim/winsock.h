/*******************************************************************************
 *                                O P E N  T S
 ******************************************************************************/
/* Port shim for <winsock.h>: maps the Win32 socket API onto BSD/POSIX sockets
 * so the engine's networking layer compiles and links on macOS/Linux. Under
 * OPENTS_EXPERIMENTAL_NONWIN32 only.
 *
 * Win32 and POSIX share the core socket names (socket/connect/send/recv/bind/
 * listen/accept/select/gethostbyname/inet_addr/htons/ntohs/...), so those are
 * left to resolve to the POSIX declarations directly. We only supply the
 * Win32-specific names (SOCKET, closesocket, WSA* startup, SOCKADDR aliases). */
#ifndef OPENTS_PORT_SHIM_WINSOCK_H
#define OPENTS_PORT_SHIM_WINSOCK_H

#include "windows.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>

typedef int SOCKET;
#define INVALID_SOCKET      ((SOCKET)~0)
#define SOCKET_ERROR        (-1)
#define closesocket(s)      ::close(s)

typedef struct sockaddr   SOCKADDR;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct in_addr    IN_ADDR;
typedef struct in_addr    IN_ADDR_T;
typedef struct hostent    HOSTENT;
typedef struct fd_set     FD_SET;
typedef struct timeval    TIMEVAL;

/* Win32 carries its own startup handshake; POSIX sockets need none. */
typedef struct {
	unsigned short wVersion;
	unsigned short wHighVersion;
	char           szDescription[256];
	char           szSystemStatus[128];
	unsigned short iMaxSockets;
	unsigned short iMaxUdpDg;
	void          *lpVendorInfo;
} WSADATA;

inline int WSAStartup(WORD /*wVersionRequested*/, WSADATA * /*lpWSAData*/) { return 0; }
inline int WSACleanup(void) { return 0; }
inline int WSAGetLastError(void) { return errno; }
inline void WSASetLastError(int /*iError*/) {}

#define WSANOTINITIALISED  0
#define WSAEWOULDBLOCK     EWOULDBLOCK
#define WSAECONNRESET      ECONNRESET
#define WSAEINPROGRESS     EINPROGRESS

/* Win32 select() takes a small struct; alias to POSIX fd_set + timeval. */
#ifndef FD_SETSIZE
#define FD_SETSIZE 64
#endif

#endif /* OPENTS_PORT_SHIM_WINSOCK_H */
