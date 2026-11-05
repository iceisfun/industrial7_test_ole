/* StgOpenStorageOnHandle: a storage over a file handle the caller owns (Windows 8 and later). */
#include "ext.h"

typedef HRESULT (WINAPI *sooh_t)(HANDLE, DWORD, void *, void *, REFIID, void **);

static void mk_docfile(WCHAR *wpath, const char *leaf)
{
    char ap[MAX_PATH]; IStorage *st = NULL; IStream *s = NULL; ULONG n; HRESULT h;
    t_tmpfile(ap, sizeof ap, leaf);
    MultiByteToWideChar(CP_ACP, 0, ap, -1, wpath, MAX_PATH);
    h = StgCreateDocfile(wpath, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &st);
    if (h < 0) { t_skip("StgCreateDocfile failed %08lx", (unsigned long)h); wpath[0] = 0; return; }
    IStorage_CreateStream(st, L"S1", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &s);
    IStream_Write(s, "hello", 5, &n); IStream_Release(s);
    IStorage_Commit(st, STGC_DEFAULT); IStorage_Release(st);
}
static HANDLE open_rw(const WCHAR *p) { return CreateFileW(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL); }

TEST(oex_storage_on_handle_opens_a_docfile_and_leaves_the_handle_open)
{
    sooh_t p = (sooh_t)t_proc_in("ole32.dll", "StgOpenStorageOnHandle");
    WCHAR path[MAX_PATH]; HANDLE h; void *o = (void *)1; IStream *s = NULL; char b[8] = {0}; ULONG n = 0; DWORD sz;
    REQUIRE(p);
    mk_docfile(path, "sh1.stg"); if (!path[0]) return;
    h = open_rw(path);
    CHECK(h != INVALID_HANDLE_VALUE);
    EQ(p(h, STGM_READWRITE | STGM_SHARE_EXCLUSIVE, NULL, NULL, &IID_IStorage, &o), S_OK);
    CHECK(o != NULL && o != (void *)1);
    if (o && o != (void *)1) {
        EQ(IStorage_OpenStream((IStorage *)o, L"S1", NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &s), S_OK);
        if (s) { EQ(IStream_Read(s, b, 5, &n), S_OK); EQ(n, 5); EXPECT_STR(b, "hello"); IStream_Release(s); }
        EQ(IStorage_Release((IStorage *)o), 0);
    }
    sz = GetFileSize(h, NULL);
    CHECK(sz != INVALID_FILE_SIZE && sz > 0);       /* the handle is duplicated, not taken over: it is still the caller's */
    CloseHandle(h);
}

TEST(oex_storage_on_handle_mode_checks)
{
    sooh_t p = (sooh_t)t_proc_in("ole32.dll", "StgOpenStorageOnHandle");
    WCHAR path[MAX_PATH]; HANDLE h; void *o; HRESULT r;
    const DWORD RWX = STGM_READWRITE | STGM_SHARE_EXCLUSIVE;
    struct { DWORD mode; HRESULT want; const char *what; } c[] = {
        { RWX,                                           S_OK,                "RW exclusive" },
        { STGM_READ | STGM_SHARE_DENY_WRITE,             S_OK,                "R deny write" },
        { STGM_READ | STGM_SHARE_EXCLUSIVE,              S_OK,                "R exclusive" },
        { RWX | STGM_TRANSACTED,                         S_OK,                "RW exclusive transacted" },
        { STGM_WRITE | STGM_SHARE_EXCLUSIVE,             S_OK,                "W exclusive" },
        { RWX | STGM_SIMPLE,                             S_OK,                "RW exclusive simple" },
        { STGM_READ | STGM_SHARE_EXCLUSIVE | STGM_PRIORITY, S_OK,             "R exclusive priority" },
        { RWX | STGM_FAILIFTHERE,                        S_OK,                "RW exclusive failifthere" },
        { STGM_READWRITE | STGM_SHARE_DENY_WRITE,        X_STG_E_INVALIDFLAG, "RW deny write" },
        { STGM_READWRITE,                                X_STG_E_INVALIDFLAG, "RW default share" },
        { 0,                                             X_STG_E_INVALIDFLAG, "read, default share" },
        { STGM_READ | STGM_SHARE_DENY_NONE,              X_STG_E_INVALIDFLAG, "R deny none" },
        { STGM_READ | STGM_SHARE_DENY_READ,              X_STG_E_INVALIDFLAG, "R deny read" },
        { RWX | STGM_CREATE,                             X_STG_E_INVALIDFLAG, "create" },
        { RWX | STGM_CONVERT,                            X_STG_E_INVALIDFLAG, "convert" },
        { RWX | STGM_NOSCRATCH,                          X_STG_E_INVALIDFLAG, "noscratch" },
        { RWX | STGM_DELETEONRELEASE,                    (HRESULT)0x80030001, "delete on release" },
    };
    unsigned i;
    REQUIRE(p);
    mk_docfile(path, "sh2.stg"); if (!path[0]) return;
    h = open_rw(path);
    for (i = 0; i < sizeof c / sizeof c[0]; i++) {
        o = (void *)1;
        r = p(h, c[i].mode, NULL, NULL, &IID_IStorage, &o);
        CHECKF(r == c[i].want, "%s: got %08lx want %08lx", c[i].what, (unsigned long)r, (unsigned long)c[i].want);
        if (r >= 0 && o) IStorage_Release((IStorage *)o);
        else CHECKF(o == (void *)1, "%s: the output is untouched on a mode error", c[i].what);
    }
    CloseHandle(h);
}

