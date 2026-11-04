#!/usr/bin/env python3
"""Convert the serial logs of a staged-exe runner (the i7 Win10-personality kernel) into the framed grammar that
win10_results.py reads, so the same tables, --tests and --failures views work on an i7 run.

usage: i7_serial.py <run_dir> <area>... [-o serial.olet.log]

<run_dir>/logs/boot-*.log   guest COM1 logs. The runner prints, per staged exe, a line `##BEGIN <id>`, then the exe's
                            console output as `CON: <text>` records, then `##END <id> rc=0x<hex> ms=<n>` (the END line
                            may carry TIMEOUT, or CREATEFAIL/PARAMFAIL with st=<ntstatus>). Other lines (kernel
                            output, ##PROGRESS and similar markers) are ignored.
<run_dir>/results.tsv       optional scheduler verdicts (header row, columns id and status): CRASH-KERNEL / NOTRUN
                            mean the guest died while the exe ran, or it never got a verdict.
(a `gate/` sub-directory of <run_dir> holding logs/ and results.tsv is accepted too)

Console records may arrive one per written character (unbuffered stdout) and the CR of each CRLF may show as a
trailing '.', so the records are joined and cut at a '.' that is followed by a known corpus line start. The output is
`OLET begin/os/mode/start/out/rc/done` lines (mode `i7`); an exe that did not end cleanly gets no `OLET rc` line (shown
as HANG by win10_results.py), a timed-out exe gets `rc TIMEOUT`, a refused launch gets `rc MISSING` plus an
`OLET out` line `I7 CREATEFAIL st=...`. Use `win10_results.py --i7 <run_dir> <area>...` to do both steps in one go."""
import glob, os, re, sys

LINE_START = re.compile(r"\.(?=(?:RUN  |OK   |FAIL |SKIP |NOTE |SUMMARY |FATAL |  FAIL |  NOTE |  SKIP |  INFO ))")


def reassemble(recs):
    parts = LINE_START.split("".join(recs))
    if parts and parts[-1].endswith("."):
        parts[-1] = parts[-1][:-1]
    return [p for p in parts if p != ""]


def parse_logs(run):
    base = run if os.path.isdir(os.path.join(run, "logs")) else os.path.join(run, "gate")
    tests = {}
    for path in sorted(glob.glob(os.path.join(base, "logs", "boot-*.log"))):
        cur = None
        for raw in open(path, errors="replace").read().split("\n"):
            line = raw.rstrip("\r\x00").lstrip("\x00 ")
            if not line.startswith("CON: "):
                continue
            con = line[5:]
            if con.startswith("##BEGIN "):
                cur = tests[con.split()[1]] = dict(recs=[], rc=None, ms=None, timeout=False, createfail=None, ended=False)
            elif con.startswith("##END "):
                tok = con.rstrip().split()
                t = tests.get(tok[1])
                if t is not None:
                    t["ended"] = True
                    kv = dict(x.split("=", 1) for x in tok[2:] if "=" in x)
                    if "CREATEFAIL" in tok or "PARAMFAIL" in tok:
                        t["createfail"] = kv.get("st", "?").rstrip(".")
                    else:
                        try:
                            t["rc"] = int(kv["rc"].rstrip("."), 16)
                        except Exception:
                            pass
                        t["ms"] = kv.get("ms", "").rstrip(".")
                        t["timeout"] = "TIMEOUT" in tok or "TIMEOUT." in tok
                cur = None
            elif con.startswith("##"):
                continue
            elif cur is not None:
                cur["recs"].append(con)
    for t in tests.values():
        t["lines"] = reassemble(t["recs"])
    return tests


def verdicts(run):
    v = {}
    for p in (os.path.join(run, "results.tsv"), os.path.join(run, "gate", "results.tsv")):
        if os.path.isfile(p):
            rows = [l.rstrip("\n").split("\t") for l in open(p, errors="replace")]
            for r in rows[1:]:
                d = dict(zip(rows[0], r))
                v[d.get("id", r[0])] = d.get("status", "")
            break
    return v


def to_olet(run, areas):
    tests = parse_logs(run)
    gv = verdicts(run)
    out = ["OLET begin modes=i7 exes=%d" % len(areas), "OLET os build=i7 personality=win10", "OLET mode i7"]
    for a in areas:
        out.append("OLET start " + a)
        t = tests.get(a)
        if t is None:
            out.append("OLET rc %s MISSING 0" % a)   # never got a BEGIN: not run
            continue
        out += ["OLET out %s | %s" % (a, l) for l in t["lines"]]
        if gv.get(a) in ("CRASH-KERNEL", "NOTRUN"):
            out.append("OLET out %s | I7 %s" % (a, gv[a]))
            continue                                   # no rc line: HANG
        if t["createfail"] is not None:
            out.append("OLET out %s | I7 CREATEFAIL st=%s" % (a, t["createfail"]))
            out.append("OLET rc %s MISSING 0" % a)
        elif not t["ended"]:
            pass                                       # log stops inside the exe: HANG
        else:
            secs = "%.1f" % (int(t["ms"]) / 1000.0) if (t["ms"] or "").isdigit() else "0"
            rc = "TIMEOUT" if t["timeout"] else (str(t["rc"]) if t["rc"] is not None and t["rc"] < 256 else
                                                  ("0x%x" % t["rc"] if t["rc"] is not None else "-1"))
            out.append("OLET rc %s %s %s" % (a, rc, secs))
    out.append("OLET done")
    return "\n".join(out) + "\n"


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    dest = None
    if "-o" in argv:
        i = argv.index("-o"); dest = argv[i + 1]; del argv[i:i + 2]
    if len(argv) < 2:
        sys.stderr.write(__doc__.split("\n")[0] + "\nusage: i7_serial.py <run_dir> <area>... [-o FILE]\n")
        return 64
    text = to_olet(argv[0], argv[1:])
    if dest:
        open(dest, "w").write(text)
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
