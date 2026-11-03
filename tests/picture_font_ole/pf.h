/* Shared helpers for the picture_font_ole area: oleaut32 OleCreateFontIndirect / IFont, StdPicture
 * (OleCreatePictureIndirect, OleLoadPicture*, OleSavePictureFile), OleTranslateColor, OleIconToCursor,
 * OleCreatePropertyFrame*. Every oleaut32 entry point is resolved through t.h's U()/UIN() (LoadLibrary +
 * GetProcAddress); interface methods are called through vtables (CINTERFACE + COBJMACROS).
 * GDI/USER calls (CreateBitmap, CreateDIBSection...) are fixture scaffolding only. */
#ifndef PF_H
#define PF_H
#define COBJMACROS
#define CINTERFACE
#define INITGUID
#include "t.h"
#include <objbase.h>
#include <ole2.h>
#include <olectl.h>
#include <ocidl.h>
#include <oleauto.h>
#include <stdlib.h>
#include "pf_data.h"

/* GUID_NULL is not defined by the headers under INITGUID in this toolchain: use a local one. */
static const GUID PF_GUID_NULL = { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } };
#undef IID_NULL
#define IID_NULL PF_GUID_NULL

/* The harness links only kernel32/advapi32/ntdll: ole32/oleaut32 scaffolding (BSTRs, variants, streams) goes
 * through the module list, GDI/USER32 through explicit LoadLibrary of those DLLs. */
#define PF_OA(n) /* resolved over the module list */
#define SysAllocString(...)         (U(SysAllocString)(__VA_ARGS__))
#define SysAllocStringLen(...)      (U(SysAllocStringLen)(__VA_ARGS__))
#define SysFreeString(...)          (U(SysFreeString)(__VA_ARGS__))
#define SysStringLen(...)           (U(SysStringLen)(__VA_ARGS__))
#define VariantInit(...)            (U(VariantInit)(__VA_ARGS__))
#define VariantClear(...)           (U(VariantClear)(__VA_ARGS__))
#define CreateStreamOnHGlobal(...)  (U(CreateStreamOnHGlobal)(__VA_ARGS__))
#define PF_GDI(n) (UIN("gdi32.dll", n))
#define PF_USR(n) (UIN("user32.dll", n))
#define CreateCompatibleDC(...)     (PF_GDI(CreateCompatibleDC)(__VA_ARGS__))
#define CreateCompatibleBitmap(...) (PF_GDI(CreateCompatibleBitmap)(__VA_ARGS__))
#define CreateDIBSection(...)       (PF_GDI(CreateDIBSection)(__VA_ARGS__))
#define CreateBitmap(...)           (PF_GDI(CreateBitmap)(__VA_ARGS__))
#define DeleteDC(...)               (PF_GDI(DeleteDC)(__VA_ARGS__))
#define DeleteObject(...)           (PF_GDI(DeleteObject)(__VA_ARGS__))
#define SelectObject(...)           (PF_GDI(SelectObject)(__VA_ARGS__))
#define GetDeviceCaps(...)          (PF_GDI(GetDeviceCaps)(__VA_ARGS__))
#define GetObjectA(...)             (PF_GDI(GetObjectA)(__VA_ARGS__))
#define GetObjectW(...)             (PF_GDI(GetObjectW)(__VA_ARGS__))
#define GetObjectType(...)          (PF_GDI(GetObjectType)(__VA_ARGS__))
#define GetPixel(...)               (PF_GDI(GetPixel)(__VA_ARGS__))
#define SetPixelV(...)              (PF_GDI(SetPixelV)(__VA_ARGS__))
#define CreateMetaFileA(...)        (PF_GDI(CreateMetaFileA)(__VA_ARGS__))
#define CloseMetaFile(...)          (PF_GDI(CloseMetaFile)(__VA_ARGS__))
#define DeleteMetaFile(...)         (PF_GDI(DeleteMetaFile)(__VA_ARGS__))
#define CreateEnhMetaFileA(...)     (PF_GDI(CreateEnhMetaFileA)(__VA_ARGS__))
#define CloseEnhMetaFile(...)       (PF_GDI(CloseEnhMetaFile)(__VA_ARGS__))
#define DeleteEnhMetaFile(...)      (PF_GDI(DeleteEnhMetaFile)(__VA_ARGS__))
#define GetEnhMetaFileHeader(...)   (PF_GDI(GetEnhMetaFileHeader)(__VA_ARGS__))
#define GetEnhMetaFileBits(...)     (PF_GDI(GetEnhMetaFileBits)(__VA_ARGS__))
#define CreatePalette(...)          (PF_GDI(CreatePalette)(__VA_ARGS__))
#define GetStockObject(...)         (PF_GDI(GetStockObject)(__VA_ARGS__))
#define GetPaletteEntries(...)      (PF_GDI(GetPaletteEntries)(__VA_ARGS__))
#define BitBlt(...)                 (PF_GDI(BitBlt)(__VA_ARGS__))
#define GetDIBits(...)              (PF_GDI(GetDIBits)(__VA_ARGS__))
#define Rectangle(...)              (PF_GDI(Rectangle)(__VA_ARGS__))
#define CreateSolidBrush(...)       (PF_GDI(CreateSolidBrush)(__VA_ARGS__))
#define FillRect(...)               (PF_USR(FillRect)(__VA_ARGS__))
#define GetDC(...)                  (PF_USR(GetDC)(__VA_ARGS__))
#define ReleaseDC(...)              (PF_USR(ReleaseDC)(__VA_ARGS__))
#define LoadIconA(...)              (PF_USR(LoadIconA)(__VA_ARGS__))
#define CreateIcon(...)             (PF_USR(CreateIcon)(__VA_ARGS__))
#define DestroyIcon(...)            (PF_USR(DestroyIcon)(__VA_ARGS__))
#define DestroyCursor(...)          (PF_USR(DestroyCursor)(__VA_ARGS__))
#define CopyIcon(...)               (PF_USR(CopyIcon)(__VA_ARGS__))
#define GetIconInfo(...)            (PF_USR(GetIconInfo)(__VA_ARGS__))
#define GetSystemMetrics(...)       (PF_USR(GetSystemMetrics)(__VA_ARGS__))
#define GetSysColor(...)            (PF_USR(GetSysColor)(__VA_ARGS__))