TEST(oex_storage_on_handle_riid_and_pointer_arguments)
{
    sooh_t p = (sooh_t)t_proc_in("ole32.dll", "StgOpenStorageOnHandle");
    WCHAR path[MAX_PATH]; HANDLE h; void *o; HRESULT r; const DWORD m = STGM_READWRITE | STGM_SHARE_EXCLUSIVE;
    REQUIRE(p);
    mk_docfile(path, "sh3.stg"); if (!path[0]) return;
    h = open_rw(path);
    o = (void *)1; r = p(h, m, NULL, NULL, &IID_IUnknown, &o); EQ(r, S_OK); if (r >= 0 && o) IUnknown_Release((IUnknown *)o);
    o = (void *)1; r = p(h, m, NULL, NULL, &IID_IPropertySetStorage, &o); EQ(r, S_OK); if (r >= 0 && o) IUnknown_Release((IUnknown *)o);
    o = (void *)1; r = p(h, m, NULL, NULL, &IID_IStream, &o); EQ(r, X_E_NOINTERFACE); EQ(o, NULL);      /* the root has no IStream */
    EQ(p(h, m, NULL, NULL, &IID_IStorage, NULL), X_STG_E_INVALIDPOINTER);
    o = (void *)1; r = p(h, m, (void *)1, NULL, &IID_IStorage, &o); EQ(r, S_OK); if (r >= 0 && o) IStorage_Release((IStorage *)o);    /* reserved1 / 2 are ignored */
    o = (void *)1; r = p(h, m, NULL, (void *)1, &IID_IStorage, &o); EQ(r, S_OK); if (r >= 0 && o) IStorage_Release((IStorage *)o);
    CloseHandle(h);
}

TEST(oex_storage_on_handle_invalid_handles)
{
    sooh_t p = (sooh_t)t_proc_in("ole32.dll", "StgOpenStorageOnHandle");
    void *o = (void *)1;
    REQUIRE(p);
    EQ(p(INVALID_HANDLE_VALUE, STGM_READWRITE | STGM_SHARE_EXCLUSIVE, NULL, NULL, &IID_IStorage, &o), X_E_HANDLE);
    EQ(o, 1);
    EQ(p(NULL, STGM_READWRITE | STGM_SHARE_EXCLUSIVE, NULL, NULL, &IID_IStorage, &o), X_E_HANDLE);
    EQ(o, 1);
    EQ(p((HANDLE)(ULONG_PTR)0x7ff4, STGM_READ | STGM_SHARE_EXCLUSIVE, NULL, NULL, &IID_IStorage, &o), X_E_HANDLE);   /* never a handle of this process */
    EQ(o, 1);
    /* the mode is judged before the handle */
    EQ(p(INVALID_HANDLE_VALUE, STGM_READWRITE, NULL, NULL, &IID_IStorage, &o), X_STG_E_INVALIDFLAG);
}

TEST(oex_storage_on_handle_does_not_compare_the_handle_access_with_the_mode)
{
    sooh_t p = (sooh_t)t_proc_in("ole32.dll", "StgOpenStorageOnHandle");
    WCHAR path[MAX_PATH]; HANDLE h; void *o = (void *)1;
    REQUIRE(p);
    mk_docfile(path, "sh4.stg"); if (!path[0]) return;
    h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    EQ(p(h, STGM_READWRITE | STGM_SHARE_EXCLUSIVE, NULL, NULL, &IID_IStorage, &o), S_OK);     /* a read-only handle opens in a read-write mode */
    if (o && o != (void *)1) IStorage_Release((IStorage *)o);
    o = (void *)1;
    EQ(p(h, STGM_READ | STGM_SHARE_EXCLUSIVE, NULL, NULL, &IID_IStorage, &o), S_OK);
    if (o && o != (void *)1) IStorage_Release((IStorage *)o);
    CloseHandle(h);
}

