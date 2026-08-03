#!/bin/sh
# Run svgd-collect briefly against real /proc, verify several drop-in RRDs are
# produced + fetchable. Config enables cpu/load/uptime/memory, each of which
# populates within the ~7s run on any host. (interface/disk/df/processes are
# more environment-sensitive and are intentionally not asserted here.)
set -e
D=$(mktemp -d)
cat > "$D/collect.json" <<EOF
{ "interval": 2, "datadir": "$D/rrd", "hostname": "testhost", "readers": ["cpu","load","uptime","memory"] }
EOF
./bin/svgd-collect "$D/collect.json" &
PID=$!
sleep 7
kill -TERM $PID 2>/dev/null || true
wait $PID 2>/dev/null || true

# Each entry is a drop-in path svgd would read. cpu needs two samples (delta
# reader) so it is only asserted if present; load/uptime/memory populate on
# the first sample and MUST exist.
CPU="$D/rrd/testhost/cpu-total/percent-active.rrd"
LOAD="$D/rrd/testhost/load/load.rrd"
UPTIME="$D/rrd/testhost/uptime/uptime.rrd"
MEMORY="$D/rrd/testhost/memory/percent-used.rrd"

ok=0
fail=0
# fetch-and-count helper: asserts the file exists AND rrdtool fetch AVERAGE works.
check() {
    F="$1"
    if [ ! -f "$F" ]; then
        echo "FAIL: drop-in RRD not created at $F"
        fail=$((fail + 1))
        return
    fi
    if ! rrdtool fetch "$F" AVERAGE --start -120 --end now >/dev/null 2>&1; then
        echo "FAIL: rrdtool fetch failed for $F"
        fail=$((fail + 1))
        return
    fi
    ok=$((ok + 1))
}

# Required: load, uptime, memory populate on the first interval.
check "$LOAD"
check "$UPTIME"
check "$MEMORY"
# cpu is a delta reader (needs two samples to emit); best-effort, not fatal.
if [ -f "$CPU" ]; then
    check "$CPU"
else
    echo "note: cpu RRD not yet present (delta reader; needs another interval) - skipping"
fi

if [ "$fail" -ne 0 ]; then
    echo "FAIL: $fail assertion(s) failed, $ok RRD(s) verified"
    rm -rf "$D"
    exit 1
fi
echo "PASS: $ok drop-in RRD(s) created and fetchable (load/uptime/memory required + cpu best-effort)"
rm -rf "$D"
