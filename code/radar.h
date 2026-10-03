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

/* $Header: /CounterStrike/RADAR.H 1     3/03/97 10:25a Joe_bostic $ */
/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : RADAR.H                                                      *
 *                                                                                             *
 *                   Programmer : Joe L. Bostic                                                *
 *                                                                                             *
 *                   Start Date : 12/15/94                                                     *
 *                                                                                             *
 *                  Last Update : December 15, 1994 [JLB]                                      *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#pragma once

#include "coord.h"
#include "display.h"
#include "hashtable.h"
#include "stimer.h"
#include "timer.h"

#include "bsize.hh"

class DSurface;
class BSurface;
template<class T> class DynamicVectorClass;

typedef DynamicVectorClass<Point2D> FOUNDATION_LIST;

struct RadarTrackingStruct {

	RadarTrackingStruct(TechnoClass * object = NULL, int x = 0, int y = 0) : Object(object), Position(x, y) {}
	RadarTrackingStruct(RadarTrackingStruct const & that) : Object(that.Object), Position(that.Position) {}

	/*
	 * This is the object that appears as a blip at the tracked position. A building covers
	 * several radar pixels, so it is tracked once for every pixel of its radar foundation.
	 */
	TechnoClass * Object;

	/*
	 * This is the radar pixel that the object occupies. The tracking table is hashed on this
	 * alone, so the radar can find whichever object sits on a given pixel.
	 */
	Point2D Position;

	bool operator==(const RadarTrackingStruct & that) const { return(Object == that.Object && Position == that.Position); }
	bool operator!=(const RadarTrackingStruct & that) const { return(Object != that.Object || Position != that.Position); }

	/*
	 * The non-const overloads compare Position only. A lookup key is built with
	 * Object == NULL and relies on these to match on position alone (see Get),
	 * while Add and Remove use the const (Object + Position) overloads above.
	 */
	bool operator==(RadarTrackingStruct & that) { return(Position == that.Position); }
	bool operator!=(RadarTrackingStruct & that) { return(Position != that.Position); }

	int Hash(void) const { return((Position.X - 5 * Position.Y) & 0xFF); }

	bool Use_Head(void) const;

	static int Hash_Old(RadarTrackingStruct const & s);
	static int Hash2(RadarTrackingStruct const & s);
};

typedef HashTableClass<RadarTrackingStruct, TechnoClass *> RADAR_HASH_TABLE;

class RadarClass: public DisplayClass
{
		typedef DisplayClass BASECLASS;
		friend class RadarEventClass;

	public:
		virtual void Serialize(SaveStreamClass & stream) override;

	public:
		RadarClass(void);
		virtual ~RadarClass(void) override;

		/*
		**	The dimensions and coordinates of the radar map.
		*/
		int RadX;
		int RadY;
		int RadWidth;
		int RadHeight;
		int RadOffX;
		int RadOffY;
		int RadIWidth;
		int RadIHeight;
		int RadPWidth;
		int RadPHeight;

		/*
		**	Initialization
		*/
		virtual void One_Time(void) override;   // One-time inits
		virtual void Init_Clear(void) override; // Clears all to known state

		void Post_Load_Radar_Fixup(void);

		virtual bool Map_Cell(Cell const & cell, HouseClass * house) override;
		virtual Cell Click_Cell_Calc(Point2D const & point) const;
		virtual void AI(KeyNumType & input, Point2D const & xy) override;
		virtual void Draw_It(bool complete=false) override;
		virtual void Reposition_Sidebar(void) override;
		virtual void Set_Map_Dimensions(Rect const & size, bool, int cell_height, bool) override;
		virtual void Set_Local_Dimensions(Rect const & size) override;
		virtual void Set_Tactical_Position(Coord const & coord);
		virtual void Init_For_House(void);

		void Radar_Activate(int control);
		void Radar_Background(Cell const & cell);
		Point2D Coord_To_Radar_Pixel(Coord const & coord, bool clip);
		bool Cell_On_Radar(Cell const & cell);
		bool Is_Radar_Active(void);
		bool Is_Radar_Tactical(void);
		bool Is_Radar_Existing(void);

		void Flag_Background_Update(Cell const & cell) {Radar_Background(cell); }

