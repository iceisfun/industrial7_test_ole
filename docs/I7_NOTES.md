# Running the corpus on the i7 Win10-personality kernel

The i7 kernel runs this corpus inside its Windows 10 personality: a freestanding runner executable starts the staged
static mingw exes one at a time, each exe's console output reaches the serial log between `##BEGIN <id>` and
`##END <id> rc=0x.. ms=..` markers, and an owned Windows 10 DLL family sits on the system drive. This file records what
the exes need from that family, how output and results are framed, and a suggested bring-up order with the expected
Windows 10 totals. Every number below is a measurement on genuine Windows 10 22H2 (build 19045); nothing here has been
measured on i7 yet, and the i7 side of the framing is described from the runner's documented behaviour.

## What i7 must provide

The exes are static (no libgcc/libstdc++ DLLs) and import only four DLLs, all in the i7 family: `kernel32`, `msvcrt`
(every exe), `user32` (41 exes) and `advapi32` (22 exes). Nothing imports `ntdll`, `gdi32` or the DLLs under test.

**The DLLs under test are not imported; they are loaded at run time and i7 must provide them.** The harness
(`common/t.c`) searches `ole32.dll combase.dll oleaut32.dll rpcrt4.dll sechost.dll ntdll.dll kernel32.dll` with
`GetProcAddress`. `main()` loads `ole32.dll` first and exits with code 2 and `FATAL cannot load ole32.dll` if that fails,
so without an `ole32.dll` every area fails at start. `oleaut32.dll` is not in the listed family and is needed by the
oleaut32 areas (`bstr_core`, `safearray_core`, `variant_*`, `typelib_*`, `dispatch_typeinfo_dispcall`, `usrmarshal_oleaut32`,
parts of `exports_*` and `gap_*`); `combase`, `rpcrt4` and `sechost` are in the family. Minimum for the tiers below:

| tier | needs |
|---|---|
| 0 | `ole32` (load + export table), `combase`, `oleaut32`, `rpcrt4`, `sechost` loadable with their export names/forwarders |
| 1 | working code in `oleaut32` (BSTR, SAFEARRAY, VARIANT), `rpcrt4` (UUID strings, NDR formats), `combase` (HSTRING, memory, GUID strings) |
| 2 | in-process COM (apartments, class factories, contexts, marshaling), structured storage on a file system, type libraries, `gdi32` for the picture tests |
| 3 | a service control manager, an RPC endpoint mapper / ncalrpc, ROT, process creation of the exe itself (`--child`), user32 windows and messages |
| 4 | everything of 3 plus registry writes under `HKCU\Software\Classes`, service create/delete, ETW sessions, audit policy, the OLE clipboard |

Run-time `LoadLibrary`/`GetModuleHandle` of other names, all tolerant of failure unless noted: `gdi32.dll` (picture/font and
user-marshal tests), `shell32.dll` and `msvbvm60.dll` (a type-library search list in `typelib_load_info`, absence is tolerated),
`version.dll` (`com_init_apartments`: must load, then `GetModuleHandle` must return the same handle), `winhttp.dll` (named in an
inbox-class-registration check in `com_class_factories`), `comsvcs.dll` (`GetModuleHandle` only, `com_marshal_xproc`),
`netapi32.dll` (`dx_gap_02`, skips when absent). Test names such as `combase_no_such_dll.dll` are deliberate load failures.

### Imported functions (union over the 66 exes of the first measurement)

Counts: kernel32 131, msvcrt 79, user32 34, advapi32 65.

