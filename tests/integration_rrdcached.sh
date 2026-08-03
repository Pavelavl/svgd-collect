#!/bin/sh
# End-to-end check of the OPTIONAL rrdcached routing path in writer_rrd.c.
# Requires the rrdcached binary on PATH (skips with exit 0 if absent — this is
# an optional feature check, not part of the default `make test`).
#
# Two scenarios:
#   1. happy path:  rrdcached_addr set + daemon running  -> writes go via rrdc_update
#                   and, after writer_shutdown's flush, RRDs are durable + fetchable.
#   2. fallback:    rrdcached_addr set but daemon absent -> writer logs the connect
#                   failure and falls back to direct rrd_update_r (no samples lost).
set -e

REPO=$(cd "$(dirname "$0")/.." && pwd)
BIN="$REPO/bin/svgd-collect"

if ! command -v rrdcached >/dev/null 2>&1; then
    echo "skip: rrdcached not installed (optional feature)"
    exit 0
fi
if ! command -v rrdtool >/dev/null 2>&1; then
    echo "skip: rrdtool not installed"
    exit 0
fi

fail=0
D=""
D2=""
RC_PID=""
cleanup() {
    [ -n "$RC_PID" ] && kill "$RC_PID" 2>/dev/null || true
    [ -n "$D" ]  && rm -rf "$D"  || true
    [ -n "$D2" ] && rm -rf "$D2" || true
}
trap cleanup EXIT

# ---- 1. happy path: daemon running ----
D=$(mktemp -d)
SOCK="$D/rrdcached.sock"
mkdir -p "$D/rrd"
# -g stays in foreground so we can background it with & and track $!.
# -F flush at shutdown, -R allow nested dirs under -b, -B restrict to base.
rrdcached -g -l "unix:$SOCK" -b "$D/rrd" -B -R -F -w 1 -z 1 -p "$D/rc.pid" >/dev/null 2>&1 &
RC_PID=$!

i=0
while [ $i -lt 30 ] && [ ! -S "$SOCK" ]; do i=$((i+1)); sleep 0.1; done
if [ ! -S "$SOCK" ]; then echo "FAIL: rrdcached socket did not appear"; fail=$((fail+1)); fi

cat > "$D/collect.json" <<EOF
{ "interval": 1, "datadir": "$D/rrd", "hostname": "dhost",
  "rrdcached_addr": "unix:$SOCK", "readers": ["load","uptime"] }
EOF
"$BIN" "$D/collect.json" >/dev/null 2>&1 &
SC=$!
sleep 3
kill -TERM $SC 2>/dev/null || true
wait $SC 2>/dev/null || true

for f in "$D/rrd/dhost/load/load.rrd" "$D/rrd/dhost/uptime/uptime.rrd"; do
    if [ ! -f "$f" ]; then echo "FAIL: $f missing"; fail=$((fail+1)); continue; fi
    if ! rrdtool fetch "$f" AVERAGE --start -120 --end now >/dev/null 2>&1; then
        echo "FAIL: rrdtool fetch failed for $f"; fail=$((fail+1));
    fi
done

# stop the happy-path daemon before the fallback leg
kill "$RC_PID" 2>/dev/null || true
RC_PID=""

# ---- 2. fallback: daemon address dead -> direct writes ----
D2=$(mktemp -d)
mkdir -p "$D2/rrd"
cat > "$D2/collect.json" <<EOF
{ "interval": 1, "datadir": "$D2/rrd", "hostname": "fbhost",
  "rrdcached_addr": "unix:$D2/no-such.sock", "readers": ["uptime"] }
EOF
"$BIN" "$D2/collect.json" 2>"$D2/err.log" &
SC2=$!
sleep 2
kill -TERM $SC2 2>/dev/null || true
wait $SC2 2>/dev/null || true

# must have logged the connect failure (proves the daemon path was attempted)
if ! grep -q "rrdc_connect" "$D2/err.log"; then
    echo "FAIL: no rrdc_connect warning logged on dead daemon"; fail=$((fail+1)); fi
F2="$D2/rrd/fbhost/uptime/uptime.rrd"
if [ ! -f "$F2" ]; then echo "FAIL: fallback $F2 missing"; fail=$((fail+1)); fi
if ! rrdtool fetch "$F2" AVERAGE --start -120 --end now >/dev/null 2>&1; then
    echo "FAIL: rrdtool fetch failed for $F2"; fail=$((fail+1)); fi

if [ "$fail" -ne 0 ]; then echo "FAIL: $fail assertion(s) failed"; exit 1; fi
echo "PASS: rrdcached happy-path writes + dead-daemon fallback both verified"
