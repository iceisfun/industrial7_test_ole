# Windows 10 run notes

Windows 10 22H2 x64 is the authority for every expectation in this corpus. This file describes how the corpus is
packed, run on a throw-away Windows 10 guest and read back, and records what is environment dependent.
(`HANDOFF.md` covers manual runs on a machine you operate by hand.)

## Tools (all under `tools/`)

| file | purpose |
|---|---|
| `win10_pack.py` | build a guest payload (`bin/`, `list.txt`, `runner.ps1`, `run.cmd`) from `build/*.exe` |
| `guest/runner.ps1` | guest side: runs the listed exes one at a time per mode, 600 s limit each, frames every output line on COM1 |
| `guest/olet_runas.c` | helper: launches a command with a restricted Medium-integrity copy of the session token (std mode) |
| `guest/olet_probe.c` | helper: prints the identity (integrity, elevation, privileges) a mode actually runs with |
| `win10_run.sh` | host-side driver: ships a payload to a guest launcher (parametrised through `OLET_ORACLE_*` environment variables), fetches `serial.log` |
| `win10_results.py` | parse a `serial.log` into per-exe / per-test tables (tsv or json) |
| `win10_summarize.py` | write the committed per-mode summaries (counts and failing/skipped tests only, never raw logs) |
| `win10_compare.py` | compare several logs of the same build for determinism |
| `test_win10_results.py` | unit test of the parser (including the i7 conversion) |
| `i7_serial.py`, `i7_areas.py`, `i7_bringup.txt` | i7 serial-log conversion and area lists, see `docs/I7_NOTES.md` |

## Modes

- `adm`: the guest's autologon token, an elevated local Administrator (High integrity).
- `std`: a restricted copy of that token in the same interactive session (Medium integrity, only the change-notify
  privilege, Administrators deny-only). This is a restricted-admin token, not a real standard account; `TokenIsElevated`
  can still read 1 on it, so tests decide on integrity level, privileges or the observed refusal.
- `dx`: the elevated token again, but runs only the `dx_*` areas (destructive pass: registry registrations under
  HKCU\Software\Classes, service create/delete, audit policy, ETW sessions, endpoint-map entries, clipboard). `dx_*`
  areas are refused in `adm`/`std` payloads and nothing else is accepted in a `dx` payload. Use a disposable guest.

Areas run strictly one after another, never two exes at once; a per-exe limit of 600 s kills the whole process tree
and reports the exe as TIMEOUT. Scratch files are under `%TEMP%\olet_<pid>`.

## Output grammar

Test exes print `RUN name`, then `OK name` / `FAIL name` / `SKIP name` (two spaces after `OK`), with
`  FAIL file:line expr (detail)`, `  SKIP reason` and `  NOTE text` lines before the result line, and finally
`SUMMARY tests= failed= skipped= checks_pass= checks_fail=`. The runner wraps each line as `OLET out <exe> | <line>` and
adds `OLET begin|os|mode|start|rc|done` markers. A (mode, exe) row has status OK, TIMEOUT, NOSUMMARY (crash: no SUMMARY),
HANG (log stops inside the exe) or MISSING. The payload counts as finished only when `OLET done` is present.

## How to run

    make clean && make -j8                                  # build/*.exe (66 areas) and build/tools/*.exe
    python3 tools/win10_pack.py PAY --all --mode both       # every non-dx_ area, adm then std
    python3 tools/win10_pack.py PAY --all --mode dx         # only the dx_* areas
    python3 tools/win10_pack.py PAY --exes core_smoke,bstr_core --mode both --timeout 120 [--filter SUBSTR] [--full]

    export OLET_ORACLE_SSH=user@host OLET_ORACLE_DIR=/path/to/launcher/dir OLET_ORACLE_RUN=launcher.sh
    tools/win10_run.sh PAY RUNDIR [guest_timeout_s]         # RUNDIR/serial.log

    python3 tools/win10_results.py RUNDIR/serial.log [--tests | --failures | --format json]
    python3 tools/win10_summarize.py results/win10 RUNDIR/serial.log [more.log ...]
    python3 tools/win10_compare.py run1/serial.log run2/serial.log --lines

The launcher boots a fresh disposable Windows 10 guest, runs `run.cmd` of the payload at logon with COM1 captured,
and powers off. Keep a payload to roughly ten minutes of test time per guest boot; split larger sets into several
payloads and combine the logs with `win10_summarize.py` (later logs replace earlier rows of the same (mode, exe)).
`results/` is for local use; commit only the sanitized summaries written by `win10_summarize.py`.

## Windows build of the reference runs

Recorded per run in the `OLET os` line (build, version, SKU). Pipeline smoke run (`core_smoke` and `bstr_core`, adm and
std, fresh guest): Windows 10 build 19045 (10.0.19045, 22H2), SKU 101 (Home), 64-bit. adm token: High integrity,
elevated, Administrators enabled. std token: Medium integrity, only the change-notify privilege, Administrators
deny-only. Both exes: 0 failed, 0 skipped (core_smoke 3 tests, bstr_core 113 tests). Locale NOTE lines print `?` for
non-ASCII text because the runner replaces non-printable bytes before framing them on COM1.
