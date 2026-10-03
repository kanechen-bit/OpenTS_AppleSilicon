/*
 * com_stub.h -- COM-type replacement for non-Windows OpenTS builds.
 *
 * OpenTS was written against the Windows COM headers (<comdef.h>,
 * <unknwn.h>, <objidl.h>). On Apple Silicon / macOS those headers do not
 * exist. This header supplies ONLY the symbols the engine actually
 * references, re-expressed in portable C++ so the COM-using translation
 * units parse without the Component Object Model.
 *
 * It is included exclusively under OPENTS_EXPERIMENTAL_NONWIN32 (see the
 * #include sites in the COM-using headers). The Win32 build is byte-for-byte
 * unchanged: those headers keep #include <comdef.h> and never reach here.
 *
 * Design notes
 * ------------
 * - IUnknown becomes a plain abstract base with the three COM methods as pure
 *   virtuals. AddRef/Release are vestigial in the engine (abstract.h:92).
 * - MIDL_INTERFACE("guid") X : public IUnknown  ->  class X : public IUnknown.
 *   The GUID string is dropped; __uuidof is reduced to a no-op producing an
 *   empty GUID so the smart-pointer typedefs still parse.
 * - _COM_SMARTPTR_TYPEDEF(X, __uuidof(X)) -> typedef _opents_com_ptr<X> X##Ptr.
 *   The engine uses the alias as a smart pointer: operator->, operator&,
 *   bool-ish tests, cross-interface construction (IStreamPtr from
 *   ILinkStreamPtr) and CreateInstance(). _opents_com_ptr provides exactly
 *   that surface; CreateInstance() is a hollow E_NOTIMPL stub.
 * - HRESULT / LONG / BOOL etc. keep their Win32 widths so the ABI assumptions
 *   in serialization code stay correct on the 64-bit target.
 *
 * RUNTIME STATUS: the Ole / Co entry points below are hollow stubs
 * (return E_NOTIMPL / S_OK). They let the call sites parse and link. The Stg*
 * persistence entry points (StgCreateDocfile / StgOpenStorage / CoFileTimeNow)
 * ARE implemented below as a self-consistent, non-OLE structured-storage
 * container so save/load round-trips inside this build -- see OpentsStorage /
 * OpentsStream. Cross-platform save exchange (the pointer-width break in
 * savestream.h) remains tracked separately as Piece C.
 */

#ifndef OPENTS_PORT_COM_STUB_H
#define OPENTS_PORT_COM_STUB_H

#if !defined(OPENTS_EXPERIMENTAL_NONWIN32)
#error "com_stub.h must only be included on the non-Windows experimental build"
#endif

/* ---- Primitive COM types (Win32 widths, NOT host widths) ----------------
 *
 * `long` is 8 bytes on LP64 (macOS/Linux) but Win32's LONG/ULONG/DWORD are all
 * 4. The difference is not cosmetic, because the engine puns these types
 * against 32-bit objects. The decisive case: RawFileClass::Read keeps its byte
 * counter in an `int` and calls
 *
 *     ReadFile(Handle, buffer, size, &(DWORD &)bytesread, NULL);
 *
 * With DWORD 8 bytes wide the shim stored a 64-bit count through a pointer
 * aliasing a 4-byte int. Clang treats that pun as undefined and folds
 * `bytesread` back to its initial 0, so the read loop broke on its first
 * iteration: RawFileClass::Read returned 0 for EVERY file while still filling
 * the caller's buffer.
 *
 * Plain mixfiles hid it -- their callers ignore the return value -- but PKStraw
 * is explicit (`if (got != Encrypted_Key_Length()) return(0);`), so the RSA key
 * block was never accepted, no Blowfish key was derived, and every ENCRYPTED
 * mixfile header decoded as garbage (Count in the millions) and aborted
 * Bootstrap. Packed layouts such as RECT/POINT/SIZE are also built from LONG,
 * so they were twice their on-disk size too. */
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "port_trace.h"

typedef int32_t       HRESULT;
typedef int32_t       LONG;
typedef uint32_t      ULONG;
typedef int           BOOL;
typedef short          VARIANT_BOOL;
typedef uint32_t      DWORD;
typedef unsigned short WORD;
typedef unsigned char BYTE;
typedef short         SHORT;
typedef void *        LPVOID;
typedef size_t        SIZE_T;
typedef bool          boolean;   /* wtypes.h boolean; not standard C++ */
typedef long long     LONGLONG;
typedef unsigned long long ULONGLONG;

/* INT / UINT: windef.h types. Declared here (the base stub, included before
 * windows_stub.h) so both the COM and Win32 surfaces see them. Guarded so a
 * later translation-unit definition is not clobbered. */
#ifndef INT
typedef int  INT;
#endif
#ifndef UINT
typedef unsigned int UINT;
#endif

#define EXTERN_C extern "C"

/* ---- GUID / CLSID / IID ------------------------------------------------ */
/* NOTE: the tag MUST be `_GUID` (as in Windows guiddef.h). Some engine TUs
 * write the out-of-line form `... (struct _GUID const &guid, ...)`; if our tag
 * were `GUID` then `struct _GUID` would denote a *different* incomplete type
 * and the out-of-line definition would not match the IUnknown declaration. */
typedef struct _GUID {
	unsigned long  Data1;
	unsigned short Data2;
	unsigned short Data3;
	unsigned char  Data4[8];
} GUID;

typedef GUID CLSID;
typedef GUID IID;

/* Generated MIDL _i.c files guard their own IID/CLSID type definitions behind
 * __IID_DEFINED__ / CLSID_DEFINED. Define these so those guarded typedefs are
 * skipped (matching Windows guiddef.h), avoiding a redefinition clash. */
#ifndef __IID_DEFINED__
#define __IID_DEFINED__
#endif
#ifndef CLSID_DEFINED
#define CLSID_DEFINED
#endif

typedef const GUID &  REFIID;
typedef const CLSID & REFCLSID;
typedef const GUID &  REFGUID;

