# svgd-collect

A lightweight system metrics collector that writes RRD files (drop-in compatible with collectd).

![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)

> WIP — see docs/superpowers/specs/2026-08-03-svgd-collect-design.md

**Known limitation (Phase 1):** `writer_write` writes through the direct librrd path (`rrd_create_r`/`rrd_update_r`); the `rrdcached` address is accepted by `writer_init` but routing updates through `rrdcached` is deferred to a later phase.

## Readers

A reader is a named function that samples one source under `/proc` and emits `metric_t` values via the writer. `collect_run` dispatches to readers through a name→reader registry in `src/collect.c`. The `config.readers[]` array selects which readers run (case-sensitive names); an empty/absent array enables **all** readers. Unknown names are logged to stderr and skipped (the run continues).

The 9 readers currently implemented:

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

Deferred (planned, not yet implemented): `thermal` (`/sys/class/thermal`) and `tcpconns` (`/proc/net/tcp`).