- kernel32: `AddVectoredExceptionHandler` `CloseHandle` `CopyFileW` `CreateDirectoryA` `CreateDirectoryW` `CreateEventA` `CreateEventW` `CreateFileA` `CreateFileW` `CreateProcessA` `CreateSemaphoreW` `CreateThread` `DeleteCriticalSection` `DeleteFileA` `DeleteFileW` `DosDateTimeToFileTime` `EnterCriticalSection` `ExitProcess` `ExitThread` `ExpandEnvironmentStringsW` `FileTimeToDosDateTime` `FileTimeToSystemTime` `FindClose` `FindFirstFileA` `FindNextFileA` `FormatMessageA` `FormatMessageW` `FreeLibrary` `GetACP` `GetComputerNameA` `GetComputerNameW` `GetCurrentDirectoryA` `GetCurrentDirectoryW` `GetCurrentProcess` `GetCurrentProcessId` `GetCurrentThread` `GetCurrentThreadId` `GetExitCodeProcess` `GetExitCodeThread` `GetFileAttributesA` `GetFileAttributesExW` `GetFileAttributesW` `GetFileSize` `GetFileTime` `GetLastError` `GetLocaleInfoA` `GetLocaleInfoW` `GetModuleFileNameA` `GetModuleFileNameW` `GetModuleHandleA` `GetModuleHandleExA` `GetModuleHandleExW` `GetModuleHandleW` `GetProcAddress` `GetProcessHeap` `GetSystemDirectoryA` `GetSystemDirectoryW` `GetSystemTime` `GetSystemTimeAsFileTime` `GetTempPathA` `GetThreadLocale` `GetTickCount` `GetUserDefaultLCID` `GetUserDefaultUILanguage` `GlobalAlloc` `GlobalFlags` `GlobalFree` `GlobalLock` `GlobalSize` `GlobalUnlock` `HeapAlloc` `HeapFree` `HeapSize` `InitializeCriticalSection` `IsDBCSLeadByteEx` `IsDebuggerPresent` `LeaveCriticalSection` `LoadLibraryA` `LocalAlloc` `LocalFree` `LocalSize` `MulDiv` `MultiByteToWideChar` `OpenEventA` `OpenProcess` `OpenThread` `QueryFullProcessImageNameA` `QueueUserAPC` `RaiseException` `ReadFile` `ReleaseSemaphore` `RemoveDirectoryA` `RemoveDirectoryW` `RemoveVectoredExceptionHandler` `ResetEvent` `RtlCaptureContext` `RtlCaptureStackBackTrace` `SetCurrentDirectoryA` `SetCurrentDirectoryW` `SetEnvironmentVariableA` `SetErrorMode` `SetEvent` `SetFileAttributesA` `SetFileAttributesW` `SetFileTime` `SetLastError` `SetUnhandledExceptionFilter` `Sleep` `SleepEx` `SystemTimeToFileTime` `TerminateProcess` `TerminateThread` `TlsGetValue` `VirtualProtect` `VirtualQuery` `WaitForMultipleObjects` `WaitForSingleObject` `WideCharToMultiByte` `WriteFile` `__C_specific_handler` `lstrcatW` `lstrcmpA` `lstrcmpW` `lstrcmpiA` `lstrcmpiW` `lstrcpyA` `lstrcpyW` `lstrcpynA` `lstrcpynW` `lstrlenA` `lstrlenW`
- msvcrt: `___lc_codepage_func` `___mb_cur_max_func` `__getmainargs` `__initenv` `__iob_func` `__set_app_type` `__setusermatherr` `_amsg_exit` `_cexit` `_commode` `_errno` `_fmode` `_initterm` `_lock` `_nextafter` `_setjmp` `_snprintf` `_snwprintf` `_strdup` `_stricmp` `_strnicmp` `_strtoi64` `_strtoui64` `_unlock` `_wcsicmp` `_wcsnicmp` `_wtoi` `abort` `atexit` `atoi` `calloc` `exit` `fflush` `fprintf` `fputc` `fputwc` `free` `fwprintf` `getc` `isdigit` `isspace` `isxdigit` `ldexp` `localeconv` `longjmp` `malloc` `memcmp` `memcpy` `memset` `putchar` `qsort` `realloc` `setvbuf` `signal` `strcat` `strchr` `strcmp` `strcpy` `strerror` `strlen` `strncmp` `strncpy` `strrchr` `strstr` `strtol` `strtoul` `tolower` `toupper` `towupper` `ungetc` `vfprintf` `wcscat` `wcschr` `wcscmp` `wcscpy` `wcslen` `wcsncmp` `wcsrchr` `wcsstr`
- user32: `AppendMenuA` `CharLowerA` `CharLowerW` `CharUpperA` `CharUpperW` `CloseClipboard` `CopyAcceleratorTableW` `CreateAcceleratorTableA` `CreateAcceleratorTableW` `CreateMenu` `CreatePopupMenu` `CreateWindowExA` `DefWindowProcA` `DestroyAcceleratorTable` `DestroyMenu` `DestroyWindow` `DispatchMessageA` `DispatchMessageW` `FindWindowExA` `GetClipboardData` `GetPropA` `GetPropW` `IsClipboardFormatAvailable` `LoadIconA` `MsgWaitForMultipleObjects` `OpenClipboard` `PeekMessageA` `PeekMessageW` `RegisterClassA` `RegisterClipboardFormatA` `RegisterClipboardFormatW` `TranslateMessage` `wsprintfA` `wsprintfW`
- advapi32: `AddAccessAllowedAce` `AddAccessAllowedAceEx` `AddAccessDeniedAceEx` `AddAuditAccessAce` `AddAuditAccessAceEx` `AddMandatoryAce` `AdjustTokenPrivileges` `AllocateAndInitializeSid` `CheckTokenMembership` `CloseServiceHandle` `ConvertStringSecurityDescriptorToSecurityDescriptorW` `CreateWellKnownSid` `EqualSid` `FreeSid` `GetAce` `GetAclInformation` `GetKernelObjectSecurity` `GetLengthSid` `GetSecurityDescriptorControl` `GetSecurityDescriptorDacl` `GetSecurityDescriptorGroup` `GetSecurityDescriptorLength` `GetSecurityDescriptorOwner` `GetSecurityDescriptorSacl` `GetSidIdentifierAuthority` `GetSidSubAuthority` `GetSidSubAuthorityCount` `GetTokenInformation` `GetUserNameW` `InitializeAcl` `InitializeSecurityDescriptor` `IsValidAcl` `IsValidSecurityDescriptor` `IsValidSid` `LookupAccountNameW` `LookupPrivilegeValueA` `MakeAbsoluteSD` `MakeSelfRelativeSD` `OpenProcessToken` `OpenSCManagerW` `OpenThreadToken` `RegCloseKey` `RegCreateKeyExA` `RegCreateKeyExW` `RegDeleteKeyExW` `RegDeleteKeyW` `RegDeleteTreeA` `RegDeleteTreeW` `RegEnumKeyExA` `RegEnumKeyExW` `RegOpenKeyExA` `RegOpenKeyExW` `RegOverridePredefKey` `RegQueryInfoKeyW` `RegQueryValueA` `RegQueryValueExA` `RegQueryValueExW` `RegQueryValueW` `RegSetValueExA` `RegSetValueExW` `SetSecurityDescriptorControl` `SetSecurityDescriptorDacl` `SetSecurityDescriptorGroup` `SetSecurityDescriptorOwner` `SetSecurityDescriptorSacl`