inline bool operator==(const GUID &a, const GUID &b) {
	if (a.Data1 != b.Data1) return false;
	if (a.Data2 != b.Data2) return false;
	if (a.Data3 != b.Data3) return false;
	for (int i = 0; i < 8; ++i) if (a.Data4[i] != b.Data4[i]) return false;
	return true;
}
inline bool operator!=(const GUID &a, const GUID &b) { return !(a == b); }

/* ---- Character / string types used by storage APIs --------------------- */
typedef wchar_t OLECHAR;
typedef wchar_t WCHAR;
typedef OLECHAR *LPOLESTR;
typedef const OLECHAR *LPCOLESTR;

/* ---- Structured-storage supporting types ------------------------------- */
/* Mirror Windows' union layout so both QuadPart and the LowPart/HighPart pair
 * are addressable (mpu.cpp reads .LowPart/.HighPart). -fms-extensions lets the
 * anonymous struct members surface as li.LowPart. */
typedef union LARGE_INTEGER {
	struct { DWORD LowPart; LONG HighPart; };
	LONGLONG QuadPart;
} LARGE_INTEGER;

typedef union ULARGE_INTEGER {
	struct { DWORD LowPart; DWORD HighPart; };
	ULONGLONG QuadPart;
} ULARGE_INTEGER;

typedef struct FILETIME {
	DWORD dwLowDateTime;
	DWORD dwHighDateTime;
} FILETIME;

typedef struct STATSTG {
	WCHAR    *pwcsName;
	DWORD     type;
	ULARGE_INTEGER cbSize;
	FILETIME  mtime;
	FILETIME  ctime;
	FILETIME  atime;
	GUID     *clsid;
	DWORD     grfMode;
	DWORD     grfLocksSupported;
} STATSTG;

/* Property-storage identifiers and set metadata (savever.cpp persistence). */
typedef DWORD PROPID;                 /* property id within a set           */
#define PID_FIRST_USABLE 2            /* first caller-assignable property id */

typedef struct STATPROPSETSTG {
	GUID     fmtid;
	GUID     clsid;
	DWORD    grfFlags;
	FILETIME mtime;
	FILETIME ctime;
	FILETIME atime;
} STATPROPSETSTG;

typedef unsigned short VARTYPE;
typedef struct PROPVARIANT {
	VARTYPE vt;
	union {
		LPOLESTR      pwszVal;
		LONG          lVal;
		ULONG         ulVal;
		INT           intVal;
		UINT          uintVal;
		VARIANT_BOOL  boolVal;
		void         *pvVal;
		FILETIME      filetime;
	};
} PROPVARIANT;

/* PROPSPEC is needed by savever.cpp, which includes com_stub.h directly (not
 * windows_stub.h), so it is defined here in full. Windows' layout: an enum
 * kind plus a union of propid / lpwstr. */
#define PRSPEC_LPWSTR 0
#define PRSPEC_PROPID 1
typedef struct PROPSPEC {
	DWORD ulKind;
	union {
		PROPID   propid;
		LPOLESTR lpwstr;
	};
} PROPSPEC;

/* ---- Interface / method macros ----------------------------------------- */
#define interface      struct
#define MIDL_INTERFACE(x) class
#define STDMETHODCALLTYPE
#define STDAPICALLTYPE
#define WINAPI
#define __stdcall
#define __cdecl
#define STDMETHODIMP    HRESULT STDMETHODCALLTYPE
#define STDMETHODIMP_(x) x STDMETHODCALLTYPE
/* STDMETHOD(method) declares a virtual COM method (used by some engine
 * interfaces that spell it out instead of using MIDL_INTERFACE's body). */
#define STDMETHOD(method)   virtual HRESULT STDMETHODCALLTYPE method
/* STDMETHOD_(type, method) is the variant that spells out the return type
 * (classfactory.h uses it for AddRef/Release returning ULONG). */
#define STDMETHOD_(type, method) virtual type STDMETHODCALLTYPE method

/* ---- HRESULT success / failure codes ----------------------------------- */
#define S_OK                     ((HRESULT)0x00000000L)
#define S_FALSE                  ((HRESULT)0x00000001L)
#define VARIANT_FALSE            ((VARIANT_BOOL)0)
#define VARIANT_TRUE             ((VARIANT_BOOL)-1)
#define E_NOINTERFACE            ((HRESULT)0x80004002L)
#define E_NOTIMPL                ((HRESULT)0x80004001L)
#define E_FAIL                   ((HRESULT)0x80004005L)
#define E_POINTER                ((HRESULT)0x80004003L)
#define E_OUTOFMEMORY            ((HRESULT)0x8007000EL)
#define E_INVALIDARG             ((HRESULT)0x80070057L)
#define CLASS_E_NOAGGREGATION    ((HRESULT)0x80040110L)
#define CLASS_E_CLASSNOTAVAILABLE ((HRESULT)0x80040111L)

#ifndef SUCCEEDED
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#endif
#ifndef FAILED
#define FAILED(hr)    (((HRESULT)(hr)) < 0)
#endif

/* ---- CLSCTX / REGCLS / STGM / PROP flags ------------------------------- */
#define CLSCTX_INPROC_SERVER   0x1
#define CLSCTX_INPROC_HANDLER  0x2
#define CLSCTX_LOCAL_SERVER    0x4
#define CLSCTX_INPROC          (CLSCTX_INPROC_SERVER)
#define CLSCTX_ALL             (CLSCTX_INPROC_SERVER | CLSCTX_INPROC_HANDLER | CLSCTX_LOCAL_SERVER)
#define REGCLS_MULTIPLEUSE     0x1
#define STGM_SHARE_EXCLUSIVE   0x10
#define STGM_READWRITE         0x2
#define STGM_CREATE            0x1000
#define STGM_READ              0x0
#define STGM_SHARE_DENY_WRITE  0x20
#define PROPSETFLAG_DEFAULT    0x0
#define VT_FILETIME            0x0040
#define VT_EMPTY               0x0000
#define VT_NULL                0x0001
#define VT_I2                  0x0002
#define VT_I4                  0x0003
#define VT_R4                  0x0004
#define VT_R8                  0x0005
#define VT_CY                  0x0006
#define VT_DATE                0x0007
#define VT_BSTR                0x0008
#define VT_BOOL                0x000B
#define VT_VARIANT             0x000C
#define VT_UI1                 0x0011
#define VT_UI2                 0x0012
#define VT_UI4                 0x0013
#define VT_LPSTR               0x001E
#define VT_LPWSTR              0x001F
#define VT_INT                 0x0016
#define VT_UINT                0x0017

