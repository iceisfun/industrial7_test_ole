/* IBindCtx / CreateBindCtx.  Facts below come from the Win10 ole32 decompilation of
 * CreateBindCtx, CBindCtx::{Get,Set}BindOptions, RegisterObjectParam, GetObjectParam,
 * RevokeObjectParam, EnumObjectParam, (Register|Revoke|Release)Bound* and GetRunningObjectTable. */
#include "mr.h"

#define BIND_OPTS_SIZE  16
#define BIND_OPTS2_SIZE 40
#define BIND_OPTS3_SIZE 48

/* ------------------------------------------------------------------------ creation */
/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_bindctx_create_null_out) {
    REQUIRE(U(CreateBindCtx));
    EXPECT_EQ(U(CreateBindCtx)(0, NULL), E_INVALIDARG);
    EXPECT_EQ(U(CreateBindCtx)(1, NULL), E_INVALIDARG);
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_bindctx_create_reserved_values) {
    static const DWORD bad[] = { 2, 3, 4, 0x10, 0xdeadbeef, 0x80000000, 0xfffffffe, 0xffffffff };
    IBindCtx *sentinel = (IBindCtx *)(ULONG_PTR)0x1234;
    REQUIRE(U(CreateBindCtx));
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        IBindCtx *b = sentinel;
        HRESULT hr = U(CreateBindCtx)(bad[i], &b);
        CHECKF(hr == E_INVALIDARG, "reserved %#lx: hr=%08lx", bad[i], hr);
        CHECKF(b == sentinel, "reserved %#lx: output pointer must be left untouched", bad[i]);
    }
}

TEST(mr_bindctx_create_flag_bit0_is_accepted) {
    /* Win10: only bits 1..31 of the reserved argument are rejected (decomp: (flags & 0xfffffffe) == 0).
     * Bit 0 requests a bind context that uses the "trusted ROT only" view. */
    IBindCtx *b = NULL;
    REQUIRE(U(CreateBindCtx));
    HRESULT hr = U(CreateBindCtx)(1, &b);
    EXPECT_EQ(hr, S_OK);
    CHECK(b != NULL);
    if (b) IBindCtx_Release(b);
}

TEST(mr_bindctx_create_without_com_init) {
    /* CreateBindCtx does not need CoInitialize; it only allocates. */
    REQUIRE(U(CreateBindCtx));
    struct { HRESULT hr; IBindCtx *b; } r = { E_FAIL, NULL };
    void run(void *p) { typeof(r) *q = p; q->hr = U(CreateBindCtx)(0, &q->b); }
    if (!mr_run_thread(run, &r, 10000)) return;
    EXPECT_EQ(r.hr, S_OK);
    if (r.b) IBindCtx_Release(r.b);
}

TEST(mr_bindctx_refcount_and_qi) {
    IBindCtx *b = NULL; void *p;
    REQUIRE(U(CreateBindCtx));
    EXPECT_EQ(U(CreateBindCtx)(0, &b), S_OK);
    if (!b) return;
    EXPECT_EQ(IBindCtx_AddRef(b), 2);
    EXPECT_EQ(IBindCtx_Release(b), 1);
    p = (void *)1;
    EXPECT_EQ(IBindCtx_QueryInterface(b, &mr_IID_IUnknown, &p), S_OK);
    EXPECT_EQ(p, b);
    EXPECT_EQ(IBindCtx_Release(b), 1);
    p = (void *)1;
    EXPECT_EQ(IBindCtx_QueryInterface(b, &mr_IID_IBindCtx, &p), S_OK);
    EXPECT_EQ(p, b);
    EXPECT_EQ(IBindCtx_Release(b), 1);
    p = (void *)1;
    EXPECT_EQ(IBindCtx_QueryInterface(b, &mr_IID_IMoniker, &p), E_NOINTERFACE);
    EXPECT_EQ(p, NULL);
    p = (void *)1;
    EXPECT_EQ(IBindCtx_QueryInterface(b, &mr_IID_IDispatch, &p), E_NOINTERFACE);
    EXPECT_EQ(p, NULL);
    EXPECT_EQ(IBindCtx_Release(b), 0);
}

/* ---------------------------------------------------------------------- bind options */
TEST(mr_bindctx_default_bind_options) {
    /* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+); defaults confirmed in CreateBindCtx decomp:
     * cbStruct 0x30, grfMode 2, dwClassContext 0x15, locale = thread locale */
    IBindCtx *b; BIND_OPTS3 o;
    REQUIRE(U(CreateBindCtx));
    b = mr_bc(); if (!b) return;
    memset(&o, 0xfe, sizeof o);
    o.cbStruct = sizeof o;
    EXPECT_EQ(IBindCtx_GetBindOptions(b, (BIND_OPTS *)&o), S_OK);
    EXPECT_EQ(sizeof(BIND_OPTS3), BIND_OPTS3_SIZE);
    EXPECT_EQ(o.cbStruct, BIND_OPTS3_SIZE);
    EXPECT_EQ(o.grfFlags, 0);
    EXPECT_EQ(o.grfMode, STGM_READWRITE);
    EXPECT_EQ(o.dwTickCountDeadline, 0);
    EXPECT_EQ(o.dwTrackFlags, 0);
    EXPECT_EQ(o.dwClassContext, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER | CLSCTX_REMOTE_SERVER);
    EXPECT_EQ(o.locale, GetThreadLocale());
    EXPECT_EQ(o.pServerInfo, NULL);
    EXPECT_EQ(o.hwnd, NULL);
    IBindCtx_Release(b);
}

