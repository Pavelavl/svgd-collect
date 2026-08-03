#!/bin/sh
# Run svgd-collect briefly against real /proc, verify a drop-in RRD is produced + fetchable.
set -e
D=$(mktemp -d)
cat > "$D/collect.json" <<EOF
{ "interval": 2, "datadir": "$D/rrd", "hostname": "testhost", "readers": ["cpu"] }
EOF
./bin/svgd-collect "$D/collect.json" &
PID=$!
sleep 7
kill -TERM $PID 2>/dev/null || true
wait $PID 2>/dev/null || true
F="$D/rrd/testhost/cpu/percent-active.rrd"
if [ ! -f "$F" ]; then echo "FAIL: drop-in RRD not created at $F"; rm -rf "$D"; exit 1; fi
rrdtool fetch "$F" AVERAGE --start -120 --end now >/dev/null 2>&1 || { echo "FAIL: rrdtool fetch failed"; rm -rf "$D"; exit 1; }
echo "PASS: drop-in RRD created and fetchable at $F"
rm -rf "$D"