Notes: `__C_specific_handler` (SEH, imported from kernel32), `_setjmp`/`longjmp` (the harness recovers from faulting tests),
`__getmainargs`, `__iob_func`, `_snprintf`/`_snwprintf`, `fwprintf`/`vfprintf` and `setvbuf` from msvcrt (printf is bound to
`stdout` through `__iob_func`). The test harness sets an unhandled-exception filter and calls `SetErrorMode`.

## Output contract for a serial-log runner

- `main()` calls `setvbuf(stdout, NULL, _IONBF, 0)` before anything else (also in `--child` mode), so every character reaches the
  console handle as it is written and a crash loses at most the current line. Output goes through the CRT `stdout` only; there
  is no stderr output and no stray `printf` outside the grammar.
- Grammar (one record per line, `\n` text-mode so a console sees CRLF): `RUN  <test>`, then zero or more `  FAIL file:line expr (detail)`,
  `  SKIP reason`, `  NOTE text` lines, then exactly one of `OK   <test>`, `FAIL <test>`, `SKIP <test>`; last line
  `SUMMARY tests=N failed=N skipped=N checks_pass=N checks_fail=N`. A missing SUMMARY means crash/timeout/dead guest; the
  last `RUN` line without a result line names the test that was running.
- Exit code: 0 all tests passed or skipped, 1 some test failed, 2 `FATAL cannot load ole32.dll`, a crash gives an NTSTATUS.
- No arguments are needed (an optional argv[1] is a test-name substring filter). Run each exe from a clean state; scratch files
  are under `%TEMP%\olet_<pid>\` (removed on exit), so `GetTempPathA` must return a writable directory.
- Tests that need a second process re-launch the exe as `<exe> --child <name> [args]` with `CreateProcessA` and judge the child's
  exit code (`t_run_child`, 20 s limit). **26 of the 66 areas do this, including `core_smoke` (`smoke_child_roundtrip`) and `gap_00`
  (many tests)**: `bstr_core com_class_factories com_context_threading com_init_apartments com_marshal_basic com_marshal_xproc
  core_smoke dispatch_typeinfo_dispcall gap_00 gap_03 gap_04 gap_08 gap_09 monikers_rot picture_font_ole propvariant_propstg
  rpc_ep_mgmt_misc rpc_server_client_ncalrpc sechost_service_ctrl_local storage_create_open` and `dx_com_registry_activation
  dx_gap_01 dx_gap_03 dx_gap_04 dx_gap_10 dx_typelib_marshal_registry`. Where `CreateProcess` of the exe from inside the guest is not
  available yet, only those tests fail (a spawn failure shows as exit code 0xDEAD0000, a timeout as 0xDEAD0001); expect 1 failed test in
  `core_smoke` until it works. A child's console output goes to the console, not into the parent's result lines.
- Time: on Windows 10 every area takes 0.2 to 25 s (table below); use a per-exe limit of at least 120 s (the Windows runs
  allowed 600 s). Keep one exe per `##BEGIN`/`##END` pair; `ms=` in the END line is the exe's wall time.

