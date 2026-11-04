# HANDOFF: running the ole32 corpus on Windows 10 x64

For the lane that executes the corpus on a genuine Windows 10 machine and feeds corrections back.

## Purpose
This repo is a black-box conformance corpus for ole32.dll, combase.dll, oleaut32.dll, rpcrt4.dll and sechost.dll.
Each test does LoadLibrary -> GetProcAddress -> call; no DLL under test is linked. Expectations come from the
Win10 decompilation, MS docs and (rewritten) Wine tests. Windows 10 results are the authority: so far the corpus
has only been run under Wine (headless), so expect some wrong expectations on the first real run.

## Prerequisites
- Disposable Windows 10 x64 VM (21H2/22H2 preferred; record the build with `ver`), with a snapshot. No domain.
- Two accounts: a standard user and an elevated administrator. Both passes matter.
- An interactive desktop session (clipboard and window tests need it; they t_skip otherwise).
- Build host: Linux with mingw-w64 (`x86_64-w64-mingw32-gcc`) and make. Binaries are static; copy `build\` to the VM.
- Python 3 on the VM (optional) for `tools\summarize.py`.

## Build
    make clean && make -j8        # build/<area>.exe, 66 exes, zero errors expected (warnings are fine)

## Run (revert the snapshot between passes)
1. Standard user:        `tools\run_all.bat`
2. Elevated admin:       `tools\run_all.bat`
3. Elevated admin, destructive: `tools\run_all.bat /destructive`   (adds all `dx_*` areas)
One area / one test: `build\<area>.exe <name-substring>`.
Each exe has children it re-launches itself; the whole run takes roughly 20-40 minutes. Hard limit per exe: 600 s.
Never run two exes at once (named RPC endpoints and ROT entries are keyed on pid, but service and ETW names are not).

## Collect results
`run_all.bat` writes `build\results\<area>.txt`. Zip that folder, and also run
`python tools\summarize.py build\results --failures > failures.txt`. Output grammar: `RUN/OK/FAIL/SKIP name`,
`  FAIL file:line expr (detail)` per failed check, `  NOTE ...` for observations, `SUMMARY tests= failed= skipped= checks_pass= checks_fail=`.
A missing SUMMARY means a crash or timeout: report the last RUN line.

## Triage
Classify every failing check:
1. Wrong expectation (Windows differs from the test): check the decomp
   (`http://docker1.dev.moonlightcompanies.com:8765/api/function/<module>/<Name>/decomp`), fix the test, rebuild, re-run that test.
2. Environment/SKU/locale/privilege: turn into `t_note` or `t_skip`.
3. Real divergence: this is what the corpus is for; record it as a finding with the expected and observed value.
Tests tagged `NOWINE()` / `t_is_wine()` guards are no-ops on Windows: if they now fail, the expectation is probably wrong.
Wine-observed failures (see COVERAGE.md "Wine status") are most likely wrong expectations OR Wine bugs; do not trust either side until Windows says so.
NOTE lines with version-dependent values (OleBuildVersion, locale month names, SKU) should be compared with the build you run.

## Expected skips
- Standard user: `dx_*` (not run), ETW session start (`sechost_etw_trace` live tests, `dx_gap_03`), SCM write rights (`dx_sechost_service_lifecycle`), audit policy (`dx_gap_00`, needs SeSecurityPrivilege), endpoint-map registration may skip when RpcSs is unreachable.
- Non-interactive session (service/ssh): clipboard (`dx_ole2_clipboard`), drag-and-drop window tests, picture/font tests that need a window.
- Missing exports on older builds show up as `missing export: ...` skips; any such skip on Win10 19041+ is a finding.

## Known weak spots
- Areas with the most Wine failures (COVERAGE.md) are the ones likeliest to contain wrong expectations.
- Cross-process areas (`com_marshal_xproc`, `rpc_server_client_ncalrpc`, `monikers_rot`) depend on timing of child start; a timeout there is first an environment suspect (AV, slow VM).
- `dx_*` areas were validated only partially under Wine (privileged paths skip there).
- 63 public names are intentionally uncovered (NdrProxyForwardingFunctionN, ObjectStublessClientN, a few *64 marshal helpers).

## Cleanup of leftovers (after a crashed destructive run)
All names carry the prefix `olet`:
    sc query state= all | findstr /i olet           & sc delete <name>
    reg query HKCU\Software\Classes /f olet /k      & reg delete <key> /f
    reg query HKCU\Software\Classes\CLSID /f olet /k
    reg query HKCU\Software\Classes\Interface /f olet /k
    reg query HKCU\Software\Classes\TypeLib /f olet /k
    logman query -ets | findstr /i olet             & logman stop <name> -ets
    rmdir /s /q %TEMP%\olet_*
    taskkill /f /im <area>.exe                       (only the corpus exes)
Audit policy (`dx_gap_00`) restores itself; if interrupted, `auditpol /get /category:*` and revert from the snapshot.
Revert the snapshot when in doubt.

## Report back
- Windows build number, user/admin, session type.
- Per area: tests / failed / skipped / checks_failed (the summarize output).
- Wrong expectations fixed (test name, old vs new expected value, evidence).
- Real divergences (function, inputs, expected from decomp, observed).
- Areas that could not run or crashed, and tests that needed new guards.
- Commit fixes with specific `git add tests/<area>`; do not push unless asked.
