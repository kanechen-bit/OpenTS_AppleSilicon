/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#pragma once

#include "nativewindow.hh"


// How the presented frame is filtered when the window is larger than it.
enum VideoScaleMode {
	VIDEO_SCALE_NEAREST,
	VIDEO_SCALE_LINEAR,
	VIDEO_SCALE_PIXELART,
	VIDEO_SCALE_SUPERSAMPLE,
};


// Where the game's frame lands inside the window. The frame keeps its aspect ratio, so
// the destination is centered and the window may show bars on two of its sides.
// Drawable dimensions and the destination rectangle are measured in physical pixels.
struct VideoScaleInfo
{
	int GameWidth;
	int GameHeight;
	int DrawableWidth;
	int DrawableHeight;
	int DestX;
	int DestY;
	int DestWidth;
	int DestHeight;
	float ScaleX;
	float ScaleY;
};


bool Video_Init(NativeWindow const & window, int drawablewidth, int drawableheight, int refreshrate);
void Video_Shutdown(void);

bool Video_Set_Mode(int width, int height);
void Video_On_Resize(int drawablewidth, int drawableheight);
void Video_Set_Refresh_Rate(int refreshrate);

void Video_Mark_Dirty(void);
void Video_Present(void);
/*
 * Puts the visible surface on screen even while a composite hold is on. The
 * hold exists to stop a half-built composite reaching the screen; an animation
 * that walks a frame at a time is asking to show exactly those states, so it
 * presents through the hold. See the dialog reveal.
 */
void Video_Present_Force(void);
void Video_Present_If_Dirty(void);

// Suspends presentation while a composite is being assembled, so a present
// cannot catch it half drawn. Release and present once the composite is whole.
void Video_Hold_Present(bool hold);

VideoScaleInfo const & Video_Get_Scale_Info(void);

int * EnumDisplayModes(int minwidth, int minheight, int maxwidth, int maxheight);
