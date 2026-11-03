/* dx_gap_09: CLIPFORMAT_UserFree with registered (named) clipboard formats.
 * RegisterClipboardFormat adds a session-wide atom that is not removable, hence this lives in a dx_ area.
 * Names carry the olet prefix and the pid so repeated runs do not collide.
 * Win10 decompilation: CLIPFORMAT_UserFree (shared body with HRGN_UserFree) does nothing at all; in particular
 * it does not unregister a clipboard format and does not touch the name. */
#include "g9.h"

typedef UINT (WINAPI *regcf_t)(LPCWSTR);
typedef int (WINAPI *getcfn_t)(UINT, LPWSTR, int);

static UINT reg_named(const char *tag, WCHAR *name_out)
{
    regcf_t rc = (regcf_t)t_proc_in("user32.dll", "RegisterClipboardFormatW");
    char tmp[128]; int i;
    snprintf(tmp, sizeof tmp, "olet_gap9_%lu_%s", GetCurrentProcessId(), tag);
    for (i = 0; tmp[i]; i++) name_out[i] = (WCHAR)tmp[i];
    name_out[i] = 0;
    return rc ? rc(name_out) : 0;
}

TEST(dx_gap9_clipformat_userfree_keeps_registered_format)
{
    void *f = UM_FNIN("ole32.dll", "CLIPFORMAT_UserFree");
    getcfn_t gn = (getcfn_t)t_proc_in("user32.dll", "GetClipboardFormatNameW");
    regcf_t rc = (regcf_t)t_proc_in("user32.dll", "RegisterClipboardFormatW");
    WCHAR name[128], back[128]; UINT id; ULONG ctx;
    REQUIRE(f); REQUIRE(gn); REQUIRE(rc);
    id = reg_named("keep", name);
    if (!id) { t_skip("RegisterClipboardFormatW unavailable in this session"); return; }
    CHECKF(id >= 0xC000 && id <= 0xFFFF, "registered formats live in 0xC000..0xFFFF (got %#x)", id);
    for (ctx = 0; ctx <= C_CONT; ctx++) {
        CLIPFORMAT cf = (CLIPFORMAT)id; UMC c;
        um_init(&c, NULL, 0, ctx);
        XEQ(um_x_free(f, UMF(&c), &cf), 0);
        CHECKF(cf == id, "ctx %lu: value unchanged", ctx);
        memset(back, 0, sizeof back);
        CHECKF(gn(id, back, 128) == lstrlenW(name) && !lstrcmpW(back, name), "ctx %lu: name still registered", ctx);
        CHECKF(rc(name) == id, "ctx %lu: re-registering returns the same id", ctx);
    }
}

TEST(dx_gap9_clipformat_userfree_after_unmarshal_of_named_format)
{
    void *fu = UM_FNIN("ole32.dll", "CLIPFORMAT_UserUnmarshal"), *fm = UM_FNIN("ole32.dll", "CLIPFORMAT_UserMarshal");
    void *ff = UM_FNIN("ole32.dll", "CLIPFORMAT_UserFree"), *fs = UM_FNIN("ole32.dll", "CLIPFORMAT_UserSize");
    regcf_t rc = (regcf_t)t_proc_in("user32.dll", "RegisterClipboardFormatW");
    WCHAR name[128]; UINT id; UMC c; CLIPFORMAT cf, out = 0; unsigned char *b, *e; ULONG sz;
    REQUIRE(fu); REQUIRE(fm); REQUIRE(ff); REQUIRE(fs); REQUIRE(rc);
    id = reg_named("rt", name);
    if (!id) { t_skip("RegisterClipboardFormatW unavailable in this session"); return; }
    cf = (CLIPFORMAT)id;
    um_init(&c, NULL, 0, C_LOCAL);
    XEQ(um_x_size(fs, UMF(&c), 0, &cf, &sz), 0);
    b = um_buf_new(sz + 32);
    um_init(&c, b, sz + 32, C_LOCAL);
    XEQ(um_x_mar(fm, UMF(&c), b, &cf, &e), 0);
    um_init(&c, b, (ULONG)(e - b), C_LOCAL);
    XEQ(um_x_mar(fu, UMF(&c), b, &out, &e), 0);
    CHECKF(out == cf, "same process: the name resolves to the same id (%#x vs %#x)", out, cf);
    um_init(&c, NULL, 0, C_LOCAL);
    XEQ(um_x_free(ff, UMF(&c), &out), 0);
    CHECK(out == cf);
    CHECK(rc(name) == id);
    um_buf_free(b);
}

TEST(dx_gap9_clipformat_userfree_many_registered_formats)
{
    void *f = UM_FNIN("ole32.dll", "CLIPFORMAT_UserFree");
    regcf_t rc = (regcf_t)t_proc_in("user32.dll", "RegisterClipboardFormatW");
    int i;
    REQUIRE(f); REQUIRE(rc);
    for (i = 0; i < 20; i++) {
        WCHAR name[128]; char tag[16]; UINT id; CLIPFORMAT cf; UMC c;
        snprintf(tag, sizeof tag, "m%d", i);
        id = reg_named(tag, name);
        if (!id) { t_skip("RegisterClipboardFormatW unavailable in this session"); return; }
        cf = (CLIPFORMAT)id;
        um_init(&c, NULL, 0, C_DIFFM);
        XEQ(um_x_free(f, UMF(&c), &cf), 0);
        CHECK(rc(name) == id);
    }
}
