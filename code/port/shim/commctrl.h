#pragma once
/* Shim <commctrl.h> (Win32 common controls) for the non-Windows experimental
 * OpenTS build. Only the message/constant symbols the engine references are
 * provided; the control classes themselves are not implemented (the headless
 * core does not instantiate them). */
#if !defined(OPENTS_EXPERIMENTAL_NONWIN32)
#error "code/port/shim/commctrl.h may only be included under OPENTS_EXPERIMENTAL_NONWIN32"
#endif
#include "windows_stub.h"

/* Trackbar (slider) messages. Values match Win32. */
#define TBM_GETPOS              0x0400
#define TBM_GETRANGEMIN         0x0401
#define TBM_GETRANGEMAX         0x0402
#define TBM_GETTIC              0x0403
#define TBM_SETTIC              0x0404
#define TBM_SETRANGE            0x0405
#define TBM_SETPOS              0x0406
#define TBM_SETRANGEMIN         0x0407
#define TBM_SETRANGEMAX         0x0408
#define TBM_CLEARTICS           0x0409
#define TBM_SETSEL              0x040A
#define TBM_SETSELEND           0x040B
#define TBM_SETSELSTART         0x040C
#define TBM_GETSELSTART         0x040D
#define TBM_GETSELEND           0x040E
#define TBM_CLEARSEL            0x040F
#define TBM_SETTICFREQ          0x0410
#define TBM_SETPAGESIZE         0x0411
#define TBM_GETPAGESIZE         0x0412
#define TBM_SETLINESIZE         0x0413
#define TBM_GETLINESIZE         0x0414
#define TBM_GETTHUMBRECT        0x0415
#define TBM_GETCHANNELRECT      0x0416
#define TBM_SETTHUMBLENGTH      0x0417
#define TBM_GETTHUMBLENGTH      0x0418
#define TBM_GETNUMTICS          0x0419
#define TBM_GETTICPOS           0x041A
#define TBM_GETPTICS            0x041B
#define TBM_GETTOOLTIPS         0x041C
#define TBM_SETTOOLTIPS         0x041D
#define TBM_GETBUDDY            0x041E
#define TBM_SETBUDDY            0x041F

/* Common control class name constants (kept as opaque strings; not used to
 * create real windows in the headless build). */
#define WC_BUTTONA              "Button"
#define WC_LISTBOXA             "ListBox"
#define WC_COMBOBOXA            "ComboBox"
#define WC_EDITA                "Edit"
#define WC_STATICA              "Static"
#define WC_LISTVIEWA            "SysListView32"
#define WC_TREEVIEWA            "SysTreeView32"
#define WC_TABCONTROLA          "SysTabControl32"
#define WC_TRACKBARA            "msctls_trackbar32"
