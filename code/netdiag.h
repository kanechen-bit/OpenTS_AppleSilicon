/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

/*
**	LAN transport diagnostics.
**
**	macOS gates the local network per application: a send can be refused outright
**	(EPERM/EACCES from Local Network privacy) or packets can simply never arrive
**	(the application firewall dropping incoming traffic, or a silent privacy
**	drop). Both leave a LAN lobby looking empty with nothing reported anywhere,
**	so the transport counts what actually happened and the lobby can tell the
**	player what to go and fix.
**
**	The counters are advisory only; nothing in the transport's behaviour depends
**	on them, and a platform whose backend bumps none of them (the null backend,
**	the Windows backend) simply reports zeros.
*/

#ifndef NETDIAG_H
#define NETDIAG_H

namespace LanDiag {

/** Datagrams the kernel accepted from us. */
extern unsigned long Send_Ok;

/** Sends refused by macOS Local Network privacy (EPERM/EACCES). */
extern unsigned long Send_Blocked;

/** Sends that failed for any other reason. */
extern unsigned long Send_Errors;

/** Datagrams read off the socket, before any filtering. */
extern unsigned long Recv_Raw;

/** Received datagrams dropped because they came from one of our own addresses. */
extern unsigned long Recv_Dropped_Self;

/** Errno of the most recent failed send, or 0. */
extern int Last_Send_Errno;

/** Clears every counter; called when a LAN transport is configured. */
void Reset(void);

}

#endif
