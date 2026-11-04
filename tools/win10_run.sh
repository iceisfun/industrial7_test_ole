#!/bin/bash
# Host-side driver: run one packed payload on a throw-away Windows 10 guest and fetch the framed COM1 log.
#   tools/win10_run.sh <payload_dir> <out_dir> [guest_timeout_s]
# The payload comes from tools/win10_pack.py. Nothing about the oracle host is stored in the repo; set
#   OLET_ORACLE_SSH   ssh target of a host that can boot guests (user@host)
#   OLET_ORACLE_DIR   directory on that host holding the guest launcher script and a jobs/ folder
#   OLET_ORACLE_RUN   launcher script name inside that directory (default vm-run.sh); it takes
#                     <payload_dir> <out_dir> [timeout] and writes <out_dir>/serial.log
# The launcher is expected to serialise guests with its own lock, so this script never starts more than one guest.
# Job folders are named ole-* and are removed here when the run is over. Result: <out_dir>/serial.log.
set -eu
P=${1:?payload dir}; O=${2:?out dir}; T=${3:-3600}
H=${OLET_ORACLE_SSH:?set OLET_ORACLE_SSH}; D=${OLET_ORACLE_DIR:?set OLET_ORACLE_DIR}; R=${OLET_ORACLE_RUN:-vm-run.sh}
J="$D/jobs/ole-$(basename "$O")-$$"
mkdir -p "$O"
ssh -o BatchMode=yes "$H" "mkdir -p '$J/payload' '$J/out'"
rsync -a --delete "$P"/ "$H:$J/payload/"
rc=0
ssh -o BatchMode=yes "$H" "'$D/$R' '$J/payload' '$J/out' $T" >"$O/launcher.out" 2>&1 || rc=$?
scp -q -o BatchMode=yes "$H:$J/out/serial.log" "$O/serial.log" || true
ssh -o BatchMode=yes "$H" "rm -rf '$J'" || true
tail -n 3 "$O/launcher.out"
echo "launcher rc=$rc"
[ -s "$O/serial.log" ] || { echo "no serial.log"; exit 1; }
exit $rc
