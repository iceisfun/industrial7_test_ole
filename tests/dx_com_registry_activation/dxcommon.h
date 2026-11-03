/* Shared helpers for the dx_com_registry_activation area.
 * State-changing: everything is written under HKCU\Software\Classes, tracked, and swept at start and exit.
 * The test exe doubles as the COM server: it exports DllGetClassObject (in-proc server = the exe path,
 * LoadLibrary of the running exe's own path returns the already-loaded image) and re-launches itself with
 * --child dxlsrv as LOCAL_SERVER. The class object / instances implement IClassFactory / IPersist only;
 * IPersist::GetClassID returns an encoded CLSID describing where the instance lives (see dx_info). */
#ifndef DXCOMMON_H
#define DXCOMMON_H
#include "t.h"
#include <ole2.h>
#include <objbase.h>

/* Registry helpers (HKCU\Software\Classes\<subkey>). subkey uses backslashes. */
int  dx_ready(void);                         /* one-time sweep+probe; 0 when HKCU\Software\Classes not writable */
HKEY dx_root(void);                          /* HKEY_CURRENT_USER on Windows (HKLM only inside the Wine validation prefix) */
#define DX_BEGIN() do { if (!dx_ready()) { t_skip("HKCU\\Software\\Classes is not writable"); return; } } while (0)
void dx_view(REGSAM wow);                    /* 0 (default), KEY_WOW64_32KEY or KEY_WOW64_64KEY for the helpers below */
LONG dx_set(const char *subkey, const char *name, const char *val);   /* REG_SZ, creates the key; name NULL = default */
LONG dx_setx(const char *subkey, const char *name, const char *val);  /* REG_EXPAND_SZ */
LONG dx_setw(const wchar_t *subkey, const wchar_t *name, const wchar_t *val);
LONG dx_setdw(const char *subkey, const char *name, DWORD val);
LONG dx_mkkey(const char *subkey);
int  dx_get(const char *subkey, const char *name, char *out, size_t cap);  /* 0 = ok */
int  dx_exists(const char *subkey);
void dx_del(const char *subkey);             /* delete tree, ignore errors */
void dx_track(const char *subkey);           /* delete at cleanup */
void dx_cleanup(void);

/* Unique GUIDs for this run: Data1 7E57C0DE, Data2 = kind, Data3 = serial, Data4 = "olet" + pid. */
void dx_newguid(GUID *g, WORD kind);
void dx_guidstr(const GUID *g, char *out40);
void dx_guidwstr(const GUID *g, wchar_t *out40);
int  dx_parse_guid(const char *s, GUID *g);
void dx_clsidkey(const GUID *g, char *out, size_t cap);   /* "CLSID\{...}" */
void dx_progid(char *out, size_t cap, const char *leaf);  /* "olet<pid>.<leaf>" */

/* Server-class kinds (GUID Data2). */
#define DXK_NORMAL   0x0001
#define DXK_IFACE    0x0002
#define DXK_AGG      0x00A9   /* instance supports aggregation */
#define DXK_NOCLASS  0xEEEE   /* DllGetClassObject returns CLASS_E_CLASSNOTAVAILABLE */
#define DXK_NOCREATE 0xEEED   /* IClassFactory::CreateInstance returns E_OUTOFMEMORY */

const char *dx_server_path(void);
int  dx_selfload_ok(void);                   /* LoadLibrary(exe path) is the running image and exports DllGetClassObject */
/* Register CLSID\{g} with InprocServer32=exe (+ThreadingModel when model != NULL). Returns the CLSID key path in key. */
void dx_reg_inproc(GUID *clsid, const char *model, char *key, size_t cap);
/* Register LocalServer32 = "exe" --child dxlsrv <parentpid> <apt> <regcls> <clsid> [extra] */
void dx_reg_local(GUID *clsid, const char *apt /* sta|mta */, const char *regcls /* multi|single */, char *key, size_t cap);
void dx_local_stop(const GUID *clsid, DWORD pid);   /* signal quit event, wait, kill if needed */
void dx_local_prepare(const GUID *clsid);           /* create the quit event before activating */

/* Instance description encoded in IPersist::GetClassID */
typedef struct {
    DWORD tid; int apt; int qual; unsigned idx; unsigned flags; DWORD pid; int marker_ok;
} dx_info;
#define DXF_EMBEDDING 1
#define DXF_LOCAL     2
int  dx_getinfo(IUnknown *p, dx_info *out);          /* QI IPersist + GetClassID; returns hr */
int  dx_is_direct(IUnknown *p);                      /* p is our own vtable, not a proxy */
extern volatile LONG dx_live, dx_nclassobj, dx_ncreate, dx_nlock;
extern volatile DWORD dx_ci_tid;
extern GUID dx_ci_iid, dx_gco_clsid;
extern IUnknown *volatile dx_ci_outer;
void dx_reset_counters(void);

/* COM thread helpers (through the DLL under test). */
HRESULT dx_coinit(DWORD model);
void dx_couninit(void);

/* Child with a log file: runs child `name`, collects what it dx_log()ed, returns exit code. */
DWORD dx_child(const char *name, const char *extra, DWORD timeout_ms, char *log, size_t logcap);
void  dx_log(int argc, char **argv, const char *fmt, ...);
#endif