TEST(mr_bindctx_get_options_cbstruct_variants) {
    static const struct { DWORD in; DWORD out; } t[] = {
        { BIND_OPTS_SIZE, BIND_OPTS_SIZE }, { BIND_OPTS2_SIZE, BIND_OPTS2_SIZE },
        { BIND_OPTS3_SIZE, BIND_OPTS3_SIZE }, { 0xffffffff, BIND_OPTS3_SIZE }, { 100, BIND_OPTS3_SIZE },
        { 49, BIND_OPTS3_SIZE },
    };
    IBindCtx *b;
    REQUIRE(U(CreateBindCtx));
    b = mr_bc(); if (!b) return;
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        BYTE raw[160]; BIND_OPTS *o = (BIND_OPTS *)raw;
        memset(raw, 0xfe, sizeof raw);
        o->cbStruct = t[i].in;
        HRESULT hr = IBindCtx_GetBindOptions(b, o);
        CHECKF(hr == S_OK, "in=%lu hr=%08lx", t[i].in, hr);
        CHECKF(o->cbStruct == t[i].out, "in=%lu cbStruct out=%lu want %lu", t[i].in, o->cbStruct, t[i].out);
        CHECKF(o->grfMode == STGM_READWRITE, "in=%lu grfMode=%lx", t[i].in, o->grfMode);
        /* bytes past the stored size are never touched */
        for (unsigned k = BIND_OPTS3_SIZE; k < sizeof raw; k++)
            if (raw[k] != 0xfe) { CHECKF(0, "in=%lu byte %u modified", t[i].in, k); break; }
        if (t[i].in == BIND_OPTS_SIZE)
            for (unsigned k = BIND_OPTS_SIZE; k < BIND_OPTS3_SIZE; k++)
                if (raw[k] != 0xfe) { CHECKF(0, "cbStruct=16: byte %u modified", k); break; }
    }
    IBindCtx_Release(b);
}

TEST(mr_bindctx_get_options_cbstruct_zero_and_null) {
    /* decomp: cbStruct 0 copies nothing and succeeds; a NULL pointer is E_INVALIDARG */
    IBindCtx *b; BYTE raw[64];
    REQUIRE(U(CreateBindCtx));
    b = mr_bc(); if (!b) return;
    memset(raw, 0x5a, sizeof raw);
    ((BIND_OPTS *)raw)->cbStruct = 0;
    EXPECT_EQ(IBindCtx_GetBindOptions(b, (BIND_OPTS *)raw), S_OK);
    for (unsigned k = 4; k < sizeof raw; k++) CHECKF(raw[k] == 0x5a, "byte %u modified", k);
    EXPECT_EQ(IBindCtx_GetBindOptions(b, NULL), E_INVALIDARG);
    IBindCtx_Release(b);
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_bindctx_set_options_rejects_oversize) {
    IBindCtx *b; BIND_OPTS3 o;
    REQUIRE(U(CreateBindCtx));
    b = mr_bc(); if (!b) return;
    memset(&o, 0, sizeof o);
    o.cbStruct = 0xffffffff;
    EXPECT_EQ(IBindCtx_SetBindOptions(b, (BIND_OPTS *)&o), E_INVALIDARG);
    o.cbStruct = BIND_OPTS3_SIZE + 1;
    EXPECT_EQ(IBindCtx_SetBindOptions(b, (BIND_OPTS *)&o), E_INVALIDARG);
    o.cbStruct = 0x1000;
    EXPECT_EQ(IBindCtx_SetBindOptions(b, (BIND_OPTS *)&o), E_INVALIDARG);
    EXPECT_EQ(IBindCtx_SetBindOptions(b, NULL), E_INVALIDARG);
    /* a rejected Set must not disturb the stored options */
    memset(&o, 0, sizeof o); o.cbStruct = sizeof o;
    EXPECT_EQ(IBindCtx_GetBindOptions(b, (BIND_OPTS *)&o), S_OK);
    EXPECT_EQ(o.cbStruct, BIND_OPTS3_SIZE);
    EXPECT_EQ(o.grfMode, STGM_READWRITE);
    IBindCtx_Release(b);
}