/* ---- Standard COM interface identifiers ---------------------------------
 *
 * On the supported target these come from uuid.lib, which is why
 * code/CMakeLists.txt marks the MIDL-generated *_i.c files HEADER_FILE_ONLY:
 * those files define the engine's own interfaces, while the standard ones are
 * simply expected to be supplied by the platform. There is no uuid.lib here,
 * so the well-known values are defined outright.
 *
 * The values are load-bearing, not just link filler. QueryInterface and
 * IsEqualGUID compare them, so a placeholder GUID would compile and link and
 * then make every request for IStream or IPersistStream fail at run time --
 * which is exactly the kind of silent breakage a stub is supposed to avoid.
 *
 * inline rather than a plain definition: this header is force-included into
 * every translation unit, so an ordinary external definition would be emitted
 * once per TU and collide at link. A vague-linkage definition is merged. */
EXTERN_C inline const IID IID_IUnknown             = { 0x00000000, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
EXTERN_C inline const IID IID_IStream              = { 0x0000000C, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
EXTERN_C inline const IID IID_IStorage             = { 0x0000000B, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
EXTERN_C inline const IID IID_IPersist             = { 0x0000010C, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
EXTERN_C inline const IID IID_IPersistStream       = { 0x00000109, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
EXTERN_C inline const IID IID_IPropertySetStorage  = { 0x0000013A, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
EXTERN_C inline const IID IID_IPropertyStorage     = { 0x00000138, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
/* IID_ILinkStream is defined in isun_i.c; forward-declared here because this
 * header is force-included before ilinkstm.h and the _opents_iid_of below
 * needs it visible. */
EXTERN_C const IID IID_ILinkStream;
EXTERN_C inline const IID IID_IClassFactory        = { 0x00000001, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
EXTERN_C inline const GUID FMTID_SummaryInformation = { 0xF29F85E0, 0x4FF9, 0x1068, { 0xAB, 0x91, 0x08, 0x00, 0x2B, 0x27, 0xB3, 0xD9 } };

/* ---- Core COM interfaces ----------------------------------------------- */
class IUnknown {
public:
	virtual ~IUnknown() = default;
	virtual HRESULT QueryInterface(REFIID riid, LPVOID *ppvObject) = 0;
	virtual ULONG AddRef(void) = 0;
	virtual ULONG Release(void) = 0;
};

class IStream : public IUnknown {
public:
	virtual HRESULT Read(void *pv, ULONG cb, ULONG *pcbRead) = 0;
	virtual HRESULT Write(const void *pv, ULONG cb, ULONG *pcbWritten) = 0;
	virtual HRESULT Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition) = 0;
	virtual HRESULT SetSize(ULARGE_INTEGER libNewSize) = 0;
	virtual HRESULT CopyTo(IStream *pstm, ULARGE_INTEGER cb, ULARGE_INTEGER *pcbRead, ULARGE_INTEGER *pcbWritten) = 0;
	virtual HRESULT Commit(DWORD grfCommitFlags) = 0;
	virtual HRESULT Revert(void) = 0;
	virtual HRESULT LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) = 0;
	virtual HRESULT UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) = 0;
	virtual HRESULT Stat(STATSTG *pstatstg, DWORD grfStatFlag) = 0;
	virtual HRESULT Clone(IStream **ppstm) = 0;
};

/* ILinkStream is defined in the engine's ilinkstm.h (which includes this stub),
 * so only a forward declaration lives here to avoid a redefinition when that
 * header is also parsed. */
class ILinkStream;

class IPersist : public IUnknown {
public:
	virtual HRESULT GetClassID(CLSID *pClassID) = 0;
};

class IPersistStream : public IPersist {
public:
	virtual HRESULT IsDirty(void) = 0;
	virtual HRESULT Load(IStream *stream) = 0;
	virtual HRESULT Save(IStream *stream, BOOL cleardirty) = 0;
	virtual HRESULT GetSizeMax(ULARGE_INTEGER *pcbSize) = 0;
};

class IClassFactory : public IUnknown {
public:
	virtual HRESULT CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppvObj) = 0;
	virtual HRESULT LockServer(BOOL fLock) = 0;
};

class IStorage : public IUnknown {
public:
	virtual HRESULT CreateStream(const wchar_t *pwcsName, DWORD grfMode, DWORD reserved1, DWORD reserved2, IStream **ppstm) = 0;
	virtual HRESULT OpenStream(const wchar_t *pwcsName, void *reserved1, DWORD grfMode, DWORD reserved2, IStream **ppstm) = 0;
	virtual HRESULT Commit(DWORD grfCommitFlags) = 0;
	virtual HRESULT Revert(void) = 0;
};

/* Engine persist code aliases the COM interface pointers (saveload.cpp). */
typedef IPersistStream *LPPERSISTSTREAM;

/* Forward declaration: IPropertySetStorage::Open/Create hand back an
 * IPropertyStorage** (see below for the full interface). */
class IPropertyStorage;

class IPropertySetStorage : public IUnknown {
public:
	virtual HRESULT Open(REFGUID fmtid, DWORD grfMode, IPropertyStorage **ppstg) = 0;
	virtual HRESULT Create(REFGUID fmtid, void *pclsid, DWORD grfFlags, DWORD grfMode, IPropertyStorage **ppstg) = 0;
};

class IPropertyStorage : public IUnknown {
public:
	virtual HRESULT ReadMultiple(ULONG cpspec, const PROPSPEC rgpspec[], PROPVARIANT rgpropvar[]) = 0;
	virtual HRESULT WriteMultiple(ULONG cpspec, const PROPSPEC rgpspec[], const PROPVARIANT rgpropvar[], PROPID propidNameFirst) = 0;
	virtual HRESULT DeleteMultiple(ULONG cpspec, const PROPSPEC rgpspec[]) = 0;
	virtual HRESULT Read(PROPID propid, PROPVARIANT *pvar) = 0;
	virtual HRESULT Write(PROPID propid, const PROPVARIANT *pvar) = 0;
	virtual HRESULT Delete(PROPID propid) = 0;
	virtual HRESULT Commit(DWORD grfCommitFlags) = 0;
	virtual HRESULT Revert(void) = 0;
	virtual HRESULT Enum(void **ppenum) = 0;
	virtual HRESULT SetTimes(const FILETIME *pctime, const FILETIME *patime, const FILETIME *pmtime) = 0;
	virtual HRESULT Stat(STATPROPSETSTG *ppsstg) = 0;
};

/* ---- Interface identifier for the pointer being created -----------------
 *
 * A class that inherits more than one interface (a locomotor inherits both
 * ILocomotion and IPiggyback) has a different address for each subobject, so
 * the IID asked of QueryInterface decides which one comes back. Creating
 * through IID_IUnknown hands back the IUnknown subobject, which is NOT the
 * ILocomotion subobject for such a class -- the caller then dispatches through
 * the wrong vtable and jumps to nonsense. Create each smart pointer through the
 * IID of the interface it actually holds.
 */
class ILocomotion;
class IPiggyback;
class IFlyControl;
EXTERN_C const IID IID_ILocomotion;
EXTERN_C const IID IID_IPiggyback;
EXTERN_C const IID IID_IFlyControl;

/* Which IID an interface-to-interface conversion must ask for.
 *
 * _com_ptr_t converts between interface pointers by asking the source object for
 * the TARGET interface, and QueryInterface is what moves the pointer to the
 * correct subobject of a class that implements several interfaces. Asking for
 * the wrong IID (or not asking at all) leaves the pointer on the source
 * subobject, and every later call then dispatches through the wrong vtable. */
template <class T> inline IID const & _opents_iid_of(void) { return IID_IUnknown; }
template <> inline IID const & _opents_iid_of<IUnknown>(void) { return IID_IUnknown; }
template <> inline IID const & _opents_iid_of<IStream>(void) { return IID_IStream; }
template <> inline IID const & _opents_iid_of<ILinkStream>(void) { return IID_ILinkStream; }
template <> inline IID const & _opents_iid_of<IStorage>(void) { return IID_IStorage; }
template <> inline IID const & _opents_iid_of<IPersist>(void) { return IID_IPersist; }
template <> inline IID const & _opents_iid_of<IPersistStream>(void) { return IID_IPersistStream; }
template <> inline IID const & _opents_iid_of<ILocomotion>(void) { return IID_ILocomotion; }
template <> inline IID const & _opents_iid_of<IPiggyback>(void) { return IID_IPiggyback; }
template <> inline IID const & _opents_iid_of<IFlyControl>(void) { return IID_IFlyControl; }

/* ---- Native class-factory registry -------------------------------------
 *
 * On Windows, COM is used here for exactly one job: creating engine objects
 * by CLSID. startup.cpp registers a TClassFactory<T> for each CLSID through
 * CoRegisterClassObject, and the engine creates instances by CLSID through
 * the smart pointer below (which calls CoCreateInstance). ole32 supplies that
 * registry on Windows; this target has no ole32, so it is a small native
 * table. Without it every object created by CLSID -- most visibly every
 * locomotion -- comes back NULL, and the first virtual call on the NULL
 * object faults. */
#define OPENTS_MAX_CLASS_OBJECTS 512

struct _opents_class_entry {
	CLSID clsid;
	IClassFactory *factory;
	DWORD token;
};

inline _opents_class_entry * _opents_class_table(void) {
	static _opents_class_entry table[OPENTS_MAX_CLASS_OBJECTS];
	return table;
}

inline int & _opents_class_count(void) {
	static int count = 0;
	return count;
}

inline HRESULT __stdcall CoCreateInstance(REFCLSID rclsid, IUnknown *pUnkOuter, DWORD dwClsCtx, REFIID riid, void **ppv);

/* ---- Smart-pointer replacement for _com_ptr_t -------------------------- */

#include <type_traits>

/* Converting cast used by the smart pointer's raw-pointer constructor.
 *
 * _com_ptr_t wraps a foreign pointer by asking the source object for the target
 * interface, and QueryInterface is what moves the pointer onto the right
 * SUBOBJECT. AircraftClass inherits BOTH FootClass and IFlyControl, so the
 * IFlyControl subobject sits at a different address than the FootClass one: a
 * blind reinterpret_cast leaves the pointer on the source subobject and the next
 * call dispatches through the wrong vtable -- flyctrl->Landing_Altitude() landed
 * on AircraftClass::GetClassID(CLSID*) and wrote through a garbage out-pointer.
 * Ask QueryInterface, and answer NULL when the object does not implement the
 * target interface -- that is what _com_ptr_t does on Windows, and the engine's
 * "if (ptr != NULL)" guards are written against it.
 *
 * Falling back to a reinterpret_cast for a failed request is NOT a safe
 * substitute, because an object that does not implement the interface has no
 * such subobject to point at, and it matters in practice: RULES.INI puts
 * GHUNTER and NHUNTER under [VehicleTypes] with the flyer locomotor, so a
 * UnitClass really does drive a FlyLocomotionClass, and FlyLocomotionClass asks
 * its linked object for IFlyControl. On Windows the request fails and the guard
 * skips the call. With a reinterpreted pointer the call went through anyway,
 * straight into the FootClass vtable: IFlyControl derives from IUnknown, so
 * Landing_Altitude and IPersist's GetClassID are both slot 5, and the wrong
 * caller ran GetClassID with a garbage out-parameter and wrote into read-only
 * data. Every engine object that implements a second interface advertises it
 * through QueryInterface (AircraftClass for IFlyControl, and
 * Drive/Walk/DropPodLocomotionClass for IPiggyback), so a refusal here means the
 * object genuinely does not have that interface. */
template <class T, class U>
inline typename std::enable_if<std::is_base_of<IUnknown, U>::value, T *>::type
_opents_qi_cast(U *src)
{
	void *p = nullptr;
	if (src != nullptr && src->QueryInterface(_opents_iid_of<T>(), &p) == S_OK && p != nullptr) {
		return(reinterpret_cast<T *>(p));
	}
	return(nullptr);
}

template <class T, class U>
inline typename std::enable_if<!std::is_base_of<IUnknown, U>::value, T *>::type
_opents_qi_cast(U *src)
{
	return(reinterpret_cast<T *>(src));
}

template <class T>
class _opents_com_ptr {
	T *m_p;
public:
	explicit _opents_com_ptr(T *p = nullptr) : m_p(p) {}
	/* _com_ptr_t has a converting constructor from *any* interface pointer; on
	 * Windows it routes through QueryInterface. The engine relies on this to
	 * wrap a base game-object pointer (e.g. FootClass*) as IFlyControlPtr even
	 * though FootClass is not statically derived from IFlyControl -- see
	 * _opents_qi_cast above. */
	template <class U> _opents_com_ptr(U *p) : m_p(_opents_qi_cast<T>(p)) {}
	/* _com_ptr_t also has a constructor taking a CLSID that calls CreateInstance. */
	explicit _opents_com_ptr(REFCLSID clsid, IUnknown *pUnkOuter = nullptr, DWORD dwClsCtx = 0) {
		CreateInstance(clsid, pUnkOuter, dwClsCtx);
	}
	_opents_com_ptr(const _opents_com_ptr &o) : m_p(o.m_p) {}
	/* Converting constructor from another interface smart pointer.
	 *
	 * _com_ptr_t converts by asking the source object for the target interface,
	 * and that is what moves the pointer onto the right SUBOBJECT when a class
	 * implements several interfaces. A blind reinterpret_cast keeps the source
	 * subobject, so the next call dispatches through the wrong vtable: casting
	 * ILocomotion* to IPersist* and calling GetClassID lands on slot 5 of the
	 * ILocomotion table, which is Link_To_Object, and installs the CLSID
	 * out-parameter (a stack address) as the locomotor's linked object -- after
	 * which Head_To_Coord reads LinkedTo->PositionCoord and jumps through
	 * whatever the stack held there.
	 * Ask QueryInterface, and hold NULL when the object does not answer -- the
	 * same contract _com_ptr_t has on Windows, and the one the engine's
	 * "if (ptr != NULL)" guards rely on. Keeping the source pointer through a
	 * failed request would leave it on the wrong subobject. See
	 * _opents_qi_cast above for the case that made this fatal. */
	template <class U> _opents_com_ptr(const _opents_com_ptr<U> &o)
		: m_p(nullptr) {
		U *src = o.GetInterface();
		if (src != nullptr) {
			void *p = nullptr;
			if (src->QueryInterface(_opents_iid_of<T>(), &p) == S_OK && p != nullptr) {
				m_p = reinterpret_cast<T *>(p);
			}
		}
	}
	~_opents_com_ptr() {}

	T *operator->() const { return m_p; }
	operator T *() const { return m_p; }
	T *GetInterface() const { return m_p; }
	T **operator&() { return &m_p; }
	T *operator=(T *p) { m_p = p; return m_p; }
	_opents_com_ptr &operator=(const _opents_com_ptr &o) { m_p = o.m_p; return *this; }

	/* Detach: release ownership and return the raw pointer (used by call sites
	 * that hand the interface to a caller). Hollow: just returns m_p. */
	T *Detach(void) { T *p = m_p; m_p = nullptr; return p; }

	/* Attach: take ownership of a raw interface pointer. cstream.cpp uses the
	 * two-arg MSVC form Attach(Interface*, bool fAddRef). Hollow: just stores. */
	void Attach(T *p, bool fAddRef = true) { (void)fAddRef; m_p = p; }

	HRESULT CreateInstance(REFCLSID rclsid, IUnknown *pUnkOuter = nullptr, DWORD dwClsCtx = 0) {
		/* Ask the native registry to build the object, then hold it as T*.
		 * Request the IID of the interface this pointer actually holds, so a
		 * class with several interfaces answers with the right subobject.
		 * Interfaces this stub cannot name fall back to IID_IUnknown, which
		 * every engine class answers. */
		void *p = nullptr;
		HRESULT hr = CoCreateInstance(rclsid, pUnkOuter, dwClsCtx, _opents_iid_of<T>(), &p);
		m_p = reinterpret_cast<T *>(p);
		return hr;
	}
	ULONG Release(void) { return 0; }
};

#define _COM_SMARTPTR_TYPEDEF(T, uuid) typedef _opents_com_ptr<T> T##Ptr
/* __uuidof(T) maps to the interface's real IID via the _opents_iid_of table.
 * The earlier "IID()" (all-zero) form made every QueryInterface(__uuidof(IStream))
 * get rejected, so the compressor never bound its storage stream. */
#define __uuidof(T)                     _opents_iid_of<T>()

/* Standard COM smart-pointer aliases. On Windows these are generated by the
 * system COM headers (objidl.h / unknwn.h); the engine references them
 * without ever declaring them, so the stub supplies them. */
typedef _opents_com_ptr<IUnknown>            IUnknownPtr;
typedef _opents_com_ptr<IStream>             IStreamPtr;
typedef _opents_com_ptr<IStorage>            IStoragePtr;
typedef _opents_com_ptr<IPersist>            IPersistPtr;
typedef _opents_com_ptr<IPersistStream>      IPersistStreamPtr;
typedef _opents_com_ptr<IPropertySetStorage> IPropertySetStoragePtr;
typedef _opents_com_ptr<IPropertyStorage>     IPropertyStoragePtr;

/* ---- OleSaveToStream / OleLoadFromStream --------------------------------
 *
 * These are the real OLE entry points the engine's save/load uses for every
 * persistable game object. On Windows they live in ole32; here they are
 * implemented against the port's IStream. The contract (matching the original):
 *
 *   OleSaveToStream : write the object's CLSID (WriteClassStm), then call
 *                     IPersistStream::Save. AbstractClass::Save_Members writes
 *                     the swizzle id + members AFTER this CLSID prefix.
 *   OleLoadFromStream: read the CLSID (ReadClassStm), CoCreateInstance it, then
 *                     call IPersistStream::Load. The freshly-built object
 *                     reattaches itself to its heap during Load.
 */
inline HRESULT __stdcall WriteClassStm(IStream *pStm, REFCLSID rclsid) {
	if (pStm == NULL) return E_POINTER;
	ULONG written = 0;
	return pStm->Write((const void *)&rclsid, (ULONG)sizeof(CLSID), &written);
}
inline HRESULT __stdcall ReadClassStm(IStream *pStm, CLSID *pclsid) {
	if (pStm == NULL || pclsid == NULL) return E_POINTER;
	ULONG read = 0;
	HRESULT hr = pStm->Read(pclsid, (ULONG)sizeof(CLSID), &read);
	if (SUCCEEDED(hr) && read != (ULONG)sizeof(CLSID)) hr = E_FAIL;
	return hr;
}
inline HRESULT __stdcall OleSaveToStream(IUnknown *pPS, IStream *pStm) {
	if (pPS == NULL || pStm == NULL) return E_POINTER;
	IPersistStream *pps = NULL;
	HRESULT hr = pPS->QueryInterface(_opents_iid_of<IPersistStream>(), (void **)&pps);
	if (FAILED(hr)) return hr;
	CLSID clsid;
	hr = pps->GetClassID(&clsid);
	if (SUCCEEDED(hr)) {
		hr = WriteClassStm(pStm, clsid);
	}
	if (SUCCEEDED(hr)) {
		hr = pps->Save(pStm, TRUE);
	}
	pps->Release();
	return hr;
}
inline HRESULT __stdcall OleLoadFromStream(IStream *pStm, REFIID riid, void **ppvObj) {
	if (pStm == NULL || ppvObj == NULL) return E_POINTER;
	*ppvObj = NULL;
	CLSID clsid;
	HRESULT hr = ReadClassStm(pStm, &clsid);
	if (FAILED(hr)) return hr;
	IUnknown *punk = NULL;
	hr = CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER, IID_IUnknown, (void **)&punk);
	if (FAILED(hr)) return hr;
	IPersistStream *pps = NULL;
	hr = punk->QueryInterface(_opents_iid_of<IPersistStream>(), (void **)&pps);
	if (SUCCEEDED(hr)) {
		hr = pps->Load(pStm);
		if (SUCCEEDED(hr)) {
			hr = pps->QueryInterface(riid, ppvObj);
		}
		pps->Release();
	}
	punk->Release();
	return hr;
}
inline HRESULT __stdcall CoRegisterClassObject(REFCLSID rclsid, IUnknown *pUnk, DWORD dwClsCtx, DWORD flags, DWORD *pdwRegister) {
	(void)dwClsCtx; (void)flags;
	static DWORD next_token = 1;
	int & count = _opents_class_count();
	if (count >= OPENTS_MAX_CLASS_OBJECTS) {
		return E_OUTOFMEMORY;
	}
	_opents_class_entry & entry = _opents_class_table()[count++];
	entry.clsid = rclsid;
	entry.factory = reinterpret_cast<IClassFactory *>(pUnk);
	entry.token = next_token++;
	if (pdwRegister != NULL) {
		*pdwRegister = entry.token;
	}
	return S_OK;
}
inline HRESULT __stdcall CoRevokeClassObject(DWORD dwRegister) {
	_opents_class_entry * table = _opents_class_table();
	int & count = _opents_class_count();
	for (int i = 0; i < count; i++) {
		if (table[i].token == dwRegister) {
			table[i] = table[count - 1];
			count--;
			break;
		}
	}
	return S_OK;
}
inline HRESULT __stdcall CoInitialize(void *pvReserved) {
	(void)pvReserved; return S_OK;
}
inline void __stdcall CoUninitialize(void) {}
inline HRESULT __stdcall CoCreateInstance(REFCLSID rclsid, IUnknown *pUnkOuter, DWORD dwClsCtx, REFIID riid, void **ppv) {
	(void)dwClsCtx;
	if (ppv == NULL) {
		return E_POINTER;
	}
	*ppv = NULL;
	_opents_class_entry * table = _opents_class_table();
	int count = _opents_class_count();
	for (int i = 0; i < count; i++) {
		if (table[i].factory != NULL && table[i].clsid == rclsid) {
			return table[i].factory->CreateInstance(pUnkOuter, riid, ppv);
		}
	}
	return CLASS_E_CLASSNOTAVAILABLE;
}
/* ---------------------------------------------------------------------------
 * Minimal structured-storage implementation (Apple Silicon / macOS port)
 *
 * The Windows build persists save games through OLE structured storage
 * (StgCreateDocfile / StgOpenStorage over IStorage/IStream). There is no ole32
 * on this target, so the entry points above were hollow stubs and every
 * save/load failed at "Creating DocFile". The classes below supply a
 * self-consistent, non-OLE container that round-trips inside this same build.
 * It is intentionally NOT byte-compatible with Windows DocFiles: cross-platform
 * save exchange is tracked separately (Piece C) and is a different problem from
 * "make save/load work on the port".
 *
 * SaveVersionInfo::Save/Load each carry two persistence paths -- a property-set
 * path (IPropertySetStorage) and a per-stream "old way". By answering
 * E_NOINTERFACE to IID_IPropertySetStorage, this storage steers the engine onto
 * the per-stream path, which is fully implemented here, so the standard OLE
 * property-set binary format does not need to be reproduced.
 * ------------------------------------------------------------------------- */
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <cstring>
#include <chrono>

#ifndef STREAM_SEEK_SET
#define STREAM_SEEK_SET 0
#define STREAM_SEEK_CUR 1
#define STREAM_SEEK_END 2
#endif

class OpentsStream : public IStream {
public:
	std::wstring name_;
	std::vector<unsigned char> data_;
	size_t pos_;
	explicit OpentsStream(const wchar_t *name) : pos_(0) { if (name) name_ = name; }
	virtual ~OpentsStream() {}

	HRESULT QueryInterface(REFIID riid, LPVOID *ppv) {
		if (!ppv) return E_POINTER;
		*ppv = nullptr;
		if (riid == IID_IUnknown) *ppv = static_cast<IUnknown *>(this);
		else if (riid == IID_IStream) *ppv = static_cast<IStream *>(this);
		return (*ppv != nullptr) ? S_OK : E_NOINTERFACE;
	}
	ULONG AddRef(void) { return 1; }
	ULONG Release(void) { return 1; }

	HRESULT Read(void *pv, ULONG cb, ULONG *pcbRead) {
		if (!pv) return E_POINTER;
		size_t avail = (data_.size() > pos_) ? (data_.size() - pos_) : 0;
		size_t got = (cb < avail) ? cb : avail;
		if (got) std::memcpy(pv, data_.data() + pos_, got);
		pos_ += got;
		if (pcbRead) *pcbRead = static_cast<ULONG>(got);
		return S_OK;
	}
	HRESULT Write(const void *pv, ULONG cb, ULONG *pcbWritten) {
		if (!pv) return E_POINTER;
		if (pos_ + cb > data_.size()) data_.resize(pos_ + cb);
		std::memcpy(data_.data() + pos_, pv, cb);
		pos_ += cb;
		if (pcbWritten) *pcbWritten = cb;
		return S_OK;
	}
	HRESULT Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition) {
		long long base = 0;
		if (dwOrigin == STREAM_SEEK_CUR) base = (long long)pos_;
		else if (dwOrigin == STREAM_SEEK_END) base = (long long)data_.size();
		long long np = base + dlibMove.QuadPart;
		if (np < 0) np = 0;
		pos_ = (size_t)np;
		if (plibNewPosition) plibNewPosition->QuadPart = (ULONGLONG)pos_;
		return S_OK;
	}
	HRESULT SetSize(ULARGE_INTEGER libNewSize) {
		data_.resize((size_t)libNewSize.QuadPart);
		return S_OK;
	}
	HRESULT CopyTo(IStream *pstm, ULARGE_INTEGER cb, ULARGE_INTEGER *pcbRead, ULARGE_INTEGER *pcbWritten) {
		if (!pstm) return E_POINTER;
		size_t n = (size_t)cb.QuadPart;
		if (n > data_.size() - pos_) n = data_.size() - pos_;
		std::vector<unsigned char> buf(n);
		if (n) std::memcpy(buf.data(), data_.data() + pos_, n);
		ULONG wrote = 0;
		HRESULT hr = pstm->Write(buf.data(), (ULONG)n, &wrote);
		pos_ += n;
		if (pcbRead) pcbRead->QuadPart = n;
		if (pcbWritten) pcbWritten->QuadPart = wrote;
		return hr;
	}
	HRESULT Commit(DWORD) { return S_OK; }
	HRESULT Revert(void) { return S_OK; }
	HRESULT LockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) { return S_OK; }
	HRESULT UnlockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) { return S_OK; }
	HRESULT Stat(STATSTG *pstatstg, DWORD) {
		if (pstatstg) pstatstg->cbSize.QuadPart = data_.size();
		return S_OK;
	}
	HRESULT Clone(IStream **ppstm) {
		if (!ppstm) return E_POINTER;
		OpentsStream *s = new OpentsStream(name_.c_str());
		s->data_ = data_;
		s->pos_ = pos_;
		*ppstm = s;
		return S_OK;
	}
};