/* Single-evaluation equality check (EXPECT_EQ evaluates its arguments twice). */
#define EQ(expr, want) do { ULONG_PTR v_ = (ULONG_PTR)(expr); \
    t_check(v_ == (ULONG_PTR)(want), __FILE__, __LINE__, #expr " == " #want, "got 0x%llx want 0x%llx", \
            (unsigned long long)v_, (unsigned long long)(ULONG_PTR)(want)); } while (0)
/* Soft equality: a mismatch is logged with t_note, never fails (unverified / SKU dependent). */
#define SOFT_EQ(expr, want) do { ULONG_PTR v_ = (ULONG_PTR)(expr); \
    if (v_ != (ULONG_PTR)(want)) t_note("%s:%d: %s: got 0x%llx, expected 0x%llx", __FILE__, __LINE__, #expr, \
            (unsigned long long)v_, (unsigned long long)(ULONG_PTR)(want)); } while (0)
#define PF_N(a) ((int)(sizeof(a) / sizeof((a)[0])))

/* HRESULTs spelled out so that a header mismatch cannot hide a wrong expectation. */
#define PF_S_FALSE                      ((HRESULT)1)
#define PF_E_UNEXPECTED                 ((HRESULT)0x8000FFFF)
#define PF_E_NOTIMPL                    ((HRESULT)0x80004001)
#define PF_E_NOINTERFACE                ((HRESULT)0x80004002)
#define PF_E_POINTER                    ((HRESULT)0x80004003)
#define PF_E_FAIL                       ((HRESULT)0x80004005)
#define PF_E_INVALIDARG                 ((HRESULT)0x80070057)
#define PF_CTL_E_INVALIDPROPERTYVALUE   ((HRESULT)0x800A017C)
#define PF_CTL_E_INVALIDPICTURE         ((HRESULT)0x800A01E1)
#define PF_CTL_E_FILENOTFOUND           ((HRESULT)0x800A0035)
#define PF_CTL_E_PATHFILEACCESSERROR    ((HRESULT)0x800A004B)
#define PF_DISP_E_UNKNOWNINTERFACE      ((HRESULT)0x80020001)
#define PF_DISP_E_MEMBERNOTFOUND        ((HRESULT)0x80020003)
#define PF_DISP_E_PARAMNOTFOUND         ((HRESULT)0x80020004)
#define PF_DISP_E_TYPEMISMATCH          ((HRESULT)0x80020005)
#define PF_DISP_E_UNKNOWNNAME           ((HRESULT)0x80020006)
#define PF_DISP_E_BADVARTYPE            ((HRESULT)0x80020008)
#define PF_DISP_E_BADINDEX              ((HRESULT)0x8002000B)
#define PF_DISP_E_BADPARAMCOUNT         ((HRESULT)0x8002000E)
#define PF_DISP_E_PARAMNOTOPTIONAL      ((HRESULT)0x8002000F)
#define PF_CONNECT_E_NOCONNECTION       ((HRESULT)0x80040200)
#define PF_CONNECT_E_CANNOTCONNECT      ((HRESULT)0x80040201)
#define PF_INET_E_UNKNOWN_PROTOCOL      ((HRESULT)0x800C000D)
#define PF_INET_E_RESOURCE_NOT_FOUND    ((HRESULT)0x800C0005)

