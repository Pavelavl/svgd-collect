#!/bin/sh
# Integration test for the Prometheus /metrics endpoint.
# Starts svgd-collect with metrics_addr configured, scrapes /metrics, and
# validates the exposition: required metric families appear with correct types
# (counter vs gauge). Runs `promtool check metrics` if installed.
#
# NOTE: all HTTP probes are gathered while the collector is alive; substring
# assertions run after shutdown.

D=$(mktemp -d)
PORT=19103
# Bind a high port on loopback so this never clashes with a real scrape target.
cat > "$D/collect.json" <<EOF
{ "interval": 1, "datadir": "$D/rrd", "hostname": "mettest",
  "metrics_addr": "127.0.0.1:$PORT",
  "readers": ["cpu","load","uptime","memory","swap","interface","disk","tcpconns"] }
EOF

./bin/svgd-collect "$D/collect.json" 2>"$D/stderr" &
PID=$!
# Wait for the listener to come up (up to ~5s), then give one collection cycle.
i=0
while [ $i -lt 50 ]; do
    if curl -fsS "http://127.0.0.1:$PORT/health" >/dev/null 2>&1; then break; fi
    i=$((i + 1)); sleep 0.1
done
sleep 1.5    # ensure at least one publish() has happened

ok=0
fail=0
fail_msg=""

# --- capture body, headers, and a 404 probe, all while the process is alive ---
BODY=$(curl -fsS "http://127.0.0.1:$PORT/metrics")
printf '%s\n' "$BODY" > "$D/metrics.txt"

CT=$(curl -fsS -i "http://127.0.0.1:$PORT/metrics" | tr -d '\r' | grep -i '^Content-Type:' || true)

# /nope: -f makes curl exit non-zero on 4xx; capture the code via -w anyway.
NOPE=$(curl -sS -o /dev/null -w '%{http_code}' "http://127.0.0.1:$PORT/nope" 2>/dev/null || echo "000")

# Shut down the collector now that all probes are captured.
kill -TERM $PID 2>/dev/null || true
wait $PID 2>/dev/null || true

need() {   # <substring that MUST appear in BODY>
    if printf '%s\n' "$BODY" | grep -Fq "$1"; then
        ok=$((ok + 1))
    else
        echo "FAIL: /metrics missing: $1"
        fail=$((fail + 1))
    fi
}

# --- required families + correct TYPE (counter/gauge) ---
need '# TYPE svgd_cpu_percent gauge'
need '# TYPE svgd_memory_percent gauge'
need '# TYPE svgd_swap_bytes gauge'
need '# TYPE svgd_load gauge'
need '# TYPE svgd_uptime_seconds gauge'
need '# TYPE svgd_tcp_connections gauge'
need '# TYPE svgd_network_bytes_total counter'
need '# TYPE svgd_network_packets_total counter'
need '# TYPE svgd_network_errors_total counter'
need '# TYPE svgd_disk_ops_total counter'
need '# TYPE svgd_disk_bytes_total counter'

# --- required label dimensions ---
need 'svgd_load{interval="1m"}'
need 'svgd_load{interval="5m"}'
need 'svgd_load{interval="15m"}'
need 'svgd_memory_percent{type="used"}'
need 'svgd_memory_percent{type="cached"}'
need 'svgd_swap_bytes{state="used"}'
need 'svgd_swap_bytes{state="free"}'
need 'svgd_network_bytes_total{device="'
need 'direction="receive"'
need 'direction="transmit"'

# --- Content-Type must be Prometheus text ---
case "$CT" in
    *text/plain*) ok=$((ok + 1)) ;;
    *) echo "FAIL: unexpected Content-Type: $CT"; fail=$((fail + 1)) ;;
esac

# --- unknown path must be 404 ---
if [ "$NOPE" = "404" ]; then
    ok=$((ok + 1))
else
    echo "FAIL: /nope -> $NOPE (want 404)"
    fail=$((fail + 1))
fi

# --- promtool check metrics (best-effort) ---
if command -v promtool >/dev/null 2>&1; then
    if promtool check metrics "$D/metrics.txt" >/dev/null 2>&1; then
        ok=$((ok + 1))
        echo "promtool check metrics: OK"
    else
        echo "FAIL: promtool check metrics reported errors:"
        promtool check metrics "$D/metrics.txt" || true
        fail=$((fail + 1))
    fi
else
    echo "note: promtool not installed; skipped promtool check metrics"
fi

if [ "$fail" -ne 0 ]; then
    echo "FAIL: $fail assertion(s) failed, $ok passed"
    echo "--- stderr ---"; cat "$D/stderr"
    echo "--- metrics (first 40 lines) ---"; head -40 "$D/metrics.txt"
    rm -rf "$D"
    exit 1
fi
echo "PASS: $ok /metrics assertion(s) (port 127.0.0.1:$PORT)"
rm -rf "$D"
