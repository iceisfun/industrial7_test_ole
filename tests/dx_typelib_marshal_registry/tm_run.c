/* Apartment / thread runner for cross-apartment marshaling tests. */
#include "tm.h"

const TMCfg tm_cfgs[3] = {
    { COINIT_APARTMENTTHREADED, COINIT_MULTITHREADED, "server STA / client MTA" },
    { COINIT_MULTITHREADED, COINIT_APARTMENTTHREADED, "server MTA / client STA" },
    { COINIT_APARTMENTTHREADED, COINIT_APARTMENTTHREADED, "server STA / client STA2" },
};
volatile LONG tm_client_hr;

void tm_pump_wait(HANDLE h, DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    for (;;) {
        DWORD now = GetTickCount(), left = (end - now) > ms ? 0 : end - now, r;
        MSG m;
        if (!left) return;
        r = MsgWaitForMultipleObjects(1, &h, FALSE, left, QS_ALLINPUT);
        if (r == WAIT_OBJECT_0 || r == WAIT_FAILED || r == WAIT_TIMEOUT) return;
        while (PeekMessageW(&m, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); }
    }
}

typedef struct {
    DWORD apt; IStream *stm; const GUID *iid; tm_client_fn fn; void *ctx; HANDLE done; volatile LONG finished;
} Job;

static DWORD WINAPI job_thread(void *p)
{
    Job *j = p; void *itf = NULL; HRESULT hr;
    hr = U(CoInitializeEx)(NULL, j->apt);
    if (FAILED(hr)) { tm_client_hr = hr; SetEvent(j->done); return 1; }
    hr = U(CoGetInterfaceAndReleaseStream)(j->stm, j->iid, &itf);
    j->stm = NULL;
    tm_client_hr = hr;
    if (SUCCEEDED(hr) && itf) {
        j->fn(itf, j->ctx);
        IUnknown_Release((IUnknown *)itf);
    } else if (j->fn) {
        j->fn(NULL, j->ctx);
    }
    j->finished = 1;
    U(CoUninitialize)();
    SetEvent(j->done);
    return 0;
}

int tm_run_thread(DWORD apt, IStream *stm, const GUID *iid, tm_client_fn fn, void *ctx, DWORD ms)
{
    Job j; HANDLE th; DWORD tid;
    memset(&j, 0, sizeof j);
    j.apt = apt; j.stm = stm; j.iid = iid; j.fn = fn; j.ctx = ctx; j.done = CreateEventW(NULL, TRUE, FALSE, NULL);
    th = CreateThread(NULL, 0, job_thread, &j, 0, &tid);
    if (!th) { CloseHandle(j.done); return 0; }
    tm_pump_wait(j.done, ms);
    if (WaitForSingleObject(j.done, 0) != WAIT_OBJECT_0) {
        t_note("client thread did not finish in %lu ms", (unsigned long)ms);
        CloseHandle(th); /* leak the job on purpose: the thread may still touch it */
        return 0;
    }
    WaitForSingleObject(th, 5000);
    CloseHandle(th); CloseHandle(j.done);
    return j.finished;
}

int tm_run_cfg(const TMCfg *cfg, tm_new_fn mk, const GUID *iid, tm_client_fn fn, void *ctx)
{
    HRESULT hr; IUnknown *obj; IStream *stm = NULL; int ok = 0;
    hr = U(CoInitializeEx)(NULL, cfg->srv_apt);
    CHECKF(SUCCEEDED(hr), "CoInitializeEx(server) hr=0x%lx", (long)hr);
    if (FAILED(hr)) return 0;
    obj = mk();
    hr = U(CoMarshalInterThreadInterfaceInStream)(iid, obj, &stm);
    CHECKF(hr == S_OK, "CoMarshalInterThreadInterfaceInStream (%s) hr=0x%lx", cfg->name, (long)hr);
    if (SUCCEEDED(hr)) ok = tm_run_thread(cfg->cli_apt, stm, iid, fn, ctx, 15000);
    CHECKF(ok, "client finished (%s)", cfg->name);
    IUnknown_Release(obj);
    U(CoUninitialize)();
    return ok;
}

int tm_run_all(tm_new_fn mk, const GUID *iid, tm_client_fn fn, void *ctx)
{
    int i;
    for (i = 0; i < 3; i++) {
        t_note("config: %s", tm_cfgs[i].name);
        if (!tm_run_cfg(&tm_cfgs[i], mk, iid, fn, ctx)) return 0;
    }
    return 1;
}

BOOL tm_is_proxy_call(IOletCustom *c)
{
    LONG tid = 0, pid = 0, apt = 0;
    HRESULT hr = c->lpVtbl->ApartmentInfo(c, &tid, &pid, &apt);
    return SUCCEEDED(hr) && (DWORD)tid != GetCurrentThreadId();
}