		/*
		**	Toggles player names on & off
		*/
		//void Player_Names(bool on);
		bool Is_Player_Names(void);
		void Draw_Names(void);
		void Noop(int, int);
		void Compute_Radar_Image(void);
		Rect Compute_Background(Rect const & cell_rect, Rect & update_rect, bool fill_in);
		void Fill_In_Background(Rect const & cell_rect, Rect const & update_rect);
		Rect Cell_Radar_Rect(Cell const & cell);
		Rect Cell_To_Radar_Pixel(Cell const & cell);
		Cell Radar_Pixel_To_Cell(Point2D const & point);
		void Plot_Radar_Background(void);
		void Radar_Track(TechnoClass * techno, Point2D point);
		void Radar_Untrack(TechnoClass * techno, Point2D point);
		void Init_Radar(void);
		void Reset_Radar(void);
		void Clear_Radar(void);
		void Plot_Radar_Pixel(Point2D const & point);
		void Render_Tracked_Objects(void);
		void Radar_Pixel(Point2D const & point);

		void Compute_Foundations(void);
		FOUNDATION_LIST const & Get_Foundation(BuildingTypeClass * btype);

		void Radar_Cell(Cell const & cell);
		void Resolve_Radar_Point(Point2D const & point, Cell & cell, ObjectClass *& object);
		void Set_Radar_State(bool on);

		void Toggle_Radar(bool on);
		bool Is_Playing_Movie(void);
		void Redraw_Radar(bool complete);

		void Render_Radar(void);
		void Play_Movie(void);
		void Complete_Radar_Refresh(void);

		void Queue_Next_Movie(void) {RadarState = RSTATE_NEXT_MOVIE; RadarAnimFrame = RADAR_ACTIVATED_FRAME;}

		void Clear_Background_Stack(void) { BackgroundStack.Clear(); }

		void Set_Radar_Scale(int scale) { RadarScale = scale; }

	public:

		/*
		**	Radar map constant values.
		*/
		enum RadarClassEnums {
			RADAR_ACTIVATED_FRAME = 25,
			MAX_RADAR_FRAMES = 40
		};

		/*
		 * This is the region of the sidebar that the radar has drawn into and that still
		 * needs copying to the visible page. It is RECT_NONE when nothing is outstanding.
		 */
		Rect LastDrawRect;

		/*
		 * This is the composed radar picture -- the terrain background with the object blips
		 * and the view outline drawn over it. Its dirty portion is blitted to the sidebar.
		 */
		DSurface * RadarSurface;

	protected:

		/*
		 * This is the scaled down picture of the terrain alone, with no objects drawn over
		 * it. The radar surface begins as a copy, and a vacated pixel is redrawn from it.
		 */
		BSurface * BackgroundSurface;

		/*
		 * These are the cells whose radar background color has gone stale. The recoloring is
		 * put off until the radar renders, so a burst of terrain changes costs one resample.
		 */
		DynamicVectorClass<Cell> BackgroundStack;

		/*
		 * This is the unscaled radar picture -- the low and high terrain colors of every
		 * visible cell, two entries wide per cell. The radar background is resampled from
		 * here, so a cell that changes appearance need only have its two colors rewritten.
		 */
		RGBClass * BackgroundColors;

		/*
		**	This gadget class is used for capturing input to the tactical map. All mouse input
		**	will be routed through this gadget.
		*/
		class RTacticalClass : public GadgetClass {
			public:
				RTacticalClass(void) : GadgetClass(0,0,0,0,LEFTPRESS|LEFTRELEASE|LEFTHELD|LEFTUP|RIGHTPRESS|RIGHTUP,true) {};

			protected:
				virtual int Action(unsigned flags, KeyNumType & key) override;
			friend class RadarClass;
		};
		friend class RTacticalClass;

		/*
		**	This is the "button" that tracks all input to the tactical map.
		**	It must be available to derived classes, for Save/Load purposes.
		*/
		static RTacticalClass RadarButton;

	private:

		/*
		**	The width and height is controlled by the actual dimensions
		**	of the radar map display box (in pixels).
		*/
		int RadarCellWidth;
		int RadarCellHeight;
		Rect CellRedrawRect;