TEST(mr_bindctx_set_options_full_roundtrip) {
    IBindCtx *b; BIND_OPTS3 s, g;
    REQUIRE(U(CreateBindCtx));
    b = mr_bc(); if (!b) return;
    memset(&s, 0, sizeof s);
    s.cbStruct = sizeof s; s.grfFlags = BIND_MAYBOTHERUSER; s.grfMode = STGM_READ | STGM_SHARE_DENY_WRITE;
    s.dwTickCountDeadline = 0x1234; s.dwTrackFlags = 7; s.dwClassContext = CLSCTX_INPROC_SERVER;
    s.locale = 0x0409; s.pServerInfo = NULL; s.hwnd = (HWND)(ULONG_PTR)0x10;
    EXPECT_EQ(IBindCtx_SetBindOptions(b, (BIND_OPTS *)&s), S_OK);
    memset(&g, 0xcc, sizeof g); g.cbStruct = sizeof g;
    EXPECT_EQ(IBindCtx_GetBindOptions(b, (BIND_OPTS *)&g), S_OK);
    EXPECT_EQ(g.cbStruct, BIND_OPTS3_SIZE);
    EXPECT_EQ(g.grfFlags, BIND_MAYBOTHERUSER);
    EXPECT_EQ(g.grfMode, STGM_READ | STGM_SHARE_DENY_WRITE);
    EXPECT_EQ(g.dwTickCountDeadline, 0x1234);
    EXPECT_EQ(g.dwTrackFlags, 7);
    EXPECT_EQ(g.dwClassContext, CLSCTX_INPROC_SERVER);
    EXPECT_EQ(g.locale, 0x0409);
    EXPECT_EQ(g.hwnd, (HWND)(ULONG_PTR)0x10);
    IBindCtx_Release(b);
}

TEST(mr_bindctx_set_options_shrinks_stored_size) {
    /* decomp: Set memcpy()s cbStruct bytes -- including the cbStruct member itself -- over the stored
     * options, so a later Get reports the SMALLER size and fills only that many bytes. */
    static const DWORD sizes[] = { BIND_OPTS_SIZE, BIND_OPTS2_SIZE };
    IBindCtx *b;
    REQUIRE(U(CreateBindCtx));
    b = mr_bc(); if (!b) return;
    for (unsigned i = 0; i < 2; i++) {
        BIND_OPTS3 s, g; BYTE raw[BIND_OPTS3_SIZE];
        memset(&s, 0, sizeof s);
        s.cbStruct = sizes[i]; s.grfMode = STGM_READ; s.grfFlags = 0x40;
        EXPECT_EQ(IBindCtx_SetBindOptions(b, (BIND_OPTS *)&s), S_OK);
        memset(raw, 0xee, sizeof raw);
        ((BIND_OPTS *)raw)->cbStruct = BIND_OPTS3_SIZE;
        EXPECT_EQ(IBindCtx_GetBindOptions(b, (BIND_OPTS *)raw), S_OK);
        memcpy(&g, raw, sizeof g);
        CHECKF(g.cbStruct == sizes[i], "stored size %lu reported as %lu", sizes[i], g.cbStruct);
        CHECKF(g.grfMode == STGM_READ && g.grfFlags == 0x40, "fields mode=%lx flags=%lx", g.grfMode, g.grfFlags);
        for (unsigned k = sizes[i]; k < BIND_OPTS3_SIZE; k++)
            if (raw[k] != 0xee) { CHECKF(0, "stored %lu: byte %u written", sizes[i], k); break; }
        /* restoring a full BIND_OPTS3 brings the full size back */
        memset(&s, 0, sizeof s); s.cbStruct = BIND_OPTS3_SIZE; s.grfMode = STGM_READWRITE;
        EXPECT_EQ(IBindCtx_SetBindOptions(b, (BIND_OPTS *)&s), S_OK);
        memset(&g, 0, sizeof g); g.cbStruct = BIND_OPTS3_SIZE;
        EXPECT_EQ(IBindCtx_GetBindOptions(b, (BIND_OPTS *)&g), S_OK);
        EXPECT_EQ(g.cbStruct, BIND_OPTS3_SIZE);
    }
    IBindCtx_Release(b);
}

TEST(mr_bindctx_get_options_after_small_set_with_small_request) {
    IBindCtx *b; BIND_OPTS o, s;
    REQUIRE(U(CreateBindCtx));
    b = mr_bc(); if (!b) return;
    memset(&s, 0, sizeof s);
    s.cbStruct = BIND_OPTS_SIZE; s.grfFlags = 1; s.grfMode = STGM_READ; s.dwTickCountDeadline = 99;
    EXPECT_EQ(IBindCtx_SetBindOptions(b, &s), S_OK);
    memset(&o, 0, sizeof o); o.cbStruct = BIND_OPTS_SIZE;
    EXPECT_EQ(IBindCtx_GetBindOptions(b, &o), S_OK);
    EXPECT_EQ(o.cbStruct, BIND_OPTS_SIZE);
    EXPECT_EQ(o.grfFlags, 1);
    EXPECT_EQ(o.grfMode, STGM_READ);
    EXPECT_EQ(o.dwTickCountDeadline, 99);
    IBindCtx_Release(b);
}