class OpentsStorage : public IStorage {
public:
	std::wstring path_;
	std::map<std::wstring, OpentsStream *> streams_;

	explicit OpentsStorage(const wchar_t *path) { if (path) path_ = path; }
	virtual ~OpentsStorage() {
		for (auto &kv : streams_) delete kv.second;
		streams_.clear();
	}

	HRESULT QueryInterface(REFIID riid, LPVOID *ppv) {
		if (!ppv) return E_POINTER;
		*ppv = nullptr;
		if (riid == IID_IUnknown) *ppv = static_cast<IUnknown *>(this);
		else if (riid == IID_IStorage) *ppv = static_cast<IStorage *>(this);
		/* Deliberately not IID_IPropertySetStorage: forces the per-stream path. */
		return (*ppv != nullptr) ? S_OK : E_NOINTERFACE;
	}
	ULONG AddRef(void) { return 1; }
	ULONG Release(void) { return 1; }

	HRESULT CreateStream(const wchar_t *pwcsName, DWORD, DWORD, DWORD, IStream **ppstm) {
		if (!pwcsName || !ppstm) return E_POINTER;
		*ppstm = nullptr;
		std::wstring name(pwcsName);
		OpentsStream *s;
		auto it = streams_.find(name);
		if (it != streams_.end()) {
			s = it->second;
			s->data_.clear();
			s->pos_ = 0;
		} else {
			s = new OpentsStream(pwcsName);
			streams_[name] = s;
		}
		*ppstm = s;
		return S_OK;
	}
	HRESULT OpenStream(const wchar_t *pwcsName, void *, DWORD, DWORD, IStream **ppstm) {
		if (!pwcsName || !ppstm) return E_POINTER;
		*ppstm = nullptr;
		auto it = streams_.find(std::wstring(pwcsName));
		if (it == streams_.end()) return E_FAIL;
		*ppstm = it->second;
		return S_OK;
	}
	HRESULT Commit(DWORD) { return WriteToFile(path_); }
	HRESULT Revert(void) { return S_OK; }

private:
	/* The stub's MultiByteToWideChar maps each input byte 1:1 into a wchar_t,
	 * so reversing it per byte reconstructs the original (possibly UTF-8) path. */
	static std::string NarrowPath(const std::wstring &wp) {
		std::string out;
		for (wchar_t c : wp) {
			if (c == 0) break;
			out.push_back((char)(c & 0xFF));
		}
		return out;
	}
	HRESULT WriteToFile(const std::wstring &wpath) {
		std::string path = NarrowPath(wpath);
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		if (!out) return E_FAIL;
		uint32_t magic = 0x4F54444F; /* 'OTDO' */
		uint32_t version = 1;
		uint32_t n = (uint32_t)streams_.size();
		out.write((const char *)&magic, 4);
		out.write((const char *)&version, 4);
		out.write((const char *)&n, 4);
		for (auto &kv : streams_) {
			const std::wstring &nm = kv.first;
			OpentsStream *s = kv.second;
			uint32_t nl = (uint32_t)nm.size();
			uint32_t dl = (uint32_t)s->data_.size();
			out.write((const char *)&nl, 4);
			out.write((const char *)nm.data(), (std::streamsize)(nl * sizeof(wchar_t)));
			out.write((const char *)&dl, 4);
			if (dl) out.write((const char *)s->data_.data(), (std::streamsize)dl);
		}
		out.flush();
		return out ? S_OK : E_FAIL;
	}
	HRESULT ReadFromFile(const std::wstring &wpath) {
		std::string path = NarrowPath(wpath);
		std::ifstream in(path, std::ios::binary);
		if (!in) return E_FAIL;
		uint32_t magic, version, n;
		in.read((char *)&magic, 4);
		in.read((char *)&version, 4);
		in.read((char *)&n, 4);
		if (!in || magic != 0x4F54444F) return E_FAIL;
		for (uint32_t i = 0; i < n; i++) {
			uint32_t nl, dl;
			in.read((char *)&nl, 4);
			std::wstring nm;
			nm.resize(nl);
			if (nl) in.read((char *)nm.data(), (std::streamsize)(nl * sizeof(wchar_t)));
			in.read((char *)&dl, 4);
			OpentsStream *s = new OpentsStream(nm.c_str());
			s->data_.resize(dl);
			if (dl) in.read((char *)s->data_.data(), (std::streamsize)dl);
			if (!in) { delete s; return E_FAIL; }
			streams_[nm] = s;
		}
		return S_OK;
	}

