/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2025 Electronic Arts Inc.
 * Copyright 2026 OpenTS contributors
 *
 * Contains material derived from Electronic Arts source code.
 * Modified by OpenTS contributors, 2026.
 * EA's GPLv3 Section 7 additional terms and supplemental warranty
 * disclaimers apply; see LICENSE.md.
 ******************************************************************************/

/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                     $Archive:: /Commando/Library/_xmouse.h                                 $*
 *                                                                                             *
 *                      $Author:: Greg_h                                                      $*
 *                                                                                             *
 *                     $Modtime:: 7/22/97 11:37a                                              $*
 *                                                                                             *
 *                    $Revision:: 1                                                           $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#pragma once

#include "xmouse.h"

extern Mouse * MouseCursor;


inline void Hide_Mouse(void) {MouseCursor->Hide_Mouse();}
inline void Show_Mouse(void) {MouseCursor->Show_Mouse();}
inline void Conditional_Hide_Mouse(Rect rect) {MouseCursor->Conditional_Hide_Mouse(rect);}
inline void Conditional_Show_Mouse(void) {MouseCursor->Conditional_Show_Mouse();}
inline int Get_Mouse_State(void) {return(MouseCursor->Get_Mouse_State());}
inline void Set_Mouse_Cursor(Point2D const & hotspot, ShapeSet const * cursor, int shape) {MouseCursor->Set_Cursor(hotspot, cursor, shape);}
inline int Get_Mouse_X(void) {return(MouseCursor->Get_Mouse_X());}
inline int Get_Mouse_Y(void) {return(MouseCursor->Get_Mouse_Y());}
inline Point2D Get_Mouse_Point(void) {return(MouseCursor->Get_Mouse_Point());}