TEST(mr_bindctx_options_are_per_instance) {
    IBindCtx *a, *b; BIND_OPTS s, g;
    REQUIRE(U(CreateBindCtx));
    a = mr_bc(); b = mr_bc(); if (!a || !b) return;
    memset(&s, 0, sizeof s); s.cbStruct = sizeof s; s.grfMode = STGM_READ;
    EXPECT_EQ(IBindCtx_SetBindOptions(a, &s), S_OK);
    memset(&g, 0, sizeof g); g.cbStruct = sizeof g;
    EXPECT_EQ(IBindCtx_GetBindOptions(b, &g), S_OK);
    EXPECT_EQ(g.grfMode, STGM_READWRITE);
    IBindCtx_Release(a); IBindCtx_Release(b);
}

/* ------------------------------------------------------------------ object params */
/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_bindctx_objparam_null_object) {
    IBindCtx *b;
    REQUIRE(U(CreateBindCtx));
    b = mr_bc(); if (!b) return;
    EXPECT_EQ(IBindCtx_RegisterObjectParam(b, (LPOLESTR)L"Gemma", NULL), E_INVALIDARG);
    IUnknown *u = (IUnknown *)1;
    EXPECT_EQ(IBindCtx_GetObjectParam(b, (LPOLESTR)L"Gemma", &u), E_FAIL);
    EXPECT_EQ(u, NULL);
    IBindCtx_Release(b);
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_bindctx_objparam_register_get_revoke) {
    IBindCtx *b; mr_obj o; IUnknown *u;
    REQUIRE(U(CreateBindCtx));
    b = mr_bc(); if (!b) return;
    mr_obj_init(&o);
    EXPECT_EQ(IBindCtx_RegisterObjectParam(b, (LPOLESTR)L"Gemma", mr_obj_unk(&o)), S_OK);
    EXPECT_EQ(o.ref, 2);                       /* the bind context owns a reference */
    u = NULL;
    EXPECT_EQ(IBindCtx_GetObjectParam(b, (LPOLESTR)L"Gemma", &u), S_OK);
    EXPECT_EQ(u, mr_obj_unk(&o));
    EXPECT_EQ(o.ref, 3);                       /* Get AddRefs the result */
    IUnknown_Release(u);
    EXPECT_EQ(o.ref, 2);
    EXPECT_EQ(IBindCtx_RevokeObjectParam(b, (LPOLESTR)L"Gemma"), S_OK);
    EXPECT_EQ(o.ref, 1);
    u = (IUnknown *)1;
    EXPECT_EQ(IBindCtx_GetObjectParam(b, (LPOLESTR)L"Gemma", &u), E_FAIL);
    EXPECT_EQ(u, NULL);
    EXPECT_EQ(IBindCtx_RevokeObjectParam(b, (LPOLESTR)L"Gemma"), E_FAIL);
    IBindCtx_Release(b);
    EXPECT_EQ(o.ref, 1);
}

TEST(mr_bindctx_objparam_released_with_bindctx) {
    IBindCtx *b; mr_obj o;
    REQUIRE(U(CreateBindCtx));
    b = mr_bc(); if (!b) return;
    mr_obj_init(&o);
    EXPECT_EQ(IBindCtx_RegisterObjectParam(b, (LPOLESTR)L"P1", mr_obj_unk(&o)), S_OK);
    EXPECT_EQ(o.ref, 2);
    EXPECT_EQ(IBindCtx_Release(b), 0);
    EXPECT_EQ(o.ref, 1);
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_bindctx_objparam_missing_key) {
    IBindCtx *b; IUnknown *u = (IUnknown *)1;
    REQUIRE(U(CreateBindCtx));
    b = mr_bc(); if (!b) return;
    EXPECT_EQ(IBindCtx_GetObjectParam(b, (LPOLESTR)L"NonExistent", &u), E_FAIL);
    EXPECT_EQ(u, NULL);
    EXPECT_EQ(IBindCtx_RevokeObjectParam(b, (LPOLESTR)L"NonExistent"), E_FAIL);
    EXPECT_EQ(IBindCtx_GetObjectParam(b, (LPOLESTR)L"NonExistent", NULL), E_INVALIDARG);
    IBindCtx_Release(b);
}

