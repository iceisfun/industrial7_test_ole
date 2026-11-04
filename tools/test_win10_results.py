#!/usr/bin/env python3
"""Unit test for win10_results.py with a fabricated log. Run: python3 tools/test_win10_results.py"""
import os, sys, unittest
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import win10_results as W

LOG = """boot noise
OLET begin modes=adm,std exes=4 timeout=60
OLET os build=19045 version=10.0.19045 sku=48 arch=64-bit
OLET mode adm
OLET start a
OLET out a | RUN  t1
OLET out a | OK   t1
OLET out a |   FAIL x.c:3: CHECK(x) (got 1)
OLET out a | FAIL t2
OLET out a | SKIP t3
OLET out a |   SKIP no touch
OLET out a | SUMMARY tests=3 failed=1 skipped=1 checks_pass=5 checks_fail=1
OLET rc a 1 1.5
OLET start b
OLET out b | OK   u1
OLET rc b 0xc0000005 0.2
OLET start c
OLET rc c TIMEOUT 60
OLET mode std
OLET start d
OLET out d | OK   v1
OLET mode dx
OLET start e
OLET out e | SUMMARY tests=1 failed=0 skipped=0 checks_pass=2 checks_fail=0
OLET rc e 0 3.0
OLET done
"""
class T(unittest.TestCase):
    def test_parse(self):
        r = W.parse(LOG); runs = {(x["mode"], x["exe"]): x for x in r["runs"]}
        a = runs[("adm", "a")]
        self.assertEqual((a["tests"], a["failed"], a["skipped"], a["checks_fail"], a["status"]), (3, 1, 1, 1, "OK"))
        self.assertEqual(a["results"], [("OK", "t1"), ("FAIL", "t2"), ("SKIP", "t3")])
        self.assertEqual(runs[("adm", "b")]["status"], "NOSUMMARY")
        self.assertEqual(runs[("adm", "c")]["status"], "TIMEOUT")
        self.assertEqual(runs[("std", "d")]["status"], "HANG")
        self.assertEqual(runs[("dx", "e")]["status"], "OK")
        self.assertTrue(r["done"])
if __name__ == "__main__": unittest.main()