static const GUID PF_CLSID_StdFont = { 0x0be35203, 0x8f91, 0x11ce, { 0x9d, 0xe3, 0x00, 0xaa, 0x00, 0x4b, 0xb8, 0x51 } };
static const GUID PF_CLSID_StdPicture = { 0x0be35204, 0x8f91, 0x11ce, { 0x9d, 0xe3, 0x00, 0xaa, 0x00, 0x4b, 0xb8, 0x51 } };

/* Font fixtures */
static inline void pf_fontdesc(FONTDESC *fd, const WCHAR *name, LONGLONG cy, SHORT weight, SHORT charset,
                               BOOL it, BOOL ul, BOOL st) {
    fd->cbSizeofstruct = sizeof(FONTDESC);
    fd->lpstrName = (LPOLESTR)name;
    fd->cySize.int64 = cy;
    fd->sWeight = weight;
    fd->sCharset = charset;
    fd->fItalic = it;
    fd->fUnderline = ul;
    fd->fStrikethrough = st;
}
static const WCHAR pf_arial[] = { 'A','r','i','a','l',0 };
static const WCHAR pf_system[] = { 'S','y','s','t','e','m',0 };
static const WCHAR pf_mssans[] = { 'M','S',' ','S','a','n','s',' ','S','e','r','i','f',0 };

/* Create a plain 12pt Arial IFont; returns NULL when the creator is missing or fails. */
static inline IFont *pf_font_new(void) {
    FONTDESC fd; IFont *f = NULL;
    if (!U(OleCreateFontIndirect)) return NULL;
    pf_fontdesc(&fd, pf_arial, 120000, FW_NORMAL, ANSI_CHARSET, FALSE, FALSE, FALSE);
    if (FAILED(U(OleCreateFontIndirect)(&fd, &IID_IFont, (void **)&f))) return NULL;
    return f;
}

/* ---- IFontEventsDisp sink -------------------------------------------------------------------- */
typedef struct {
    IFontEventsDisp iface;
    LONG ref;
    int calls;
    DISPID last_dispid;
    WORD last_flags;
    UINT last_cargs;
    VARTYPE last_vt;
    WCHAR last_name[64];
    int bad_riid;
    int iunk_only;   /* refuse IFontEventsDisp/IDispatch in QueryInterface */
} pf_sink_t;
void pf_sink_init(pf_sink_t *s, int iunk_only);

/* ---- picture fixtures ----------------------------------------------------------------------- */
/* Memory-backed forward-only IStream with Read only (Seek/Stat unsupported), to exercise
 * OleLoadPicture on non-seekable, non-statable streams. */
IStream *pf_noseek_stream(const void *data, size_t len);
/* CreateStreamOnHGlobal over a copy of data; NULL on failure. */
IStream *pf_mem_stream(const void *data, size_t len);
/* Build "lt\0\0" + len + payload (n copies of the header when n > 1 mimic Wine's fixture). */
size_t pf_wrap_lt(BYTE *out, const void *payload, DWORD len);
/* Load blob through OleLoadPicture; returns the HRESULT, *pic set to IPicture or NULL. */
HRESULT pf_load(const void *data, size_t len, LONG lsize, IPicture **pic);
/* 1x1/2x2 24bpp DIB section bitmap; returns NULL if GDI cannot create it. */
HBITMAP pf_dib(int w, int h, int bpp, const COLORREF *pixels, void **bits_out);
IPicture *pf_pic_from_bitmap(HBITMAP bmp, HPALETTE pal, BOOL own);
/* Memory DC with a w x h 32bpp DIB selected; *old receives the stock bitmap. */
HDC pf_render_dc(int w, int h, HBITMAP *old, HBITMAP *dib_out);
void pf_render_dc_free(HDC dc, HBITMAP old, HBITMAP dib);
/* Build a 1x1 or 2x2 32bpp-less ICO file image in out; returns size. kind: 1 = icon, 2 = cursor */
size_t pf_build_ico(BYTE *out, int kind, int w, int h);
#endif
