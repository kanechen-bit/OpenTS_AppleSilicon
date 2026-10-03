#pragma once
/* Shim <conio.h> (MSVC console I/O) for the non-Windows experimental OpenTS
 * build. The engine uses getch()/kbhit() for interactive prompts; headless
 * builds treat input as absent. */
#if !defined(OPENTS_EXPERIMENTAL_NONWIN32)
#error "code/port/shim/conio.h may only be included under OPENTS_EXPERIMENTAL_NONWIN32"
#endif

inline int getch(void)  { return -1; }
inline int getche(void) { return -1; }
inline int kbhit(void)  { return 0; }
