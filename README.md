# svgd-collect

A lightweight system metrics collector that writes RRD files (drop-in compatible with collectd).

![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)

> WIP — see docs/superpowers/specs/2026-08-03-svgd-collect-design.md

**Known limitation (Phase 1):** `writer_write` writes through the direct librrd path (`rrd_create_r`/`rrd_update_r`); the `rrdcached` address is accepted by `writer_init` but routing updates through `rrdcached` is deferred to a later phase.
