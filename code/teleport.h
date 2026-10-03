/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#pragma once

#include "loco.h"


class TeleportLocomotionClass : public LocomotionClass
{
		typedef LocomotionClass BASECLASS;

	public:
		TeleportLocomotionClass(void);

		virtual HRESULT STDMETHODCALLTYPE GetClassID(CLSID * retval) override;

		virtual void Serialize(SaveStreamClass & stream) override;

		virtual boolean STDMETHODCALLTYPE Is_Moving(void) override;
		virtual Coord STDMETHODCALLTYPE Destination(void) override;
		virtual void STDMETHODCALLTYPE Move_To(Coord to) override;
		virtual void STDMETHODCALLTYPE Stop_Moving(void) override;
		virtual boolean STDMETHODCALLTYPE Process(void) override;
		virtual LayerType STDMETHODCALLTYPE In_Which_Layer(void) override;

		virtual boolean Is_Stationary(void);

	private:
		/*
		 * This is the coordinate that the object will be set down at the next time this
		 * locomotor is processed. If COORD_NONE, then no teleport is pending -- there is
		 * no separate moving flag, so this doubles as one.
		 */
		Coord DestinationCoord;
};
