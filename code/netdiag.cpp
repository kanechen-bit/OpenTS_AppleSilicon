/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "always.h"

#include "netdiag.h"

namespace LanDiag {

unsigned long Send_Ok = 0;
unsigned long Send_Blocked = 0;
unsigned long Send_Errors = 0;
unsigned long Recv_Raw = 0;
unsigned long Recv_Dropped_Self = 0;
int Last_Send_Errno = 0;

void Reset(void)
{
	Send_Ok = 0;
	Send_Blocked = 0;
	Send_Errors = 0;
	Recv_Raw = 0;
	Recv_Dropped_Self = 0;
	Last_Send_Errno = 0;
}

}