TEST(mr_bindctx_objparam_replace_same_key) {
    IBindCtx *b; mr_obj o1, o2; IUnknown *u;
    REQUIRE(U(CreateBindCtx));
    b = mr_bc(); if (!b) return;
    mr_obj_init(&o1); mr_obj_init(&o2);
    EXPECT_EQ(IBindCtx_RegisterObjectParam(b, (LPOLESTR)L"Key", mr_obj_unk(&o1)), S_OK);
    EXPECT_EQ(IBindCtx_RegisterObjectParam(b, (LPOLESTR)L"Key", mr_obj_unk(&o2)), S_OK);
    EXPECT_EQ(o1.ref, 1);                      /* replaced entry is released */
    EXPECT_EQ(o2.ref, 2);
    u = NULL;
    EXPECT_EQ(IBindCtx_GetObjectParam(b, (LPOLESTR)L"Key", &u), S_OK);
    EXPECT_EQ(u, mr_obj_unk(&o2));
    if (u) IUnknown_Release(u);
    /* registering the very same object again under the same key must not leak or over-release */
    EXPECT_EQ(IBindCtx_RegisterObjectParam(b, (LPOLESTR)L"Key", mr_obj_unk(&o2)), S_OK);
    EXPECT_EQ(o2.ref, 2);
    IBindCtx_Release(b);
    EXPECT_EQ(o2.ref, 1);
}

TEST(mr_bindctx_objparam_keys_case_sensitive) {
    /* decomp: keys are hashed and compared with memcmp on the wide string, so they are case sensitive */
    IBindCtx *b; mr_obj o1, o2; IUnknown *u;
    REQUIRE(U(CreateBindCtx));
    b = mr_bc(); if (!b) return;
    mr_obj_init(&o1); mr_obj_init(&o2);
    EXPECT_EQ(IBindCtx_RegisterObjectParam(b, (LPOLESTR)L"Key", mr_obj_unk(&o1)), S_OK);
    EXPECT_EQ(IBindCtx_RegisterObjectParam(b, (LPOLESTR)L"key", mr_obj_unk(&o2)), S_OK);
    EXPECT_EQ(o1.ref, 2);
    EXPECT_EQ(o2.ref, 2);
    u = NULL;
    EXPECT_EQ(IBindCtx_GetObjectParam(b, (LPOLESTR)L"KEY", &u), E_FAIL);
    EXPECT_EQ(IBindCtx_GetObjectParam(b, (LPOLESTR)L"key", &u), S_OK);
    EXPECT_EQ(u, mr_obj_unk(&o2));
    if (u) IUnknown_Release(u);
    EXPECT_EQ(IBindCtx_RevokeObjectParam(b, (LPOLESTR)L"KEY"), E_FAIL);
    EXPECT_EQ(IBindCtx_RevokeObjectParam(b, (LPOLESTR)L"Key"), S_OK);
    EXPECT_EQ(o1.ref, 1);
    EXPECT_EQ(o2.ref, 2);
    IBindCtx_Release(b);
}

TEST(mr_bindctx_objparam_key_prefix_is_not_a_match) {
    IBindCtx *b; mr_obj o; IUnknown *u = NULL;
    REQUIRE(U(CreateBindCtx));
    b = mr_bc(); if (!b) return;
    mr_obj_init(&o);
    EXPECT_EQ(IBindCtx_RegisterObjectParam(b, (LPOLESTR)L"ConnectManually", mr_obj_unk(&o)), S_OK);
    EXPECT_EQ(IBindCtx_GetObjectParam(b, (LPOLESTR)L"Connect", &u), E_FAIL);
    EXPECT_EQ(IBindCtx_GetObjectParam(b, (LPOLESTR)L"ConnectManuallyX", &u), E_FAIL);
    EXPECT_EQ(IBindCtx_GetObjectParam(b, (LPOLESTR)L"", &u), E_FAIL);
    EXPECT_EQ(IBindCtx_GetObjectParam(b, (LPOLESTR)L"ConnectManually", &u), S_OK);
    if (u) IUnknown_Release(u);
    IBindCtx_Release(b);
}

TEST(mr_bindctx_objparam_empty_and_name_keys) {
    /* the task's documented special keys are ordinary strings to the bind context */
    static const WCHAR *keys[] = { L"", L"Name", L"ExceededDeadline", L"ConnectManually", L"ClassMoniker", L"é中" };
    IBindCtx *b; mr_obj o[6];
    REQUIRE(U(CreateBindCtx));
    b = mr_bc(); if (!b) return;
    for (int i = 0; i < 6; i++) {
        mr_obj_init(&o[i]);
        HRESULT hr = IBindCtx_RegisterObjectParam(b, (LPOLESTR)keys[i], mr_obj_unk(&o[i]));
        CHECKF(hr == S_OK, "register key %d hr=%08lx", i, hr);
    }
    for (int i = 0; i < 6; i++) {
        IUnknown *u = NULL;
        HRESULT hr = IBindCtx_GetObjectParam(b, (LPOLESTR)keys[i], &u);
        CHECKF(hr == S_OK && u == mr_obj_unk(&o[i]), "get key %d hr=%08lx u=%p", i, hr, u);
        if (u) IUnknown_Release(u);
    }
    for (int i = 0; i < 6; i++) {
        HRESULT hr = IBindCtx_RevokeObjectParam(b, (LPOLESTR)keys[i]);
        CHECKF(hr == S_OK, "revoke key %d hr=%08lx", i, hr);
        CHECKF(o[i].ref == 1, "key %d ref=%ld", i, o[i].ref);
    }
    IBindCtx_Release(b);
}

