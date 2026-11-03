/* Shared fixtures for the picture_font_ole area (event sink, streams, bitmaps, ICO builder). */
#include "pf.h"

/* ---- IFontEventsDisp sink ------------------------------------------------------------------- */
static pf_sink_t *sink_of(IFontEventsDisp *i) { return (pf_sink_t *)i; }
static HRESULT WINAPI sk_qi(IFontEventsDisp *i, REFIID riid, void **pp) {
    pf_sink_t *s = sink_of(i);
    if (IsEqualIID(riid, &IID_IUnknown) ||
        (!s->iunk_only && (IsEqualIID(riid, &IID_IFontEventsDisp) || IsEqualIID(riid, &IID_IDispatch)))) {
        *pp = i; InterlockedIncrement(&s->ref); return S_OK;
    }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI sk_addref(IFontEventsDisp *i) { return InterlockedIncrement(&sink_of(i)->ref); }
static ULONG WINAPI sk_release(IFontEventsDisp *i) { return InterlockedDecrement(&sink_of(i)->ref); }
static HRESULT WINAPI sk_gtic(IFontEventsDisp *i, UINT *p) { return E_NOTIMPL; }
static HRESULT WINAPI sk_gti(IFontEventsDisp *i, UINT n, LCID l, ITypeInfo **p) { return E_NOTIMPL; }
static HRESULT WINAPI sk_gids(IFontEventsDisp *i, REFIID r, LPOLESTR *n, UINT c, LCID l, DISPID *d) { return E_NOTIMPL; }
static HRESULT WINAPI sk_invoke(IFontEventsDisp *i, DISPID id, REFIID riid, LCID lcid, WORD flags,
                                DISPPARAMS *dp, VARIANT *res, EXCEPINFO *ei, UINT *ae) {
    pf_sink_t *s = sink_of(i);
    s->calls++;
    s->last_dispid = id;
    s->last_flags = flags;
    s->bad_riid |= !IsEqualGUID(riid, &IID_NULL);
    s->last_cargs = dp ? dp->cArgs : 0xffff;
    s->last_name[0] = 0;
    s->last_vt = VT_EMPTY;
    if (dp && dp->cArgs >= 1) {
        s->last_vt = V_VT(&dp->rgvarg[0]);
        if (V_VT(&dp->rgvarg[0]) == VT_BSTR && V_BSTR(&dp->rgvarg[0]))
            lstrcpynW(s->last_name, V_BSTR(&dp->rgvarg[0]), 64);
    }
    return S_OK;
}
static IFontEventsDispVtbl sink_vtbl = { sk_qi, sk_addref, sk_release, sk_gtic, sk_gti, sk_gids, sk_invoke };
void pf_sink_init(pf_sink_t *s, int iunk_only) {
    memset(s, 0, sizeof *s);
    s->iface.lpVtbl = &sink_vtbl;
    s->ref = 1;
    s->iunk_only = iunk_only;
}

/* ---- streams ------------------------------------------------------------------------------- */
typedef struct { IStream iface; LONG ref; BYTE *data; size_t len, pos; } noseek_t;
static noseek_t *ns_of(IStream *i) { return (noseek_t *)i; }
static HRESULT WINAPI ns_qi(IStream *i, REFIID riid, void **pp) {
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_ISequentialStream) || IsEqualIID(riid, &IID_IStream)) {
        *pp = i; InterlockedIncrement(&ns_of(i)->ref); return S_OK;
    }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI ns_addref(IStream *i) { return InterlockedIncrement(&ns_of(i)->ref); }
static ULONG WINAPI ns_release(IStream *i) {
    noseek_t *n = ns_of(i); LONG r = InterlockedDecrement(&n->ref);
    if (!r) { free(n->data); free(n); }
    return r;
}
static HRESULT WINAPI ns_read(IStream *i, void *pv, ULONG cb, ULONG *pcb) {
    noseek_t *n = ns_of(i); size_t avail = n->len - n->pos, take = cb < avail ? cb : avail;
    memcpy(pv, n->data + n->pos, take); n->pos += take;
    if (pcb) *pcb = (ULONG)take;
    return take == cb ? S_OK : S_FALSE;
}
static HRESULT WINAPI ns_write(IStream *i, const void *pv, ULONG cb, ULONG *pcb) { return E_NOTIMPL; }
static HRESULT WINAPI ns_seek(IStream *i, LARGE_INTEGER m, DWORD o, ULARGE_INTEGER *p) { return E_NOTIMPL; }
static HRESULT WINAPI ns_setsize(IStream *i, ULARGE_INTEGER s) { return E_NOTIMPL; }
static HRESULT WINAPI ns_copyto(IStream *i, IStream *d, ULARGE_INTEGER c, ULARGE_INTEGER *r, ULARGE_INTEGER *w) { return E_NOTIMPL; }
static HRESULT WINAPI ns_commit(IStream *i, DWORD f) { return E_NOTIMPL; }
static HRESULT WINAPI ns_revert(IStream *i) { return E_NOTIMPL; }
static HRESULT WINAPI ns_lock(IStream *i, ULARGE_INTEGER o, ULARGE_INTEGER c, DWORD t) { return E_NOTIMPL; }
static HRESULT WINAPI ns_stat(IStream *i, STATSTG *s, DWORD f) { return E_NOTIMPL; }
static HRESULT WINAPI ns_clone(IStream *i, IStream **p) { return E_NOTIMPL; }
static IStreamVtbl ns_vtbl = { ns_qi, ns_addref, ns_release, ns_read, ns_write, ns_seek, ns_setsize, ns_copyto,
    ns_commit, ns_revert, ns_lock, ns_lock, ns_stat, ns_clone };

