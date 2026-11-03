#ifndef MAR_H
#define MAR_H
#include "tm.h"
void tm_leakcheck(void);
#define CLI(var, p) IOletCustom *var = (IOletCustom *)(p); if (!var) { CHECKF(0, "no interface (unmarshal hr=0x%lx)", (long)tm_client_hr); return; }
#define CLID(var, p) IOletDual *var = (IOletDual *)(p); if (!var) { CHECKF(0, "no interface (unmarshal hr=0x%lx)", (long)tm_client_hr); return; }
#define RUN_CUSTOM(fn, ctx) do { CHECK(tm_run_all(tm_mk_custom, &TM_IID_IOletCustom, fn, ctx)); tm_leakcheck(); } while (0)
#define RUN_DUAL(fn, ctx)   do { CHECK(tm_run_all(tm_mk_dual, &TM_IID_IOletDual, fn, ctx)); tm_leakcheck(); } while (0)
#define CC(p) (p)->lpVtbl
#endif
