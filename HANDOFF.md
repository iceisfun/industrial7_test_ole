# HANDOFF: running the ole32 corpus on Windows 10 x64

For whoever executes the corpus on a Windows 10 machine (or on the i7 kernel) and feeds corrections back.

## Status
**Windows 10 verified.** Windows 10 22H2 build 19045 (10.0.19045, x64), three fresh guests per pass, final run: 66 exes, 8504 tests.

| mode | exes | tests | failed | skipped | checks failed |
|---|---|---|---|---|---|
| `adm` (elevated administrator) | 50 | 7924 | 0 | 4 | 0 |
| `std` (restricted-admin token) | 50 | 7924 | 0 | 18 | 0 |
| `dx` (destructive `dx_*` areas, elevated) | 16 | 580 | 0 | 2 | 0 |

Per-test results are identical across the three guests, with two exceptions that are not expectation problems: one `NOTE` line
in `picture_font_ole` prints a bitmap handle value, and on one of three `dx` guests `dx_sechost_service_lifecycle` hit the
600 s limit inside an SCM enumeration right after `DeleteService` (the other two guests passed all 153 tests): a guest SCM
wedge, not root-caused. Every remaining skip is listed in `COVERAGE.md`. The corpus was first written and checked under Wine only;
the old Wine failure counts are obsolete. Windows is the authority: when a test and Windows disagree, fix the test (narrow the
check to what Windows does, `t_note` for version dependence, `t_skip` with a reason for environment dependence).

## Modes
- `adm`: elevated local administrator (High integrity). `std`: a restricted Medium-integrity copy of that token (change-notify
  privilege only, Administrators deny-only); not a real standard account. `dx`: elevated again, only the `dx_*` areas, which change
  machine state, on a disposable guest. `tools/win10_pack.py --mode both|adm|std|dx` builds the payload for each; details in
  `docs/WIN10_NOTES.md`.

## Purpose
This repo is a black-box conformance corpus for ole32.dll, combase.dll, oleaut32.dll, rpcrt4.dll and sechost.dll.
Each test does LoadLibrary -> GetProcAddress -> call; no DLL under test is linked. Expectations came from the
Win10 decompilation, MS docs and (rewritten) Wine tests and have been reconciled with real Windows 10 results, which are the authority.

## Prerequisites
- Disposable Windows 10 x64 VM (21H2/22H2 preferred; record the build with `ver`), with a snapshot. No domain.
- Two passes matter: elevated administrator and a reduced token (the packed runs use a restricted copy of the admin token; a real standard account is untested).
- An interactive desktop session (clipboard and window tests need it; they t_skip otherwise).
- Build host: Linux with mingw-w64 (`x86_64-w64-mingw32-gcc`) and make. Binaries are static; copy `build\` to the VM.
- Python 3 on the VM (optional) for `tools\summarize.py`.

## Build
    make clean && make -j8        # build/<area>.exe, 66 exes, zero errors expected (warnings are fine); also builds build/tools/*.exe

## Run on a guest you drive by hand (revert the snapshot between passes)
For automated runs use the packed flow instead: `python3 tools/win10_pack.py PAY --all --mode both` (or `--mode dx`), ship the payload
to a disposable guest with `tools/win10_run.sh`, read the serial log with `python3 tools/win10_results.py serial.log [--tests|--failures]`,
write sanitized summaries with `tools/win10_summarize.py`, compare logs of one build with `tools/win10_compare.py`. See `docs/WIN10_NOTES.md`.
Keep a payload to about ten minutes of test time per boot. Tool tests: `python3 tools/test_win10_results.py`.

1. Standard user:        `tools\run_all.bat`
2. Elevated admin:       `tools\run_all.bat`
3. Elevated admin, destructive: `tools\run_all.bat /destructive`   (adds all `dx_*` areas)
One area / one test: `build\<area>.exe <name-substring>`.
Many exes re-launch themselves as `--child` helpers; the whole run takes roughly 20-40 minutes by hand. Hard limit per exe: 600 s.
Never run two exes at once (named RPC endpoints and ROT entries are keyed on pid, but service and ETW names are not).

## Collect results
`run_all.bat` writes `build\results\<area>.txt`. Zip that folder, and also run
`python tools\summarize.py build\results --failures > failures.txt`. Output grammar: `RUN/OK/FAIL/SKIP name`,
`  FAIL file:line expr (detail)` per failed check, `  NOTE ...` for observations, `SUMMARY tests= failed= skipped= checks_pass= checks_fail=`.
A missing SUMMARY means a crash or timeout: report the last RUN line.

## Triage
Classify every failing check (a clean Windows 10 run has none; use this when a build, SKU or new environment shows one):
1. Wrong expectation (Windows differs from the test): check the decomp
   (the Win10 decompilation of the function), fix the test, rebuild, re-run that test.
2. Environment/SKU/locale/privilege: turn into `t_note` or `t_skip`.
3. Real divergence: this is what the corpus is for; record it as a finding with the expected and observed value.
`NOWINE()` / `t_is_wine()` guards are no-ops on Windows.
NOTE lines with version-dependent values (OleBuildVersion, locale month names, SKU) should be compared with the build you run.

## Expected skips (measured on build 19045)
- `adm`: 4 (`scl_sd_set_null_descriptor`, `scl_openservice_stop_right_denied_for_users`, `svcdyn_not_a_service_process`, `svcdyn_null_handle`).
- `std`: 18 (restricted token: trace security key read, start right on RpcSs, SC_MANAGER_LOCK, service dependency handles, fewer than two
  visible trace sessions, plus three of the `adm` skips).
- `dx`: 2 (`dx_gap_07` endpoint-inquiry tests: "cannot start ncalrpc server").
- The full list with reasons is in `COVERAGE.md`. Clipboard and window tests need an interactive desktop session (the guest autologon
  session); in a service or ssh session they `t_skip`.
- Missing exports on older builds show up as `missing export: ...` skips; any such skip on Win10 19041+ is a finding.

## Known weak spots
- Cross-process areas (`com_marshal_xproc`, `rpc_server_client_ncalrpc`, `monikers_rot`) depend on timing of child start; a timeout there is first an environment suspect (AV, slow VM).
- `dx_sechost_service_lifecycle` is the one timing-sensitive area (SCM deletion lag; 1 of 3 guests wedged).
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

## i7 (Win10-personality kernel)
The same exes run unchanged on the i7 kernel through its staged-exe serial-log runner. `docs/I7_NOTES.md` has the import table per area
(everything the exes import is `kernel32 msvcrt user32 advapi32`; `ole32` and `oleaut32` are loaded at run time and must be provided),
the output contract (unbuffered stdout, `RUN/OK/FAIL/SKIP/SUMMARY` grammar, `--child` re-launch), the Windows 10 expected totals per
area and a bring-up order smallest first. Tools: `tools/i7_areas.py` (area lists: `--nondx`, `--dx`, `--all`, `--order bringup`,
`--comma`) and `win10_results.py --i7 RUN_DIR AREA...` / `tools/i7_serial.py` (convert the serial boot logs into the framed Windows
grammar, then the usual `--tests`/`--failures` views and `win10_compare.py` apply). Compare an i7 run with the Windows tables per
test, `adm` being the closest mode.