	friend inline HRESULT __stdcall StgCreateDocfile(const wchar_t *, DWORD, DWORD, IStorage **);
	friend inline HRESULT __stdcall StgOpenStorage(const wchar_t *, IStorage *, DWORD, void *, DWORD, IStorage **);
};

/* Single active storage per translation unit. Save/load calls are sequential
 * within saveload.cpp, so recycling the previous one bounds memory without a
 * linked ownership model (the port's COM smart pointers are no-ops for
 * lifetime, by design). */
static OpentsStorage * & opents_active_storage(void) {
	static OpentsStorage *p = nullptr;
	return p;
}

inline HRESULT __stdcall StgCreateDocfile(const wchar_t *pwcsName, DWORD grfMode, DWORD reserved, IStorage **ppstgCreate) {
	(void)grfMode; (void)reserved;
	if (!ppstgCreate) return E_POINTER;
	*ppstgCreate = nullptr;
	if (opents_active_storage()) { delete opents_active_storage(); opents_active_storage() = nullptr; }
	OpentsStorage *s = new OpentsStorage(pwcsName);
	opents_active_storage() = s;
	*ppstgCreate = s;
	return S_OK;
}

inline HRESULT __stdcall StgOpenStorage(const wchar_t *pwcsName, IStorage *pstgPriority, DWORD grfMode, void *snbExclude, DWORD reserved, IStorage **ppstgOpen) {
	(void)pstgPriority; (void)grfMode; (void)snbExclude; (void)reserved;
	if (!ppstgOpen) return E_POINTER;
	*ppstgOpen = nullptr;
	if (opents_active_storage()) { delete opents_active_storage(); opents_active_storage() = nullptr; }
	OpentsStorage *s = new OpentsStorage(pwcsName);
	opents_active_storage() = s;
	HRESULT hr = s->ReadFromFile(pwcsName);
	if (FAILED(hr)) { *ppstgOpen = nullptr; return hr; }
	*ppstgOpen = s;
	return S_OK;
}