## Tools

    python3 tools/i7_areas.py --check                   # verify the bring-up list covers tests/ exactly
    python3 tools/i7_areas.py --nondx --comma           # 55 non-dx areas, the form a runner --only option takes
    python3 tools/i7_areas.py --dx --comma              # the 18 dx_ areas, to stage and run separately
    python3 tools/i7_areas.py --all --order bringup     # smallest first (tools/i7_bringup.txt)
    python3 tools/win10_results.py --i7 RUN_DIR AREA... [--tests | --failures | --format json]
    python3 tools/i7_serial.py RUN_DIR AREA... -o serial.olet.log      # the same conversion, written to a file

`RUN_DIR` holds `logs/boot-*.log` (the guest serial logs; `RUN_DIR/gate/logs` is accepted too) and optionally `results.tsv`
(scheduler verdicts `CRASH-KERNEL`/`NOTRUN`). The converter joins the `CON: ` records of each exe (one record per character is
fine: the CR of a CRLF may show as a trailing `.`, which is cut where a corpus line start follows), then emits the `OLET
begin/os/mode/start/out/rc/done` grammar of the Windows runs with mode `i7`, so the per-exe table, `--tests`, `--failures`
and `win10_summarize.py` work unchanged. Status per area: `OK` (SUMMARY seen, rc 0 or 1), `TIMEOUT` (runner killed it),
`NOSUMMARY` (ended without SUMMARY: crash), `HANG` (log stops inside the exe, or the guest died: no END), `MISSING`
(never started, or the loader refused it; the reason is in an `I7 CREATEFAIL st=<ntstatus>` out line). Compare with
Windows per test: `win10_results.py --tests` on both logs and diff the `exe/test/result` columns.

## Suggested bring-up order and Windows 10 expected totals