TEST(mr_bindctx_objparam_many_keys) {
    IBindCtx *b; static mr_obj o[200]; WCHAR k[32];
    REQUIRE(U(CreateBindCtx));
    b = mr_bc(); if (!b) return;
    for (int i = 0; i < 200; i++) {
        mr_obj_init(&o[i]); wsprintfW(k, L"param%d", i);
        HRESULT hr = IBindCtx_RegisterObjectParam(b, k, mr_obj_unk(&o[i]));
        if (hr != S_OK) { CHECKF(0, "register %d hr=%08lx", i, hr); break; }
    }
    for (int i = 199; i >= 0; i--) {
        IUnknown *u = NULL; wsprintfW(k, L"param%d", i);
        HRESULT hr = IBindCtx_GetObjectParam(b, k, &u);
        if (!(hr == S_OK && u == mr_obj_unk(&o[i]))) { CHECKF(0, "get %d hr=%08lx", i, hr); break; }
        IUnknown_Release(u);
    }
    for (int i = 0; i < 200; i += 2) { wsprintfW(k, L"param%d", i); if (IBindCtx_RevokeObjectParam(b, k) != S_OK) { CHECKF(0, "revoke %d", i); break; } }
    for (int i = 0; i < 200; i++) {
        IUnknown *u = NULL; wsprintfW(k, L"param%d", i);
        HRESULT hr = IBindCtx_GetObjectParam(b, k, &u);
        if (u) IUnknown_Release(u);
        if (hr != ((i & 1) ? S_OK : E_FAIL)) { CHECKF(0, "after revoke key %d hr=%08lx", i, hr); break; }
    }
    IBindCtx_Release(b);
    for (int i = 0; i < 200; i++) if (o[i].ref != 1) { CHECKF(0, "obj %d ref=%ld after release", i, o[i].ref); break; }
}

TEST(mr_bindctx_objparam_long_key) {
    IBindCtx *b; mr_obj o; WCHAR k[1200]; IUnknown *u = NULL;
    REQUIRE(U(CreateBindCtx));
    b = mr_bc(); if (!b) return;
    for (int i = 0; i < 1199; i++) k[i] = (WCHAR)('a' + (i % 26));
    k[1199] = 0;
    mr_obj_init(&o);
    EXPECT_EQ(IBindCtx_RegisterObjectParam(b, k, mr_obj_unk(&o)), S_OK);
    EXPECT_EQ(IBindCtx_GetObjectParam(b, k, &u), S_OK);
    EXPECT_EQ(u, mr_obj_unk(&o));
    if (u) IUnknown_Release(u);
    k[1198] = L'!';
    u = NULL;
    EXPECT_EQ(IBindCtx_GetObjectParam(b, k, &u), E_FAIL);
    IBindCtx_Release(b);
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_bindctx_enum_object_param) {
    /* CBindCtx::EnumObjectParam is not implemented on Windows 10: it clears the out pointer and
     * returns E_NOTIMPL (shares its body with CCompositeMonikerEnum::Clone). */
    IBindCtx *b; IEnumString *e = (IEnumString *)1; mr_obj o;
    REQUIRE(U(CreateBindCtx));
    b = mr_bc(); if (!b) return;
    EXPECT_EQ(IBindCtx_EnumObjectParam(b, &e), E_NOTIMPL);
    EXPECT_EQ(e, NULL);
    mr_obj_init(&o);
    EXPECT_EQ(IBindCtx_RegisterObjectParam(b, (LPOLESTR)L"x", mr_obj_unk(&o)), S_OK);
    e = (IEnumString *)1;
    EXPECT_EQ(IBindCtx_EnumObjectParam(b, &e), E_NOTIMPL);
    EXPECT_EQ(e, NULL);
    if (t_is_wine()) t_skip("Wine dereferences a NULL out pointer here");
    else EXPECT_EQ(IBindCtx_EnumObjectParam(b, NULL), E_INVALIDARG);
    IBindCtx_Release(b);
}