IStream *pf_noseek_stream(const void *data, size_t len) {
    noseek_t *n = calloc(1, sizeof *n);
    if (!n) return NULL;
    n->iface.lpVtbl = &ns_vtbl; n->ref = 1; n->len = len; n->data = malloc(len ? len : 1);
    memcpy(n->data, data, len);
    return &n->iface;
}

IStream *pf_mem_stream(const void *data, size_t len) {
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, len ? len : 1);
    IStream *s = NULL; void *p;
    if (!h) return NULL;
    p = GlobalLock(h); memcpy(p, data, len); GlobalUnlock(h);
    if (FAILED(CreateStreamOnHGlobal(h, TRUE, &s))) { GlobalFree(h); return NULL; }
    return s;
}

size_t pf_wrap_lt(BYTE *out, const void *payload, DWORD len) {
    memcpy(out, "lt\0\0", 4);
    memcpy(out + 4, &len, 4);
    memcpy(out + 8, payload, len);
    return 8 + (size_t)len;
}

HRESULT pf_load(const void *data, size_t len, LONG lsize, IPicture **pic) {
    IStream *s = pf_mem_stream(data, len); HRESULT hr;
    *pic = NULL;
    if (!s) return E_OUTOFMEMORY;
    hr = U(OleLoadPicture)(s, lsize, TRUE, &IID_IPicture, (void **)pic);
    IStream_Release(s);
    return hr;
}

/* ---- GDI fixtures -------------------------------------------------------------------------- */
HBITMAP pf_dib(int w, int h, int bpp, const COLORREF *px, void **bits_out) {
    BITMAPINFO bi; void *bits = NULL; HBITMAP b; int x, y;
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;   /* top-down */
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = (WORD)bpp; bi.bmiHeader.biCompression = BI_RGB;
    b = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!b) return NULL;
    if (px && bpp == 32) {
        DWORD *d = bits;
        for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
            COLORREF c = px[y * w + x];
            d[y * w + x] = (GetRValue(c) << 16) | (GetGValue(c) << 8) | GetBValue(c);
        }
    }
    if (bits_out) *bits_out = bits;
    return b;
}

IPicture *pf_pic_from_bitmap(HBITMAP bmp, HPALETTE pal, BOOL own) {
    PICTDESC d; IPicture *p = NULL;
    memset(&d, 0, sizeof d);
    d.cbSizeofstruct = sizeof d; d.picType = PICTYPE_BITMAP;
    d.bmp.hbitmap = bmp; d.bmp.hpal = pal;
    if (!U(OleCreatePictureIndirect)) return NULL;
    if (FAILED(U(OleCreatePictureIndirect)(&d, &IID_IPicture, own, (void **)&p))) return NULL;
    return p;
}

HDC pf_render_dc(int w, int h, HBITMAP *old, HBITMAP *dib_out) {
    HDC dc = CreateCompatibleDC(NULL);
    BITMAPINFO bi; void *bits; HBITMAP dib;
    if (!dc) return NULL;
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    dib = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!dib) { DeleteDC(dc); return NULL; }
    *old = SelectObject(dc, dib); *dib_out = dib;
    return dc;
}
void pf_render_dc_free(HDC dc, HBITMAP old, HBITMAP dib) {
    SelectObject(dc, old); DeleteObject(dib); DeleteDC(dc);
}

/* ICO/CUR with one 32bpp image. Pixel (x,y) = BGRA(x*40, y*40, 255, 255). */
size_t pf_build_ico(BYTE *out, int kind, int w, int h) {
    int maskrow = ((w + 31) / 32) * 4, xorsz = w * h * 4, andsz = maskrow * h;
    DWORD imgsz = 40 + xorsz + andsz;
    BYTE *p = out; int x, y;
    memset(out, 0, 22 + imgsz);
    *(WORD *)(p + 2) = (WORD)kind; *(WORD *)(p + 4) = 1;
    p[6] = (BYTE)w; p[7] = (BYTE)h;
    *(WORD *)(p + 10) = kind == 2 ? 0 : 1;          /* planes or hotspot x */
    *(WORD *)(p + 12) = kind == 2 ? 0 : 32;         /* bitcount or hotspot y */
    *(DWORD *)(p + 14) = imgsz; *(DWORD *)(p + 18) = 22;
    p = out + 22;
    *(DWORD *)p = 40; *(LONG *)(p + 4) = w; *(LONG *)(p + 8) = h * 2;
    *(WORD *)(p + 12) = 1; *(WORD *)(p + 14) = 32; *(DWORD *)(p + 20) = xorsz + andsz;
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
        BYTE *px = p + 40 + ((h - 1 - y) * w + x) * 4;
        px[0] = (BYTE)(x * 40); px[1] = (BYTE)(y * 40); px[2] = 255; px[3] = 255;
    }
    return 22 + imgsz;
}
