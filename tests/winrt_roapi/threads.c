/* winrt_roapi: RoInitialize on several threads at once (bounded waits). */
#include "ro_common.h"

typedef struct { RO_INIT_TYPE type; HANDLE ready, go; HRESULT first, second; APTTYPE apt; HRESULT aptHr; } coexist_t;

static DWORD WINAPI coexist_thread(void *p) {
    coexist_t *c = p;
    c->first = U(RoInitialize)(c->type);
    c->aptHr = ro_apt(&c->apt, NULL);
    SetEvent(c->ready);
    WaitForSingleObject(c->go, 10000);
    c->second = U(RoInitialize)(c->type);
    if (SUCCEEDED(c->second)) U(RoUninitialize)();
    if (SUCCEEDED(c->first)) U(RoUninitialize)();
    return 0;
}

RO_TEST(winrt_roapi_threads_mta_and_sta_coexist) {
    REQUIRE(U(RoInitialize)); REQUIRE(U(RoUninitialize));
    coexist_t c[2] = { { RO_INIT_MULTITHREADED }, { RO_INIT_SINGLETHREADED } };
    HANDLE th[2] = { 0 };
    HANDLE go = CreateEventW(NULL, TRUE, FALSE, NULL);
    for (int i = 0; i < 2; i++) {
        c[i].ready = CreateEventW(NULL, FALSE, FALSE, NULL); c[i].go = go;
        th[i] = CreateThread(NULL, 0, coexist_thread, &c[i], 0, NULL);
    }
    for (int i = 0; i < 2; i++) CHECK(th[i] && WaitForSingleObject(c[i].ready, 10000) == WAIT_OBJECT_0);
    SetEvent(go);
    for (int i = 0; i < 2; i++) {
        if (!th[i]) continue;
        if (WaitForSingleObject(th[i], 10000) != WAIT_OBJECT_0) { t_note("thread %d did not finish", i); TerminateThread(th[i], 1); }
        CloseHandle(th[i]);
        CloseHandle(c[i].ready);
    }
    CloseHandle(go);
    for (int i = 0; i < 2; i++) {
        CHECKF(c[i].first == S_OK, "thread %d first RoInitialize = %08lx", i, (unsigned long)c[i].first);
        CHECKF(c[i].second == S_FALSE, "thread %d second RoInitialize = %08lx", i, (unsigned long)c[i].second);
        CHECKF(c[i].aptHr == S_OK, "thread %d CoGetApartmentType = %08lx", i, (unsigned long)c[i].aptHr);
    }
    CHECK(c[0].apt == APTTYPE_MTA);
}