/* ----------------------------------------------------------------- bound objects */
/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_bindctx_bound_register_revoke) {
    IBindCtx *b; mr_obj o;
    REQUIRE(U(CreateBindCtx));
    b = mr_bc(); if (!b) return;
    mr_obj_init(&o);
    EXPECT_EQ(IBindCtx_RegisterObjectBound(b, NULL), S_OK);        /* NULL is a silent no-op */
    EXPECT_EQ(IBindCtx_RevokeObjectBound(b, NULL), E_INVALIDARG);
    EXPECT_EQ(IBindCtx_RevokeObjectBound(b, mr_obj_unk(&o)), MK_E_NOTBOUND);
    EXPECT_EQ(IBindCtx_RegisterObjectBound(b, mr_obj_unk(&o)), S_OK);
    EXPECT_EQ(o.ref, 2);
    EXPECT_EQ(IBindCtx_RevokeObjectBound(b, mr_obj_unk(&o)), S_OK);
    EXPECT_EQ(o.ref, 1);
    EXPECT_EQ(IBindCtx_RevokeObjectBound(b, mr_obj_unk(&o)), MK_E_NOTBOUND);
    IBindCtx_Release(b);
    EXPECT_EQ(o.ref, 1);
}

TEST(mr_bindctx_bound_duplicates_are_counted) {
    /* each RegisterObjectBound adds one list entry and one reference; Revoke removes one entry */
    IBindCtx *b; mr_obj o;
    REQUIRE(U(CreateBindCtx));
    b = mr_bc(); if (!b) return;
    mr_obj_init(&o);
    EXPECT_EQ(IBindCtx_RegisterObjectBound(b, mr_obj_unk(&o)), S_OK);
    EXPECT_EQ(IBindCtx_RegisterObjectBound(b, mr_obj_unk(&o)), S_OK);
    EXPECT_EQ(IBindCtx_RegisterObjectBound(b, mr_obj_unk(&o)), S_OK);
    EXPECT_EQ(o.ref, 4);
    EXPECT_EQ(IBindCtx_RevokeObjectBound(b, mr_obj_unk(&o)), S_OK);
    EXPECT_EQ(o.ref, 3);
    EXPECT_EQ(IBindCtx_RevokeObjectBound(b, mr_obj_unk(&o)), S_OK);
    EXPECT_EQ(IBindCtx_RevokeObjectBound(b, mr_obj_unk(&o)), S_OK);
    EXPECT_EQ(o.ref, 1);
    EXPECT_EQ(IBindCtx_RevokeObjectBound(b, mr_obj_unk(&o)), MK_E_NOTBOUND);
    IBindCtx_Release(b);
}

TEST(mr_bindctx_release_bound_objects) {
    IBindCtx *b; mr_obj o[3];
    REQUIRE(U(CreateBindCtx));
    b = mr_bc(); if (!b) return;
    for (int i = 0; i < 3; i++) { mr_obj_init(&o[i]); EXPECT_EQ(IBindCtx_RegisterObjectBound(b, mr_obj_unk(&o[i])), S_OK); }
    for (int i = 0; i < 3; i++) EXPECT_EQ(o[i].ref, 2);
    EXPECT_EQ(IBindCtx_ReleaseBoundObjects(b), S_OK);
    for (int i = 0; i < 3; i++) EXPECT_EQ(o[i].ref, 1);
    for (int i = 0; i < 3; i++) EXPECT_EQ(IBindCtx_RevokeObjectBound(b, mr_obj_unk(&o[i])), MK_E_NOTBOUND);
    EXPECT_EQ(IBindCtx_ReleaseBoundObjects(b), S_OK);     /* idempotent on an empty list */
    IBindCtx_Release(b);
}

TEST(mr_bindctx_bound_released_with_bindctx) {
    IBindCtx *b; mr_obj o1, o2;
    REQUIRE(U(CreateBindCtx));
    b = mr_bc(); if (!b) return;
    mr_obj_init(&o1); mr_obj_init(&o2);
    IBindCtx_RegisterObjectBound(b, mr_obj_unk(&o1));
    IBindCtx_RegisterObjectBound(b, mr_obj_unk(&o2));
    IBindCtx_RegisterObjectBound(b, mr_obj_unk(&o1));
    EXPECT_EQ(o1.ref, 3);
    EXPECT_EQ(IBindCtx_Release(b), 0);
    EXPECT_EQ(o1.ref, 1);
    EXPECT_EQ(o2.ref, 1);
}

TEST(mr_bindctx_bound_and_param_are_independent) {
    IBindCtx *b; mr_obj o; IUnknown *u = NULL;
    REQUIRE(U(CreateBindCtx));
    b = mr_bc(); if (!b) return;
    mr_obj_init(&o);
    IBindCtx_RegisterObjectParam(b, (LPOLESTR)L"P", mr_obj_unk(&o));
    IBindCtx_RegisterObjectBound(b, mr_obj_unk(&o));
    EXPECT_EQ(o.ref, 3);
    IBindCtx_ReleaseBoundObjects(b);
    EXPECT_EQ(o.ref, 2);
    EXPECT_EQ(IBindCtx_GetObjectParam(b, (LPOLESTR)L"P", &u), S_OK);
    if (u) IUnknown_Release(u);
    IBindCtx_Release(b);
    EXPECT_EQ(o.ref, 1);
}

