#!/usr/bin/env python3
"""Print the area (exe) names for a staged-exe runner, one per line or comma separated.

usage: i7_areas.py [--nondx | --dx | --all] [--order alpha|bringup] [--comma] [--build DIR] [--check]

  --nondx   every non-dx_ area (the default); --dx only the dx_* areas; --all both (non-dx first)
  --order   alpha (default) or bringup (tools/i7_bringup.txt order, smallest first)
  --comma   one comma-separated line (the form an --only style option takes)
  --build   directory holding <area>.exe (default ../build); areas without an exe there are an error
  --check   also verify tools/i7_bringup.txt names every tests/<area> exactly once
dx_ areas change machine state (registry, services, audit policy, clipboard): stage and run them separately, on a
disposable guest."""
import argparse, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)


def areas():
    return sorted(d for d in os.listdir(os.path.join(ROOT, "tests")) if os.path.isdir(os.path.join(ROOT, "tests", d)))


def bringup():
    out = []
    for l in open(os.path.join(HERE, "i7_bringup.txt")):
        l = l.split("#", 1)[0].strip()
        if l:
            out.append(l)
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--nondx", action="store_true"); g.add_argument("--dx", action="store_true"); g.add_argument("--all", action="store_true")
    ap.add_argument("--order", choices=["alpha", "bringup"], default="alpha")
    ap.add_argument("--comma", action="store_true"); ap.add_argument("--check", action="store_true")
    ap.add_argument("--build", default=os.path.join(ROOT, "build"))
    a = ap.parse_args(argv)
    have = areas(); order = bringup()
    if a.check:
        bad = sorted(set(have) ^ set(order)) + sorted({x for x in order if order.count(x) > 1})
        if bad:
            sys.stderr.write("i7_bringup.txt mismatch: %s\n" % " ".join(bad)); return 1
    lst = have if a.order == "alpha" else order
    nondx = [x for x in lst if not x.startswith("dx_")]; dx = [x for x in lst if x.startswith("dx_")]
    sel = dx if a.dx else (nondx + dx if a.all else nondx)
    miss = [x for x in sel if not os.path.isfile(os.path.join(a.build, x + ".exe"))]
    if miss:
        sys.stderr.write("no exe in %s for: %s (run make)\n" % (a.build, " ".join(miss))); return 2
    print(",".join(sel) if a.comma else "\n".join(sel))
    return 0


if __name__ == "__main__":
    sys.exit(main())