Measured on Windows 10 22H2 build 19045 with the `adm` token (elevated administrator) on the 55 non-dx areas and with
the `dx` pass on the 18 dx areas (final whole-corpus run: adm 8040 tests, std 8040, dx 590, 0 failed); `skips adm / std` are the skipped tests in the elevated and the restricted-admin pass
(i7 will most likely behave like `adm`); all `failed` counts are 0 on Windows. Tiers are a reading of which system
facilities the source uses (source scan plus the Windows skip reasons), not an i7 measurement; "s" is Windows wall time.
Tier totals (adm tests): tier 0 4 areas 467 tests; tier 1 16 areas 2115 tests; tier 2 16 areas 3185 tests; tier 3 14 areas 2157 tests (sum 7924); tier 4 (dx) 16 areas 580 tests.

`oleaut32_ext` (tier 1, 27 tests) was added after the 66-exe baseline measurement; the tier totals above and the totals in COVERAGE.md/HANDOFF.md do not include it.

| area | tier | DLLs imported besides kernel32 + msvcrt | other DLLs loaded by name at run time (not imported) | tests | skips adm / std | s (adm) |
|---|---|---|---|---|---|---|
| core_smoke | 0 | - | - | 3 | 0 / 0 | 0.2 |
| exports_ole32 | 0 | - | - | 101 | 0 / 0 | 0.3 |
| exports_combase | 0 | - | - | 243 | 0 / 0 | 1.7 |
| exports_oleaut32_rpcrt4_sechost | 0 | - | - | 120 | 0 / 0 | 0.7 |
| bstr_core | 1 | user32 | - | 113 | 0 / 0 | 2.0 |
| safearray_core | 1 | user32 | - | 137 | 0 / 0 | 0.7 |
| variant_core | 1 | - | - | 188 | 0 / 0 | 0.5 |
| variant_convert_numeric | 1 | - | - | 234 | 0 / 0 | 0.7 |
| variant_format_parse | 1 | - | - | 176 | 0 / 0 | 1.2 |
| variant_arith_ops | 1 | - | - | 93 | 0 / 0 | 1.8 |
| oleaut32_ext | 1 | - | - | 27 | 0 / 0 | 1.7 |
| com_memory_guid_strings | 1 | - | - | 155 | 0 / 0 | 1.2 |
| rpc_uuid_strings | 1 | user32 | - | 133 | 0 / 0 | 1.0 |
| usrmarshal_oleaut32 | 1 | - | - | 107 | 0 / 0 | 0.4 |
| usrmarshal_ole32 | 1 | - | gdi32, user32 | 91 | 0 / 0 | 0.5 |
| rpc_ndr_marshal_formats | 1 | - | - | 219 | 0 / 0 | 0.3 |
| winrt_hstring | 1 | - | - | 161 | 0 / 0 | 1.7 |
| gap_00 | 1 | advapi32, user32 | - | 60 | 0 / 0 | 1.2 |
| gap_02 | 1 | - | user32 | 82 | 0 / 0 | 0.3 |
| gap_05 | 1 | - | - | 70 | 0 / 0 | 1.6 |
| gap_10 | 1 | advapi32 | - | 96 | 0 / 0 | 0.3 |
| com_init_apartments | 2 | user32 | - | 179 | 0 / 0 | 1.1 |
| com_class_factories | 2 | user32 | - | 132 | 0 / 0 | 1.6 |
| combase_ext | 2 | advapi32 | - | 10 | 0 / 0 | 1.5 |
| com_context_threading | 2 | user32 | - | 171 | 0 / 0 | 6.7 |
| com_marshal_basic | 2 | user32 | - | 186 | 0 / 0 | 2.9 |
| storage_elements_streams | 2 | user32 | - | 458 | 0 / 0 | 2.8 |
| storage_create_open | 2 | user32 | - | 167 | 0 / 0 | 7.3 |
| storage_lockbytes_misc | 2 | - | gdi32 | 168 | 0 / 0 | 0.8 |
| propvariant_propstg | 2 | user32 | - | 235 | 0 / 0 | 8.9 |
| typelib_load_info | 2 | user32 | advapi32, gdi32, shell32 | 150 | 0 / 0 | 1.2 |
| typelib_create_roundtrip | 2 | user32 | - | 135 | 0 / 0 | 3.8 |
| dispatch_typeinfo_dispcall | 2 | user32 | - | 292 | 0 / 0 | 0.7 |
| gap_06 | 2 | user32 | - | 148 | 0 / 0 | 1.2 |
| gap_09 | 2 | - | gdi32 | 106 | 0 / 0 | 0.6 |
| picture_font_ole | 2 | - | - | 259 | 0 / 0 | 1.4 |
| ole2_core_helpers | 2 | user32 | gdi32 | 264 | 0 / 0 | 17.5 |
| winrt_roapi | 2 | user32 | - | 135 | 0 / 0 | 1.1 |
| sechost_security_sid_sd | 3 | advapi32, user32 | - | 248 | 0 / 0 | 0.5 |
| sechost_audit_policy | 3 | advapi32 | - | 17 | 0 / 4 | 0.3 |
| rpc_proxy_stub_dcom | 3 | advapi32, user32 | - | 131 | 0 / 0 | 0.6 |
| rpc_ep_mgmt_misc | 3 | user32 | - | 124 | 0 / 0 | 1.3 |
| rpc_server_client_ncalrpc | 3 | advapi32, user32 | - | 201 | 0 / 0 | 22.4 |
| rpc_ext_internal | 3 | user32 | - | 33 | 0 / 0 | 2 |
| gap_03 | 3 | advapi32, user32 | - | 108 | 0 / 0 | 0.7 |
| gap_04 | 3 | advapi32, user32 | gdi32 | 120 | 0 / 0 | 5.8 |
| gap_07 | 3 | advapi32, user32 | - | 93 | 0 / 0 | 0.7 |
| gap_08 | 3 | - | advapi32 | 80 | 0 / 1 | 1.4 |
| monikers_rot | 3 | user32 | - | 223 | 0 / 0 | 6.2 |
| ole32_ext | 3 | user32 | - | 29 | 0 / 0 | 1.1 |
| com_marshal_xproc | 3 | advapi32, user32 | - | 126 | 0 / 0 | 7.7 |
| gap_01 | 3 | advapi32, user32 | - | 72 | 0 / 1 | 1.0 |
| sechost_etw_trace | 3 | advapi32, user32 | - | 162 | 0 / 1 | 1.2 |
| sechost_services_query | 3 | advapi32, user32 | - | 337 | 2 / 13 | 4.7 |
| sechost_service_ctrl_local | 3 | advapi32, user32 | - | 132 | 2 / 2 | 24.2 |
| dx_com_class_factories | 4 | advapi32 | - | 4 | 0 / - | 0.5 |
| dx_gap_00 | 4 | advapi32 | - | 5 | 0 / - | 0.2 |
| dx_gap_02 | 4 | advapi32, user32 | - | 9 | 0 / - | 0.6 |
| dx_gap_04 | 4 | advapi32, user32 | - | 12 | 0 / - | 2.9 |
| dx_gap_06 | 4 | user32 | - | 5 | 0 / - | 0.3 |
| dx_gap_09 | 4 | - | user32 | 3 | 0 / - | 0.1 |
| dx_gap_10 | 4 | - | - | 4 | 0 / - | 0.2 |
| dx_gap_01 | 4 | advapi32, user32 | - | 24 | 0 / - | 0.7 |
| dx_gap_03 | 4 | advapi32, user32 | - | 8 | 0 / - | 0.5 |
| dx_gap_07 | 4 | user32 | - | 3 | 2 / - | 1.0 |
| dx_rpc_ep_register | 4 | user32 | - | 9 | 0 / - | 1.0 |
| dx_ole32_ext | 4 | advapi32 | - | 3 | 0 / - | 0.3 |
| dx_typelib_marshal_registry | 4 | advapi32, user32 | - | 172 | 0 / - | 10.0 |
| dx_com_registry_activation | 4 | advapi32, user32 | - | 151 | 0 / - | 35.7 |
| dx_ole2_clipboard | 4 | user32 | gdi32 | 15 | 0 / - | 0.5 |
| dx_gap_08 | 4 | - | - | 3 | 0 / - | 8.4 |
| dx_sechost_service_lifecycle | 4 | advapi32, user32 | - | 153 | 0 / - | 9.7 |
| dx_sechost_audit_policy | 4 | advapi32 | - | 7 | 0 / - | 2.3 |

