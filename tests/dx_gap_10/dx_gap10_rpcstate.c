/* dx_gap10_rpcstate: internal rpcrt4 / ole32 exports that flip process-wide switches. Each runs in a child process
 * so the parent (and every other test) keeps default RPC runtime state. Nothing here touches the machine. */
#include "t.h"
#include <rpc.h>

static void WINAPI addr_cb(void *p) { (void)p; }

/* I_RpcMgmtEnableDedicatedThreadPool: allowed only while the RPC thread pool is not yet started; marks the pool private. */
CHILD(gap10_dedicated_pool) {
    unsigned (WINAPI *en)(void) = (void *)t_proc("I_RpcMgmtEnableDedicatedThreadPool");
    unsigned char (WINAPI *q)(void) = (void *)t_proc("I_RpcMgmtQueryDedicatedThreadPool");
    if (!en || !q) return 50;
    if (q() != 0) return 1;
    if (en() != 0) return 2;
    if (q() != 1) return 3;
    if (en() != 0) return 4;                    /* idempotent while the pool has not started */
    if (q() != 1) return 5;
    return 0;
}
TEST(dx_gap10_dedicated_threadpool_switch) {
    if (t_is_wine()) { t_skip("wine"); return; }
    if (!t_proc("I_RpcMgmtEnableDedicatedThreadPool")) { t_skip("missing export"); return; }
    DWORD rc = t_run_child("gap10_dedicated_pool", "", 15000);
    CHECKF(rc == 0, "child step %lu failed", (unsigned long)rc);
}

/* I_RpcServerSetAddressChangeFn / I_RpcServerInqAddressChangeFn: a plain store / load of the callback */
CHILD(gap10_addrchange) {
    unsigned (WINAPI *set)(void *) = (void *)t_proc("I_RpcServerSetAddressChangeFn");
    void *(WINAPI *inq)(void) = (void *)t_proc("I_RpcServerInqAddressChangeFn");
    if (!set || !inq) return 50;
    if (inq() != NULL) return 1;
    if (set((void *)addr_cb) != 0) return 2;
    if (inq() != (void *)addr_cb) return 3;
    if (set(NULL) != 0) return 4;
    if (inq() != NULL) return 5;
    return 0;
}
TEST(dx_gap10_address_change_fn_store) {
    if (t_is_wine()) { t_skip("wine"); return; }
    if (!t_proc("I_RpcServerSetAddressChangeFn")) { t_skip("missing export"); return; }
    DWORD rc = t_run_child("gap10_addrchange", "", 15000);
    CHECKF(rc == 0, "child step %lu failed", (unsigned long)rc);
}

/* trivial switches that always report RPC_S_OK */
CHILD(gap10_switches) {
    unsigned (WINAPI *dis)(void) = (void *)t_proc("I_RpcServerDisableExceptionFilter");
    unsigned (WINAPI *ee)(void) = (void *)t_proc("I_RpcTurnOnEEInfoPropagation");
    void (WINAPI *ssd)(void) = (void *)t_proc("I_RpcSsDontSerializeContext");
    if (!dis || !ee || !ssd) return 50;
    if (dis() != 0) return 1;
    if (dis() != 0) return 2;
    if (ee() != 0) return 3;
    if (ee() != 0) return 4;
    ssd();
    return 0;
}
TEST(dx_gap10_rpc_process_switches) {
    if (t_is_wine()) { t_skip("wine"); return; }
    if (!t_proc("I_RpcServerDisableExceptionFilter")) { t_skip("missing export"); return; }
    DWORD rc = t_run_child("gap10_switches", "", 15000);
    CHECKF(rc == 0, "child step %lu failed", (unsigned long)rc);
}

/* DllDebugObjectRPCHook(enable, arg) in a fresh process: always TRUE, can be turned off again */
CHILD(gap10_dcom_hook) {
    int (WINAPI *h)(int, void *) = (void *)t_proc("DllDebugObjectRPCHook");
    if (!h) return 50;
    if (h(1, NULL) != 1) return 1;
    if (h(0, NULL) != 1) return 2;
    return 0;
}
TEST(dx_gap10_debug_object_rpc_hook_toggle) {
    if (t_is_wine()) { t_skip("wine"); return; }
    DWORD rc = t_run_child("gap10_dcom_hook", "", 15000);
    CHECKF(rc == 0, "child step %lu failed", (unsigned long)rc);
}
