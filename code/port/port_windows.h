/*
**  port_windows.h -- a logical Win32 window manager for the port.
**
**  The engine's dialog layer (code/ownrdraw.cpp) is a complete
**  owner-draw control system that paints into its own surfaces. It needs
**  real HWNDs only as *handles*: opaque tokens that carry an id, a class
**  name, a geometry, per-window long words, and a message procedure it
**  can subclass. Nothing is ever composited by the OS -- the engine
**  blits the results itself. So rather than map the dialog layer onto
**  native Cocoa windows, this unit implements the window *semantics*
**  directly: a tree of logical windows with the parts of CreateWindow /
**  CreateDialog / SendMessage the engine drives.
**
**  Two window kinds coexist:
**
**    * native  -- the main game window created by CreateWindowEx against
**      the class the engine registered. It is backed by a real Cocoa
**      window (portwindow/port_window.mm) and is *not* owned here; this
**      unit only knows its handle so IsWindow/GetParent answer.
**    * logical -- dialog frames and their child controls, created by
**      CreateDialogIndirectParam out of a DLGTEMPLATE. Entirely owned
**      and stored here.
**
**  Coordinate spaces: a logical window's geometry is kept in *client
**  pixels relative to its parent*, which is what SetWindowPos/MoveWindow
**  receive on Win32 and what the engine's dialog-unit conversion
**  produces. GetWindowRect answers in parent-relative pixels too --
**  there is no on-screen position to convert to, and the one consumer
**  that converts (Convert_Coordinate in wwmouse.cpp) only ever walks
**  the main window, which is native.
**
**  C linkage, engine-independent: the engine calls this only through
**  the shim in port/windows_stub.h.
*/
#ifndef OPENTS_PORT_WINDOWS_H
#define OPENTS_PORT_WINDOWS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque window token. HWND is void* in the shim; this mirrors it. */
typedef void *OpentsWin;

/*
**  Creation. Class name is a real string ("Button", "Static", the
**  engine's registered class, ...). Creation data is the per-item blob
**  from a DLGTEMPLATE (may be null/0).
*/
OpentsWin opents_win_create(char const *class_name, char const *text,
                            unsigned long style, unsigned long ex_style,
                            int x, int y, int width, int height,
                            OpentsWin parent, unsigned id,
                            void const *creation_data, unsigned creation_size);

void opents_win_destroy(OpentsWin window);

/* Dialog creation out of a classic DLGTEMPLATE (never the EX form --
** Language.dll carries only classic ones; verified). Returns the frame
** window with Dlg_Proc stored, and sends WM_INITDIALOG. */
OpentsWin opents_win_create_dialog(void const *template_bytes, unsigned template_size,
                                   OpentsWin owner, void *dlg_proc);

/* Records the native main window so IsWindow/GetParent answer for it. */
void opents_win_set_main_window(OpentsWin native_hwnd);
OpentsWin opents_win_main_window(void);

/* Records the class name the engine registered, so GetClassName answers it
** for the native window -- the engine tests that name to tell its own
** top-level window from a control. */
void opents_win_register_class_name(char const *name);
int  opents_win_class_matches_main(char const *name);

/* RegisterClass / the procedure a window's class was registered with.
** Win32 keeps the procedure with the class, not in one global, so a window
** of one class never receives the procedure of another. */
void  opents_win_register_class(char const *name, void *proc);
void *opents_win_proc_for(OpentsWin window);

/* Recognising the native window, and the class name it was registered
** with. It is not in the logical tree, so the shim tests for it before
** routing a query there. */
int opents_win_is_main(OpentsWin window);
int opents_win_get_main_class(char *buffer, unsigned size);

/* Tree queries. */
OpentsWin   opents_win_get_parent(OpentsWin window);
OpentsWin   opents_win_find_child(OpentsWin window, unsigned id);
int         opents_win_get_class_name(OpentsWin window, char *buffer, unsigned size);
int         opents_win_is_kind_of(OpentsWin window, char const *class_name); /* case-insensitive */
OpentsWin   opents_win_get_window(OpentsWin window, unsigned relationship);  /* GW_HWNDNEXT etc */