Per tier, the system facilities to expect and what to do when the area fails early:

- **Tier 0** `core_smoke`, `exports_*`: only loading and export tables. `exports_oleaut32_rpcrt4_sechost` also reads
  forwarder targets and ordinals (584,839 checks), so it is a good loader/PE-export stress.
- **Tier 1** pure computation: BSTR/SAFEARRAY/VARIANT, UUID and GUID strings, NDR format strings, user marshaling, HSTRING.
  `usrmarshal_ole32` uses `gdi32` handles (bitmap, metafile) in a few tests.
- **Tier 2** in-process COM and structured storage: `CoInitializeEx` variants and apartments, class factories, contexts
  (`com_context_threading` creates windows and threads, uses RoXxx), storage on files in the temp directory, type libraries
  created and loaded (`typelib_create_roundtrip` saves and reloads type libraries), pictures
  (`picture_font_ole`, `ole2_core_helpers`: clipboard and drag-and-drop *API* calls on the calling thread, window classes).
- **Tier 3** needs system services or a second process:
  - SCM: `sechost_services_query`, `sechost_service_ctrl_local`, `gap_08`, `gap_10` (OpenSCManager, EnumServicesStatus,
    query/control of the stock RpcSs, DcomLaunch, RpcEptMapper). Windows skips in these areas are due to the restricted token only.
  - RPC endpoint mapper / ncalrpc servers: `rpc_server_client_ncalrpc`, `rpc_ep_mgmt_misc`, `gap_04`, `gap_07` start
    ncalrpc servers in-process and talk to them; endpoint-map queries need the RPC endpoint mapper.
  - ROT and cross-process marshaling: `monikers_rot`, `com_marshal_xproc` (the ROT is served by a system service on Windows; both
    re-launch the exe with `--child`).
  - ETW: `sechost_etw_trace`, `gap_01` (EventAccess* on provider security; one std-token skip on Windows). Trace sessions need an
    ETW service.
  - Clipboard and drag-and-drop API calls: `gap_03`, `ole2_core_helpers` (tier 2).
  - Registry: `rpc_proxy_stub_dcom` (a private `HKCU` sandbox key redirected over `HKEY_CLASSES_ROOT` with `RegOverridePredefKey`,
    deleted again), `sechost_security_sid_sd` (token and security descriptors, `LookupAccountName`).