/* ------------------------------------------------------------- running object table */
/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_bindctx_get_rot_matches_global_rot) {
    IBindCtx *b; IRunningObjectTable *r1 = NULL, *r2 = NULL;
    MR_NEED_BASE();
    MR_COM_BEGIN();
    b = mr_bc();
    if (b) {
        EXPECT_EQ(IBindCtx_GetRunningObjectTable(b, &r1), S_OK);
        EXPECT_EQ(U(GetRunningObjectTable)(0, &r2), S_OK);
        EXPECT_EQ(r1, r2);
        CHECK(r1 != NULL);
        if (r1) IRunningObjectTable_Release(r1);
        if (r2) IRunningObjectTable_Release(r2);
        EXPECT_EQ(IBindCtx_GetRunningObjectTable(b, NULL), E_INVALIDARG);
        IBindCtx_Release(b);
    }
    MR_COM_END();
}

TEST(mr_bindctx_get_rot_trusted_flag_selects_filtered_rot) {
    /* decomp: CBindCtx::GetRunningObjectTable forwards (m_trustedRotOnly) as the flag and
     * GetRunningObjectTable(flags) returns &FilteredROT when bit 0 is set, pROT otherwise. */
    IBindCtx *b0, *b1 = NULL; IRunningObjectTable *plain = NULL, *filt = NULL, *g1 = NULL, *r = NULL;
    MR_NEED_BASE();
    MR_COM_BEGIN();
    b0 = mr_bc();
    EXPECT_EQ(U(CreateBindCtx)(1, &b1), S_OK);
    EXPECT_EQ(U(GetRunningObjectTable)(0, &plain), S_OK);
    EXPECT_EQ(U(GetRunningObjectTable)(1, &g1), S_OK);
    CHECK(plain != NULL && g1 != NULL);
    CHECK(plain != g1);
    if (b1) {
        EXPECT_EQ(IBindCtx_GetRunningObjectTable(b1, &filt), S_OK);
        EXPECT_EQ(filt, g1);
    }
    /* flag bits other than bit 0 are ignored by GetRunningObjectTable (no E_INVALIDARG on Win10) */
    EXPECT_EQ(U(GetRunningObjectTable)(0xfffffffe, &r), S_OK);
    EXPECT_EQ(r, plain);
    if (r) IRunningObjectTable_Release(r);
    if (plain) IRunningObjectTable_Release(plain);
    if (g1) IRunningObjectTable_Release(g1);
    if (filt) IRunningObjectTable_Release(filt);
    if (b1) IBindCtx_Release(b1);
    if (b0) IBindCtx_Release(b0);
    MR_COM_END();
}

TEST(mr_bindctx_get_rot_requires_com_init) {
    /* A thread whose apartment is not initialised gets CO_E_NOTINITIALIZED, both
     * from the global API and through a bind context. */
    struct { HRESULT hr1, hr2; IRunningObjectTable *r1, *r2; } r = { 0, 0, (void *)1, (void *)1 };
    void run(void *p) {
        typeof(r) *q = p; IBindCtx *b = NULL;
        q->hr1 = U(GetRunningObjectTable)(0, &q->r1);
        if (U(CreateBindCtx)(0, &b) == S_OK) { q->hr2 = IBindCtx_GetRunningObjectTable(b, &q->r2); IBindCtx_Release(b); }
    }
    REQUIRE(U(GetRunningObjectTable)); REQUIRE(U(CreateBindCtx));
    if (!mr_run_thread(run, &r, 10000)) return;
    EXPECT_EQ(r.hr1, CO_E_NOTINITIALIZED);
    EXPECT_EQ(r.r1, (void *)1);            /* decomp: the out pointer is only written once the apartment is initialised */
    EXPECT_EQ(r.hr2, CO_E_NOTINITIALIZED);
}

TEST(mr_bindctx_usable_from_second_thread) {
    /* a bind context is a plain in-process object: options set on one thread are visible on another */
    IBindCtx *b; BIND_OPTS s; struct { IBindCtx *b; DWORD mode; HRESULT hr; } r;
    void run(void *p) { typeof(r) *q = p; BIND_OPTS g; memset(&g, 0, sizeof g); g.cbStruct = sizeof g;
        q->hr = IBindCtx_GetBindOptions(q->b, &g); q->mode = g.grfMode; }
    REQUIRE(U(CreateBindCtx));
    b = mr_bc(); if (!b) return;
    memset(&s, 0, sizeof s); s.cbStruct = sizeof s; s.grfMode = STGM_READ | STGM_SHARE_EXCLUSIVE;
    EXPECT_EQ(IBindCtx_SetBindOptions(b, &s), S_OK);
    r.b = b; r.mode = 0; r.hr = E_FAIL;
    if (mr_run_thread(run, &r, 10000)) {
        EXPECT_EQ(r.hr, S_OK);
        EXPECT_EQ(r.mode, STGM_READ | STGM_SHARE_EXCLUSIVE);
    }
    IBindCtx_Release(b);
}