TEST(oex_storage_on_handle_other_files_have_no_istorage)
{
    sooh_t p = (sooh_t)t_proc_in("ole32.dll", "StgOpenStorageOnHandle");
    WCHAR path[MAX_PATH]; char ap[MAX_PATH]; HANDLE h; void *o; DWORD n;
    const DWORD m = STGM_READWRITE | STGM_SHARE_EXCLUSIVE;
    REQUIRE(p);
    /* a plain text file */
    t_tmpfile(ap, sizeof ap, "plain.txt"); MultiByteToWideChar(CP_ACP, 0, ap, -1, path, MAX_PATH);
    h = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(h, "plain text file", 15, &n, NULL);
    o = (void *)1; EQ(p(h, m, NULL, NULL, &IID_IStorage, &o), X_E_NOINTERFACE); EQ(o, 1);   /* not a compound file: only property storage is possible */
    o = (void *)1; EQ(p(h, m, NULL, NULL, &IID_IStream, &o), X_E_NOINTERFACE); EQ(o, 1);
    { HRESULT ru; o = (void *)1; ru = p(h, m, NULL, NULL, &IID_IUnknown, &o);
      t_note("plain file, IUnknown: %08lx (Windows answers its NTFS property storage object)", (unsigned long)ru);
      if (ru >= 0 && o && o != (void *)1) IUnknown_Release((IUnknown *)o); }
    CloseHandle(h);
    /* an empty file stays empty */
    t_tmpfile(ap, sizeof ap, "empty.bin"); MultiByteToWideChar(CP_ACP, 0, ap, -1, path, MAX_PATH);
    h = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    o = (void *)1; EQ(p(h, m, NULL, NULL, &IID_IStorage, &o), X_E_NOINTERFACE); EQ(o, 1);
    EQ(GetFileSize(h, NULL), 0);
    CloseHandle(h);
    /* a directory */
    h = CreateFileA(t_tmpdir(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    o = (void *)1; EQ(p(h, STGM_READ | STGM_SHARE_EXCLUSIVE, NULL, NULL, &IID_IStorage, &o), X_E_NOINTERFACE); EQ(o, 1);
    CloseHandle(h);
}

/* ---- what the storage writes reaches the file; the handle is the caller's throughout --------------------------- */
static int has_stream(const WCHAR *path, const WCHAR *name)
{
    IStorage *q = NULL; IStream *s = NULL; HRESULT r; int found = 0;
    r = StgOpenStorage(path, NULL, STGM_READ | STGM_SHARE_DENY_WRITE, NULL, 0, &q);
    if (r < 0) return -1;
    r = IStorage_OpenStream(q, name, NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &s);
    if (r >= 0 && s) { found = 1; IStream_Release(s); }
    IStorage_Release(q);
    return found;
}
static void add_stream(sooh_t p, HANDLE h, DWORD mode, const WCHAR *name, int commit)
{
    IStorage *st = NULL; IStream *s = NULL; ULONG n;
    if (p(h, mode, NULL, NULL, &IID_IStorage, (void **)&st) < 0 || !st) { CHECK(0); return; }
    EQ(IStorage_CreateStream(st, name, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &s), S_OK);
    if (s) { IStream_Write(s, "world!", 6, &n); IStream_Release(s); }
    if (commit) EQ(IStorage_Commit(st, STGC_DEFAULT), S_OK);
    IStorage_Release(st);
}

TEST(oex_storage_on_handle_direct_and_transacted_writes)
{
    sooh_t p = (sooh_t)t_proc_in("ole32.dll", "StgOpenStorageOnHandle");
    WCHAR path[MAX_PATH]; HANDLE h;
    const DWORD RWX = STGM_READWRITE | STGM_SHARE_EXCLUSIVE;
    REQUIRE(p);
    mk_docfile(path, "sh5.stg"); if (!path[0]) return;
    h = open_rw(path);
    add_stream(p, h, RWX, L"DIRECT", 0);                       /* direct mode: released without a commit, still written */
    add_stream(p, h, RWX | STGM_TRANSACTED, L"DROPPED", 0);    /* transacted: released without a commit, gone */
    add_stream(p, h, RWX | STGM_TRANSACTED, L"KEPT", 1);       /* transacted and committed */
    CHECK(GetFileSize(h, NULL) != INVALID_FILE_SIZE);          /* the handle outlived three storages */
    CloseHandle(h);
    EQ(has_stream(path, L"S1"), 1);
    EQ(has_stream(path, L"DIRECT"), 1);
    EQ(has_stream(path, L"DROPPED"), 0);
    EQ(has_stream(path, L"KEPT"), 1);
}

TEST(oex_storage_on_handle_second_storage_on_a_live_one_is_refused)
{
    sooh_t p = (sooh_t)t_proc_in("ole32.dll", "StgOpenStorageOnHandle");
    WCHAR path[MAX_PATH]; HANDLE h; void *a = NULL, *b = (void *)1; HRESULT r;
    const DWORD RWX = STGM_READWRITE | STGM_SHARE_EXCLUSIVE;
    REQUIRE(p);
    mk_docfile(path, "sh6.stg"); if (!path[0]) return;
    h = open_rw(path);
    EQ(p(h, RWX, NULL, NULL, &IID_IStorage, &a), S_OK);
    r = p(h, RWX, NULL, NULL, &IID_IStorage, &b);
    EQ(r, (HRESULT)0x80070021);                                /* HRESULT_FROM_WIN32(ERROR_LOCK_VIOLATION): the first holds the open locks */
    EQ(b, 1);
    if (a) IStorage_Release((IStorage *)a);
    b = NULL;
    EQ(p(h, RWX, NULL, NULL, &IID_IStorage, &b), S_OK);        /* released: the next one opens */
    if (b) IStorage_Release((IStorage *)b);
    CloseHandle(h);
}