- **Tier 4** `dx_*`: registry class registrations (`dx_com_registry_activation`, `dx_typelib_marshal_registry`, `dx_gap_01/03/06`),
  service create/delete (`dx_sechost_service_lifecycle`, `dx_gap_08`; the Windows lifecycle run needs an SCM that deletes
  services promptly, one of three guests wedged in the SCM), audit policy (`dx_gap_00`), LSA rights (`dx_gap_02`, `netapi32`),
  endpoint-map registration (`dx_rpc_ep_register`, `dx_gap_07`), the OLE clipboard (`dx_ole2_clipboard`, `dx_gap_09`, needs an
  interactive desktop window station). Run on a disposable guest.

## Risks specific to a first i7 run

- A kernel fault inside one exe loses the rest of its batch: stage the tier 0 and tier 1 areas first and run tiers 3 and 4 one
  area per boot.
- The export sweeps do many checks: `exports_oleaut32_rpcrt4_sechost` has 120 tests but its 584,839 checks
  are silent (only failed checks print), so the serial volume is small everywhere on a passing run. Output is
  one `RUN` and one result line per test plus NOTE lines.
- Locale-dependent NOTE lines (month names, code pages) print `?` on the Windows runs for non-ASCII bytes; compare NOTE lines
  loosely, compare `OK/FAIL/SKIP` per test strictly.
