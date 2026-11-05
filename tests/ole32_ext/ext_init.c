/* OleInitializeWOW (ordinal 1245): the 16-bit thunk layer's entry is OleInitialize(NULL) with both of its arguments ignored. */
#include "ext.h"

typedef HRESULT (WINAPI *wow_t)(void *, void *);

static void sta_thread(void *unused)
{
    wow_t w = (wow_t)t_proc_in("ole32.dll", "OleInitializeWOW");
    if (!w) { t_skip("missing export: OleInitializeWOW"); return; }
    EQ(x_apt_type(), 0xffff);
    EQ(w(NULL, NULL), S_OK);                 /* the first count of the thread */
    CHECK(x_apt_type() != 0xffff);
    EQ(w(NULL, NULL), S_FALSE);              /* then S_FALSE, like OleInitialize */
    OleUninitialize(); OleUninitialize();
    EQ(x_apt_type(), 0xffff);                /* two counts, two uninitializes: the apartment is gone */
    EQ(w((void *)1, (void *)2), S_OK);       /* neither argument is looked at */
    OleUninitialize();
    EQ(w(NULL, (void *)(ULONG_PTR)0x111), S_OK);
    OleUninitialize();
    EQ(x_apt_type(), 0xffff);
}
TEST(oex_oleinitializewow_counts_like_oleinitialize) { x_on_thread(sta_thread, NULL); }

static void mta_thread(void *unused)
{
    wow_t w = (wow_t)t_proc_in("ole32.dll", "OleInitializeWOW");
    if (!w) { t_skip("missing export: OleInitializeWOW"); return; }
    EQ(CoInitializeEx(NULL, COINIT_MULTITHREADED), S_OK);
    EQ(w(NULL, NULL), X_RPC_E_CHANGED_MODE);   /* OLE needs an STA */
    EQ(w(NULL, NULL), X_RPC_E_CHANGED_MODE);   /* nothing was counted, so nothing to undo */
    EQ(OleInitialize(NULL), X_RPC_E_CHANGED_MODE);
    CoUninitialize();
    EQ(x_apt_type(), 0xffff);
}
TEST(oex_oleinitializewow_in_an_mta_is_changed_mode) { x_on_thread(mta_thread, NULL); }

static void costa_thread(void *unused)
{
    wow_t w = (wow_t)t_proc_in("ole32.dll", "OleInitializeWOW");
    if (!w) { t_skip("missing export: OleInitializeWOW"); return; }
    EQ(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED), S_OK);
    EQ(w(NULL, NULL), S_OK);                  /* the COM count was there already; the OLE count is the first */
    EQ(w(NULL, NULL), S_FALSE);
    OleUninitialize(); OleUninitialize();
    CHECK(x_apt_type() != 0xffff);            /* the CoInitializeEx is still outstanding */
    CoUninitialize();
    EQ(x_apt_type(), 0xffff);
}
TEST(oex_oleinitializewow_after_coinitializeex) { x_on_thread(costa_thread, NULL); }
