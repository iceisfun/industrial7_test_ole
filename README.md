# ole32.dll conformance corpus

Black-box tests for Windows 10 x64 `ole32.dll`. The DLL is never linked: each test loads it with `LoadLibrary`,
resolves entry points with `GetProcAddress` (`U(Name)` gives a pointer typed from the SDK header;
`UIN("module.dll", Name)` pins one module, which matters for forwarders) and calls through the pointer.

Modules searched by `U()`: ole32.dll combase.dll oleaut32.dll rpcrt4.dll sechost.dll ntdll.dll kernel32.dll.

    make                         # cross-compile build/<area>.exe (mingw-w64)
    build\<area>.exe [substr]    # on Windows; optional test-name filter
    tools\run_all.bat            # everything, results\<area>.txt (add /destructive for dx_* areas)
    python tools\summarize.py results [--failures]

Status: the corpus is verified on genuine Windows 10 22H2 (build 19045): 73 exes, 8630 tests, 0 failed
(`adm` 8040 tests / 4 skipped, `std` 8040 / 22 skipped, `dx` 590 / 2 skipped); see `COVERAGE.md`. It was first written under
Wine only; Windows 10 results are the authority and all expectations were reconciled with them.

Output: `RUN/OK/FAIL/SKIP` per test, `  FAIL file:line expr (detail)` per failed check, `SUMMARY` last;
exit code 1 if any test failed. `NOTE` lines record observed, version-dependent behaviour.

Tests that need a second process re-launch the exe (`--child <name>`). Scratch files live in
`%TEMP%\olet_<pid>\` and are removed on exit. `tools/*_exports.txt` list the named exports.



## Areas

Every directory under `tests/` is one exe. Test names are prefixed with the area and are unique project-wide
(CHILD names are unique per exe). `COVERAGE.md` lists per-area test counts and the export coverage.

| prefix | what |
|---|---|
| `core_smoke` | harness smoke test |
| `exports_*` | export presence, forwarder targets and ordinals for the five modules |
| `com_*` | CoInitialize/apartments, class factories, contexts, memory/GUID/strings, in-proc and cross-process marshaling |
| `monikers_rot`, `storage_*`, `propvariant_propstg`, `ole2_core_helpers`, `picture_font_ole`, `ole32_ext` | ole32 monikers/ROT, structured storage, PROPVARIANT, OLE2 helpers, pictures/fonts; `ole32_ext`: ole32 exports the other areas only check for presence (`StgOpenStorageOnHandle`, `GetObjectFromRotByPath`, `CleanROTForApartment`/`DestroyRunningObjectTable`, `OleInitializeWOW`, `Ole32DllGetClassObject`, the drop-target registration ordinals, clipboard-broker ordinals, `OleGetPackageClipboardOwner`, `CoCheckElevationEnabled`) |
| `bstr_core`, `safearray_core`, `variant_*`, `typelib_*`, `dispatch_typeinfo_dispcall`, `usrmarshal_*` | oleaut32 |
| `rpc_*` | rpcrt4: UUID/strings, NDR, ncalrpc client/server, endpoint mapper, proxy/stub, internal `I_Rpc*`/`Ndr*` helpers (`rpc_ext_internal`) |
| `sechost_*` | sechost: SCM queries, service control, SID/SD/privileges, ETW |
| `winrt_*` | combase HSTRING and RoXxx |
| `gap_NN` | tests for exports not covered by the themed areas |
| `dx_*` | state-changing areas (see Safety); `dx_ole32_ext`: `CoIsOle1Class` with classes registered under HKCU |

## Safety model

* Non-`dx_` areas must be free of system-wide side effects: they only create objects, files under `%TEMP%\olet_<pid>\`, and
  per-process state (the one exception is a private, deleted-again `HKCU` sandbox key in `rpc_proxy_stub_dcom`). They never
  stop or reconfigure services, never write HKLM/HKCU registration, never start
  ETW sessions, never register in the RPC endpoint mapper, never take clipboard ownership.
* `dx_*` areas change machine or user state (HKCU\Software\Classes registrations, service create/config/delete,
  audit policy, ETW sessions, endpoint-mapper entries, OLE clipboard). They skip (`t_skip`) when the needed privilege
  is missing, sweep leftovers of earlier crashed runs at start (names contain `olet` and a pid) and clean up in a tail.
  `tools\run_all.bat` runs them only with `/destructive`.
* Nothing shuts down, logs off, or kills system processes; child processes are only the exe itself (`--child`)
  and are killed after a bounded wait. Waits are bounded (<= 10-20 s). No modal UI.
* Run on disposable VMs. Wine can still run the corpus headless (`tools/wine_run.sh`, throw-away prefix, never set DISPLAY), but Wine
  results are not authoritative.

## Running on Windows 10

    make                              # on Linux with mingw-w64 (x86_64-w64-mingw32-gcc); copy build\ to the VM
    tools\run_all.bat                 # non-destructive areas
    tools\run_all.bat /destructive    # adds dx_* (elevated)
    python tools\summarize.py build\results --failures

See `HANDOFF.md` for the full procedure and triage guide, `docs/WIN10_NOTES.md` for the packed guest runs (modes `adm`, `std`,
`dx`) and `docs/I7_NOTES.md` for running the exes on the i7 Win10-personality kernel (imports, serial framing, bring-up order).

## NOTICE

Many tests were adapted from the Wine project test suite (dlls/ole32/tests, dlls/combase/tests,
dlls/oleaut32/tests, dlls/rpcrt4/tests), (C) the Wine authors, licensed LGPL-2.1-or-later. They were rewritten for
this load-library harness with Wine-specific scaffolding removed; each carries a provenance comment
`/* adapted from Wine dlls/<dll>/tests/<file>.c (LGPL-2.1+) */`. Those adapted tests remain under the LGPL-2.1+.
The remaining tests were written from the Windows 10 decompilation and public documentation.
