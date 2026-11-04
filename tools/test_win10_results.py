#!/usr/bin/env python3
"""Unit test for win10_results.py with a fabricated log. Run: python3 tools/test_win10_results.py"""
import os, sys, tempfile, unittest
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import win10_results as W
import i7_serial as I

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

def con(text):
    """one CON: record per character, the CR of each line end shown as '.'"""
    return "".join("CON: %s\n" % c for c in text.replace("\n", ".")) 

class I7(unittest.TestCase):
    def run_dir(self, extra_results=None):
        d = tempfile.mkdtemp(); os.makedirs(os.path.join(d, "logs"))
        log = "kernel noise\n"
        log += "CON: ##BEGIN a\n" + con("RUN  t1\nOK   t1\n  NOTE v=1.\nRUN  t2\nSKIP t2\nSUMMARY tests=2 failed=0 skipped=1 checks_pass=3 checks_fail=0\n") + "CON: ##END a rc=0x0 ms=1500\n"
        log += "CON: ##BEGIN b\n" + con("RUN  u1\nFAIL u1\n") + "CON: ##END b rc=0xc0000005 ms=20\n"
        log += "CON: ##BEGIN c\nCON: ##END c TIMEOUT rc=0x0 ms=60000\n"
        log += "CON: ##BEGIN d\n" + con("RUN  w1\n")
        open(os.path.join(d, "logs", "boot-001.log"), "w").write(log)
        if extra_results: open(os.path.join(d, "results.tsv"), "w").write(extra_results)
        return d
    def test_i7(self):
        d = self.run_dir()
        r = W.parse(I.to_olet(d, ["a", "b", "c", "d", "e"]))
        runs = {x["exe"]: x for x in r["runs"]}
        a = runs["a"]
        self.assertEqual((a["tests"], a["failed"], a["skipped"], a["checks_pass"], a["status"], a["secs"]), (2, 0, 1, 3, "OK", 1.5))
        self.assertEqual(a["results"], [("OK", "t1"), ("SKIP", "t2")])
        self.assertEqual(a["details"], ["NOTE v=1."])
        self.assertEqual(runs["b"]["status"], "NOSUMMARY")
        self.assertEqual(runs["c"]["status"], "TIMEOUT")
        self.assertEqual(runs["d"]["status"], "HANG")
        self.assertEqual(runs["e"]["status"], "MISSING")
        self.assertTrue(r["done"])
    def test_i7_crash_kernel(self):
        d = self.run_dir("id\tstatus\nb\tCRASH-KERNEL\n")
        runs = {x["exe"]: x for x in W.parse(I.to_olet(d, ["b"]))["runs"]}
        self.assertEqual(runs["b"]["status"], "HANG")

class Areas(unittest.TestCase):
    def test_bringup_covers_tests(self):
        import i7_areas
        self.assertEqual(sorted(i7_areas.bringup()), i7_areas.areas())
        self.assertEqual(len(i7_areas.bringup()), len(set(i7_areas.bringup())))

if __name__ == "__main__": unittest.main()
