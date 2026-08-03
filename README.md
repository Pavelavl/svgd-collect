# svgd-collect

A lightweight system metrics collector that writes RRD files (drop-in compatible with collectd).

![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)

A small, self-contained C collector (~2k lines) in the spirit of svgd's "extreme efficiency" brand.

**RRD writing:** by default `writer_write` writes RRDs directly via librrd (`rrd_create_r`/`rrd_update_r`). Set `rrdcached_addr` in `collect.json` to route hot-path updates through an `rrdcached` daemon instead — RRD creation stays direct, and if the daemon is unreachable the writer falls back to direct writes so no samples are lost.

## Readers

A reader is a named function that samples one source under `/proc` and emits `metric_t` values via the writer. `collect_run` dispatches to readers through a name→reader registry in `src/collect.c`. The `config.readers[]` array selects which readers run (case-sensitive names); an empty/absent array enables **all** readers. Unknown names are logged to stderr and skipped (the run continues).

The 11 readers currently implemented:

| Name | `/proc` source | Emits |
|------|----------------|-------|
| `cpu` | `/proc/stat` | `cpu-total/percent-active` (busy% delta between samples) |
| `load` | `/proc/loadavg` | `load/load` (1/5/15-min) |
| `uptime` | `/proc/uptime` | `uptime/uptime` (seconds) |
| `memory` | `/proc/meminfo` | `memory/percent-{used,cached,buffered}` |
| `swap` | `/proc/meminfo` | `swap/swap-{used,free}` (bytes) |
| `interface` | `/proc/net/dev` | `interface-<if>/if_{octets,packets,errors}` |
| `disk` | `/proc/diskstats` | `disk-<dev>/disk_{ops,octets,time}` |
| `df` | `/proc/mounts` | `df-<mount>/df_complex-{used,free}` |
| `processes` | `/proc/[pid]` | `processes-<comm>/{ps_rss,ps_cputime,ps_count}` |
| `thermal` | `/sys/class/thermal/thermal_zoneN` | `thermal-thermal_zoneN/temperature-<type>` (°C) |
| `tcpconns` | `/proc/net/tcp{,6}` | `tcpconns/tcp_connections-<STATE>` (count) |

## Prometheus `/metrics` endpoint

svgd-collect can additionally expose a Prometheus scrape target. Set
`metrics_addr` in `collect.json` to a `host:port` string and a minimal HTTP
listener (plain C sockets, no extra dependencies) will serve `GET /metrics` in
[text exposition format](https://prometheus.io/docs/instrumenting/exposition_formats/)
from a dedicated thread. If `metrics_addr` is absent or empty, **no listener is
started** — the feature is strictly opt-in.

```json
{ "metrics_addr": "0.0.0.0:9103", "interval": 10, "readers": [] }
```

```yaml
# prometheus.yml
scrape_configs:
  - job_name: svgd-collect
    static_configs: [ { targets: ['host:9103'] } ]
```

Each collection cycle tees every emitted metric into a snapshot that the HTTP
thread renders on demand, so a scrape never blocks the collection loop. Metric
**type** (gauge vs counter) is derived from the collectd DS definitions
(`src/types.c`): `GAUGE` → `gauge`, `DERIVE`/`COUNTER` → `counter`. Sample
names (see `src/prom.c` for the full mapping):

| Family | Type | Labels |
|--------|------|--------|
| `svgd_cpu_percent` | gauge | `mode` |
| `svgd_memory_percent` | gauge | `type` |
| `svgd_swap_bytes` | gauge | `state` |
| `svgd_load` | gauge | `interval` |
| `svgd_uptime_seconds` | gauge | — |
| `svgd_network_bytes_total` / `svgd_network_packets_total` / `svgd_network_errors_total` | counter | `device`, `direction` |
| `svgd_disk_ops_total` / `svgd_disk_bytes_total` / `svgd_disk_io_time_ms_total` | counter | `device`, `operation` |
| `svgd_filesystem_bytes` | gauge | `fs`, `state` |
| `svgd_process_rss_bytes` | gauge | `process` |
| `svgd_process_cpu_jiffies_total` | counter | `process`, `mode` |
| `svgd_process_count` | gauge | `process`, `kind` |
| `svgd_thermal_celsius` | gauge | `thermal_zone`, `type` |
| `svgd_tcp_connections` | gauge | `state` |

Scrape timestamps are omitted (Prometheus stamps samples on ingest). A `GET /`
or `GET /health` returns `200 svgd-collect`; any other path returns `404`.
The listener is single-threaded (one connection at a time), which is ample for
typical 15–60s scrape cadences.