inline HRESULT __stdcall CoFileTimeNow(FILETIME *pft) {
	if (!pft) return E_POINTER;
	using namespace std::chrono;
	auto now = system_clock::now();
	long long secs = duration_cast<seconds>(now.time_since_epoch()).count();
	long long const unix_to_ft = 11644473600LL; /* 1601-01-01 -> 1970-01-01 */
	long long total_100ns = (secs + unix_to_ft) * 10000000LL;
	pft->dwLowDateTime = (DWORD)(total_100ns & 0xFFFFFFFF);
	pft->dwHighDateTime = (DWORD)(total_100ns >> 32);
	return S_OK;
}

/* _com_issue_error: COM exception helper (comdef.h). Hollow: no exception. */
inline void __stdcall _com_issue_error(HRESULT hr) { (void)hr; }

/* Interlocked* : used by the ref-count machinery in the engine's COM classes
 * (cstream.cpp). Provided as hollow atomics. Overloads cover both plain and
 * volatile LONG* call sites. */
inline LONG InterlockedIncrement(LONG *p) { return ++(*p); }
inline LONG InterlockedIncrement(volatile LONG *p) { return ++(*p); }
inline LONG InterlockedDecrement(LONG *p) { return --(*p); }
inline LONG InterlockedDecrement(volatile LONG *p) { return --(*p); }
inline LONG InterlockedExchange(LONG *p, LONG v) { LONG o = *p; *p = v; return o; }
inline LONG InterlockedCompareExchange(LONG *p, LONG v, LONG c) { LONG o = *p; if (o == c) *p = v; return o; }

/* Some engine TUs pull in com_stub.h via their own headers (e.g. ini.h) without
 * ever including <windows.h>, yet still reference Win32 APIs (ini.cpp's
 * CP_ACP / MultiByteToWideChar / GetLastError). Bring in the Win32 surface
 * here so those TUs get it too. windows_stub.h includes com_stub.h at its top;
 * the mutual include is broken cleanly by the include guards (no recursion). */
#include "windows_stub.h"

#endif /* OPENTS_PORT_COM_STUB_H */