		/*
		 * This is the table of objects that show up as blips on the radar, keyed by the radar
		 * pixel each one occupies. It lets the radar draw its blips without walking every
		 * object in the game, and lets a click on the radar find the object underneath.
		 */
		RADAR_HASH_TABLE * RadarTrackingTable;

		/*
		**	This is the list of radar pixels that need to be updated. Only a partial
		**	list is maintained for maximum speed.
		*/
		DynamicVectorClass<Point2D> PixelStack;
		unsigned char * PixelFlags;

		/*
		 * These are the radar pixels covered by a building of each of the standard building
		 * sizes, measured relative to the building's own radar position. Precomputing the
		 * footprints spares the radar from deriving them again for every building it plots.
		 */
		FOUNDATION_LIST Foundation[BSIZE_COUNT];

		/*
		**	This is the zoom factor to use. This value is the number of pixels wide
		**	each cell will occupy on the radar map. Completely zoomed out would be a
		**	value of 1.
		*/
		float ZoomFactor;

		/*
		**	The current radar position as the upper left corner cell for the
		**	radar map display.
		*/
		int RadarScale;
		int RadarX;

		/// Unused
		int field_149C;

		/*
		 * This is the row origin of the radar picture -- the smallest sum of X and Y among the
		 * cells of the local map. Subtracting it puts that corner on the radar's first row.
		 */
		int RadarY;

		/*
		**	This is the origin (pixel offsets) for the upper left corner
		**	of the radar map within the full radar map area of the screen.
		**	This is biased so that the radar map, when smaller than full
		**	size will appear centered.
		*/
		Rect RadarRect;

		enum RadarStateType {
			RSTATE_INACTIVE,
			RSTATE_ACTIVE,
			RSTATE_DEACTIVATING,
			RSTATE_ACTIVATING,
			RSTATE_NEXT_MOVIE,
			RSTATE_MOVIE_DONE,
		};

		/*
		 * This is how far along the radar display is in its opening or closing sequence. The
		 * radar only shows its contents while active; the intermediate states carry the
		 * activation animation forward a frame at a time. See RadarStateType for the choices.
		 */
		int RadarState;

		enum RadarModeType {
			RMODE_UNAVAILABLE,
			RMODE_TACTICAL,
			RMODE_PLAYER_NAMES,
			RMODE_MOVIE,
			RMODE_4,
			RMODE_5,
		};

		/*
		 * This is what the radar pane is presently showing -- the radar map itself, the list
		 * of player names, or a movie. See RadarModeType for the choices.
		 */
		int RadarMode;

		/*
		 * This is the mode that was interrupted in order to play a movie in the radar pane.
		 * The radar returns to it when the movie ends, so the interruption leaves no trace.
		 */
		int SuspendedRadarMode;

		/*
		**	If the radar map is visible then this flag is true.
		*/
		bool DoesRadarExist;

		/*
		**	If the radar map must be completely redrawn, then this flag will be true.
		**	Typical causes of this would be when the radar first appears, or when the
		**	screen has been damaged.
		*/
		bool IsToRedraw;
		bool FullRedraw;

		/*
		 * This is the outline drawn on the radar to show which part of the map the tactical
		 * view is looking at. It is recomputed from the center of the tactical view every
		 * time the radar renders and is clipped to stay within the radar picture.
		 */
		Rect RadarViewRect;

		/*
		 * This records where that outline was drawn on the previous render. When it no longer
		 * agrees with the current one, the pixels beneath the old outline are flagged for
		 * redraw so that the moving box does not leave a trail behind it.
		 */
		Rect OldRadarViewRect;

		/*
		 * This is the frame of the radar animation currently displayed. It counts up while
		 * the radar is opening and back down while it is closing, so that the radar appears
		 * to unfold and fold away rather than snapping between states.
		 */
		int RadarAnimFrame;

		/*
		 * This is the delay (expressed in system timer ticks) between frames of that
		 * animation. The animation only advances when this timer expires, which paces the
		 * radar opening and closing independently of the game frame rate.
		 */
		CDTimerClass<SystemTimerClass> RadarAnimTimer;

		static void const * RadarAnim;
};
