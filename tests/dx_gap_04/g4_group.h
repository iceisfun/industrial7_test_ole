/* Interface-group templates (not in the mingw headers). Layouts match the Win10 rpcrt4 (0x50 / 0x28 bytes). */
#ifndef G4_GROUP_H
#define G4_GROUP_H
#include "rawfx.h"
#include <stddef.h>
typedef struct { ULONG Version; RPC_IF_HANDLE IfSpec; UUID *MgrTypeUuid; RPC_MGR_EPV *MgrEpv; unsigned int Flags; unsigned int MaxCalls;
                 unsigned int MaxRpcSize; RPC_IF_CALLBACK_FN *IfCallback; UUID_VECTOR *UuidVector; const char *Annotation; void *SecurityDescriptor; } G4_IFT_A;
typedef struct { ULONG Version; RPC_IF_HANDLE IfSpec; UUID *MgrTypeUuid; RPC_MGR_EPV *MgrEpv; unsigned int Flags; unsigned int MaxCalls;
                 unsigned int MaxRpcSize; RPC_IF_CALLBACK_FN *IfCallback; UUID_VECTOR *UuidVector; const WCHAR *Annotation; void *SecurityDescriptor; } G4_IFT_W;
typedef struct { ULONG Version; const char *ProtSeq; const char *Endpoint; void *SecurityDescriptor; ULONG Backlog; } G4_EPT_A;
typedef struct { ULONG Version; const WCHAR *ProtSeq; const WCHAR *Endpoint; void *SecurityDescriptor; ULONG Backlog; } G4_EPT_W;
typedef void (__RPC_API G4_IDLE_FN)(void *group, void *ctx, ULONG idle);
typedef RPC_STATUS (WINAPI *G4_CreateW_fn)(G4_IFT_W *, ULONG, G4_EPT_W *, ULONG, ULONG, G4_IDLE_FN *, void *, void **);
typedef RPC_STATUS (WINAPI *G4_CreateA_fn)(G4_IFT_A *, ULONG, G4_EPT_A *, ULONG, ULONG, G4_IDLE_FN *, void *, void **);
typedef RPC_STATUS (WINAPI *G4_Act_fn)(void *);
typedef RPC_STATUS (WINAPI *G4_Deact_fn)(void *, BOOL);
typedef RPC_STATUS (WINAPI *G4_Close_fn)(void *);
typedef RPC_STATUS (WINAPI *G4_Inq_fn)(void *, RPC_BINDING_VECTOR **);
#define G4_CreateW ((G4_CreateW_fn)t_proc("RpcServerInterfaceGroupCreateW"))
#define G4_CreateA ((G4_CreateA_fn)t_proc("RpcServerInterfaceGroupCreateA"))
#define G4_Activate ((G4_Act_fn)t_proc("RpcServerInterfaceGroupActivate"))
#define G4_Deactivate ((G4_Deact_fn)t_proc("RpcServerInterfaceGroupDeactivate"))
#define G4_Close ((G4_Close_fn)t_proc("RpcServerInterfaceGroupClose"))
#define G4_InqBindings ((G4_Inq_fn)t_proc("RpcServerInterfaceGroupInqBindings"))
#define G4_NEED_GROUP() do { REQUIRE(G4_CreateW); REQUIRE(G4_CreateA); REQUIRE(G4_Activate); REQUIRE(G4_Deactivate); REQUIRE(G4_Close); REQUIRE(G4_InqBindings); \
    if (t_is_wine()) { t_skip("interface groups are not implemented in wine"); return; } } while (0)
#endif
