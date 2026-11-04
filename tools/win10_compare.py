#!/usr/bin/env python3
"""Compare several Windows 10 run logs (COM1 serial logs, see win10_results.py) for determinism.

usage: win10_compare.py <serial.log> <serial.log> [...] [--lines]

Per (mode, exe) it reports tests whose result differs between the logs and, with --lines, every printed output line
(NOTE, FAIL and SKIP detail, test results) that is not present the same number of times in all logs. Timing fields
(the rc seconds) are ignored. Exit code 0 when all logs agree, else 1."""
import collections, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import win10_results as wr

# Values that legitimately differ between boots (addresses, ids, timings, generated names, file times) are masked in
# NOTE detail lines so --lines only reports differences in what a test observed about the system.
_VOLATILE = [
    (re.compile(r"\b[0-9A-Fa-f]{12,16}\b"), "<ptr>"),                      # pointers, OXIDs, file times
    (re.compile(r"\b[0-9A-Fa-f]{8}:[0-9A-Fa-f]{8}\b"), "<ft>"),
    (re.compile(r"\b0x[0-9A-Fa-f]{9,16}\b"), "<ptr>"),
    (re.compile(r"\b(pid|tid|cookie|atom|port)[= ]\d+", re.I), r"\1=<n>"),
    (re.compile(r"\b(\d+) ?ms\b"), "<ms>"),
    (re.compile(r"\b(after|first|last) \d+\b"), r"\1 <n>"),
    (re.compile(r"ROOT\\OLET\\\d+"), "ROOT-OLET-<n>"),
    (re.compile(r"\b(needed|returned|holds|process) \d+"), r"\1 <n>"),
    (re.compile(r"needed=\d+"), "needed=<n>"),
    (re.compile(r"\d+ GUIDs"), "<n> GUIDs"),
    (re.compile(r"\b\d+ element\(s\)"), "<n> element(s)"),
    (re.compile(r"(\bc=|\bm=|\ba=)[0-9a-f]+:[0-9a-f]+"), r"\1<ft>"),
    (re.compile(r"service process \d+"), "service process <n>"),
    (re.compile(r"olet_[0-9A-Za-z]+(_[0-9A-Za-z]+)*"), "olet_<id>"),
    (re.compile(r"~DF[0-9A-F]+\.TMP"), "~DF<id>.TMP"),
    (re.compile(r"LRPC-[0-9a-f]+"), "LRPC-<id>"),
    (re.compile(r"\b[0-9a-f]{8}\b(?=[:,\s]|$)"), "<h8>"),
]
def mask(body):
    if not body.lstrip().startswith("NOTE"): return body
    for rx, rep in _VOLATILE: body = rx.sub(rep, body)
    return body

def outlines(text):
    d = collections.defaultdict(collections.Counter); mode = "?"
    for raw in text.splitlines():
        i = raw.find("OLET ")
        if i < 0: continue
        line = raw[i:].rstrip("\r\n")
        if line.startswith("OLET mode "): mode = line[10:].strip()
        elif line.startswith("OLET out "):
            exe, _, body = line[9:].partition(" | ")
            body = body.rstrip()
            if body.startswith("SUMMARY"):   # the number of checks varies with the system objects an area enumerates
                body = " ".join(w for w in body.split() if not w.startswith("checks_pass="))
            d[(mode, exe.strip())][mask(body)] += 1
    return d

def main(argv):
    lines = "--lines" in argv
    logs = [a for a in argv if not a.startswith("--")]
    texts = [open(p, errors="replace").read() for p in logs]
    parsed = [wr.parse(t) for t in texts]
    bad = 0
    tables = []
    for p in parsed:
        tab = {}
        for r in p["runs"]:
            tab[(r["mode"], r["exe"])] = (r["status"], dict((n, s) for s, n in r["results"]))
        tables.append(tab)
    keys = sorted(set().union(*[t.keys() for t in tables]))
    for k in keys:
        sts = [t.get(k, ("ABSENT", {}))[0] for t in tables]
        if len(set(sts)) > 1: print("%s %s status differs: %s" % (k[0], k[1], sts)); bad += 1
        names = set().union(*[t.get(k, ("", {}))[1].keys() for t in tables])
        for n in sorted(names):
            rs = [t.get(k, ("", {}))[1].get(n, "-") for t in tables]
            if len(set(rs)) > 1: print("%s %s %s: %s" % (k[0], k[1], n, rs)); bad += 1
    if lines:
        ol = [outlines(t) for t in texts]
        for k in keys:
            allk = set().union(*[o.get(k, {}).keys() for o in ol])
            for ln in sorted(allk):
                c = [o.get(k, {}).get(ln, 0) for o in ol]
                if len(set(c)) > 1: print("LINE %s %s %s: %s" % (k[0], k[1], ln[:200], c)); bad += 1
    print("# %d difference(s) over %d logs" % (bad, len(logs)))
    return 1 if bad else 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