/* Per-window long words. Index is one of the GWL_ or GWLP_ slots; both the
** value and the value it replaces are uintptr_t-wide. */
uintptr_t opents_win_get_long(OpentsWin window, int index);
uintptr_t opents_win_set_long(OpentsWin window, int index, uintptr_t value);

/* Geometry, in parent-relative client pixels. */
void opents_win_get_rect(OpentsWin window, int *x, int *y, int *w, int *h);
void opents_win_set_rect(OpentsWin window, int x, int y, int w, int h);
void opents_win_get_client_size(OpentsWin window, int *w, int *h);

/* State. */
int   opents_win_is_valid(OpentsWin window);
int   opents_win_is_visible(OpentsWin window);
void  opents_win_set_visible(OpentsWin window, int visible);
int   opents_win_is_enabled(OpentsWin window);
void  opents_win_set_enabled(OpentsWin window, int enabled);

/* The update region, in the window's own client coordinates. Showing a
** window marks it dirty and delivers its WM_PAINT; validating clears it. */
int  opents_win_get_update(OpentsWin window, int *x, int *y, int *w, int *h);
void opents_win_validate(OpentsWin window);

/* Re-deliver WM_PAINT to a window and all of its visible children, so the
** engine's owner-draw layer redraws fresh content into AlternateSurface.
** Used by the dialog compositor to keep hover/selection state live. */
void opents_win_repaint_tree(OpentsWin window);

/* Text. Returns the length copied (no NUL counted), Win32-style. */
int opents_win_get_text(OpentsWin window, char *buffer, unsigned size);
void opents_win_set_text(OpentsWin window, char const *text);

/* GetWindowTextLength: characters held, excluding the terminator. */
int opents_win_get_text_length(OpentsWin window);

/* Messaging. Send invokes the window's current procedure synchronously;
** Post enqueues through the shared port queue. */
intptr_t opents_win_send(OpentsWin window, unsigned message, uintptr_t wparam, intptr_t lparam);
void     opents_win_post(OpentsWin window, unsigned message, uintptr_t wparam, intptr_t lparam);

/* The class's own procedure, in a form the engine can store and call
** back through CallWindowProc. GWL_WNDPROC answers this for any window
** that has not been subclassed, so an owner-draw control reaches the
** built-in default for the messages it does not handle itself. */
intptr_t opents_win_def_proc(void *hwnd, unsigned message, uintptr_t wparam, intptr_t lparam);

/* Enumeration in z-order. The callback returns 0 to stop. */
typedef int (*OpentsEnumProc)(OpentsWin window, intptr_t lparam);
int opents_win_enum_children(OpentsWin parent, OpentsEnumProc proc, intptr_t lparam);

/* Dialog helpers. */
void     opents_win_end_dialog(OpentsWin dialog, intptr_t result);
intptr_t opents_win_dialog_result(OpentsWin dialog);
OpentsWin opents_win_next_tab_item(OpentsWin dialog, OpentsWin from, int previous, int search_group);


/* Focus tracking (logical windows only; the native window is its own). */
OpentsWin opents_win_set_focus(OpentsWin window);
OpentsWin opents_win_get_focus(void);

/* Keyboard translation for modeless dialogs: mirrors IsDialogMessage
** for the keys a dialog consumes. Returns 1 if the message was handled
** (and must not reach the normal procedure). */
int opents_win_is_dialog_message(OpentsWin dialog, unsigned message,
                                 uintptr_t *wparam, intptr_t *lparam);

/*
**  Unit conversion. DLGTEMPLATE units are converted with the classic
**  MS Sans Serif 8pt base (4x8 units = 6x13 pixels). Dialog 198, the
**  engine's 200x100-unit measurement probe, therefore measures 300x163
**  pixels -- which is the fixed size the engine's Resize_Dialog math
**  assumes.
*/
int opents_dialog_units_x(int units);
int opents_dialog_units_y(int units);

#ifdef __cplusplus
}
#endif

#endif /* OPENTS_PORT_WINDOWS_H */
