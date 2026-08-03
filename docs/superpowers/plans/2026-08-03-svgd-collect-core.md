# svgd-collect Core Implementation Plan (Phase 1)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build `svgd-collect` — a standalone C collector that reads CPU usage from `/proc/stat` and writes it to RRD in a collectd-compatible layout, so svgd reads it unchanged (drop-in).

**Architecture:** Single-threaded interval-loop daemon. Readers emit `metric_t` structs via a callback; a writer creates (lazy) and updates RRD files via librrd in collectd's `<host>/<plugin>[-<inst>]/<type>[-<inst>].rrd` layout. Separate git submodule; talks to svgd only through RRD files. Phase 1 delivers repo scaffold + core (types, path, RRA, writer, config, loop) + the `cpu` reader + integration test + submodule wiring. Readers 2–11 are a follow-up plan reusing the reader interface defined here.

**Tech Stack:** C (C11), librrd (`rrd_create_r`/`rrd_update_r`, optional `rrdc_*`), POSIX (`/proc`, signals), gcc + make. Tests: plain C `assert()` test binaries + a Go integration test that drives the binary (consistent with svgd's test suite).

**Spec:** `docs/superpowers/specs/2026-08-03-svgd-collect-design.md`

## Global Constraints

- License: MIT (file `LICENSE`, copyright line `Copyright (c) 2026 Pavelavl`).
- Linux-only; readers target `/proc` and `/sys`.
- Single dependency beyond libc: **librrd** (`librrd-dev`). No Duktape, no JSON library.
- Drop-in: RRD layout + DS names + DST + RRA must match collectd so svgd's `config.json` and `select_optimal_step` behave identically.
- All code Doxygen-style `@file`/`@brief` headers (match svgd C style).
- `RRD step = 5`, `xff = 0.1`, `RRARows = 2400`, `RRATimespan = {3600, 86400, 604800}` (from svgd `.infra/collectd`).
- Commits: NO `Co-Authored-By` trailer (user rule). Conventional-commit messages.

## File Structure (Phase 1)

```
svgd-collect/                       # NEW separate repo, added as submodule
├── include/
│   ├── types.h        # type_def_t lookup (DS names + DST) — Task 2
│   ├── metric.h       # metric_t + metric_emit_fn callback — Task 3
│   ├── path.h         # metric_to_path() — Task 4
│   ├── rra.h          # rra_args() builds DS/RRA argv for rrd_create — Task 5
│   ├── writer_rrd.h   # writer_t init/write (create-if-absent + update) — Task 6
│   ├── config.h       # collect_config_t + config_load() — Task 7
│   └── reader.h       # reader_t interface + extern cpu_reader — Task 8
├── src/
│   ├── main.c         # entry, signals, arg parse → loop — Task 9
│   ├── collect.c      # interval loop: config→readers→writer — Task 9
│   ├── types.c        # static type table — Task 2
│   ├── path.c         # metric_to_path — Task 4
│   ├── rra.c          # RRA pdp_per_row + argv builder — Task 5
│   ├── writer_rrd.c   # librrd create/update — Task 6
│   ├── config.c       # collect.json mini-parser — Task 7
│   └── readers/cpu.c  # /proc/stat → percent metric(s) — Task 8
├── tests/
│   ├── minitest.h     # tiny TEST()/ASSERT macros — Task 1
│   ├── test_types.c   # Task 2
│   ├── test_path.c    # Task 4
│   ├── test_rra.c     # Task 5
│   ├── test_writer.c  # Task 6
│   ├── test_config.c  # Task 7
│   ├── test_cpu.c     # Task 8
│   └── fixtures/proc/stat   # fixture /proc — Task 8
├── makefile           # build + test targets — Task 1
├── README.md
├── LICENSE
└── .gitignore
```

Interfaces flow: `reader.read()` → emits `metric_t` → `types_lookup()` resolves DS → `metric_to_path()` → `writer_write()` → `rra_args()` + `rrd_create_r`/`rrd_update_r`.

---

### Task 1: Repo scaffold + build + test harness

**Files:**
- Create: `svgd-collect/makefile`, `svgd-collect/.gitignore`, `svgd-collect/LICENSE`, `svgd-collect/README.md`, `svgd-collect/tests/minitest.h`

**Interfaces:**
- Produces: `make build` → `bin/svgd-collect`; `make test` → compiles+runs `tests/test_*.c`. `minitest.h` macros used by all later test files.

- [ ] **Step 1: Create `LICENSE` (MIT)** — full MIT text, copyright `Copyright (c) 2026 Pavelavl`.

- [ ] **Step 2: Create `.gitignore`**
```
bin/
*.o
*.rrd
```

- [ ] **Step 3: Create `tests/minitest.h`**
```c
#ifndef SVGD_COLLECT_MINITEST_H
#define SVGD_COLLECT_MINITEST_H
#include <stdio.h>
static int __mt_failures = 0;
#define TEST(name) static void name(void)
#define RUN(name) do { printf("  %s ... ", #name); name(); printf("ok\n"); } while (0)
#define ASSERT(cond) do { if (!(cond)) { printf("FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); __mt_failures++; return; } } while (0)
#define ASSERT_STR(a, b) ASSERT(strcmp((a), (b)) == 0)
#define TEST_MAIN() int main(void) { __mt_failures = 0;
#define TEST_RETURN() printf("%d failure(s)\n", __mt_failures); return __mt_failures ? 1 : 0; }
#endif
```

- [ ] **Step 4: Create `makefile`** (build + test; mirrors svgd's makefile style — tabs for recipes)
```makefile
CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -g -Iinclude -pthread
LDLIBS  = -lrrd
BIN_DIR = bin
OBJS    = src/main.o src/collect.o src/types.o src/path.o src/rra.o src/writer_rrd.o src/config.o src/readers/cpu.o

.PHONY: all build test clean

all: build
build: $(BIN_DIR)/svgd-collect
$(BIN_DIR)/svgd-collect: $(OBJS) | $(BIN_DIR)
	$(CC) -o $@ $(OBJS) $(CFLAGS) $(LDLIBS)
$(BIN_DIR):
	mkdir -p $(BIN_DIR)
src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@
TEST_OBJS = src/types.o src/path.o src/rra.o src/writer_rrd.o src/config.o src/readers/cpu.o
test: $(TEST_OBJS)
	@for t in tests/test_*.c; do \
	  base=$$(basename $$t .c); \
	  echo "== $$base =="; \
	  $(CC) $(CFLAGS) $$t $(TEST_OBJS) -o $(BIN_DIR)/$$base $(LDLIBS) || exit 1; \
	  ./$(BIN_DIR)/$$base || exit 1; \
	done
clean:
	rm -rf $(BIN_DIR) $(OBJS)
```
(The `test` target is a simple per-file harness; the implementer may refine it — it must compile each `tests/test_*.c` against the matching `src/*.c` and run it, exiting non-zero on failure.)

- [ ] **Step 5: Create `README.md`** skeleton: title, one-liner, "WIP — see spec", MIT badge.

- [ ] **Step 6: Init git repo and verify build (empty main)**
Create a placeholder `src/main.c` (`int main(void){return 0;}`) so `make build` succeeds, then:
```bash
make build && ls bin/svgd-collect
```
Expected: binary exists.

- [ ] **Step 7: Commit**
```bash
git add -A && git commit -m "chore: scaffold svgd-collect repo, build + test harness"
```

---

### Task 2: Types dictionary

**Files:** Create `include/types.h`, `src/types.c`, `tests/test_types.c`

**Interfaces:**
- Produces: `dst_t`, `ds_def_t`, `type_def_t`, `const type_def_t *types_lookup(const char *type)` (returns `NULL` if unknown). Each `type_def_t` carries the DS name list + DST list for one collectd type.

- [ ] **Step 1: Write `tests/test_types.c`**
```c
#include "minitest.h"
#include "types.h"
TEST(lookup_percent) {
    const type_def_t *t = types_lookup("percent");
    ASSERT(t != NULL);
    ASSERT_STR(t->type, "percent");
    ASSERT(t->ds_count == 1);
    ASSERT_STR(t->ds[0].name, "value");
    ASSERT(t->ds[0].dst == DST_GAUGE);
}
TEST(lookup_if_octets) {
    const type_def_t *t = types_lookup("if_octets");
    ASSERT(t != NULL && t->ds_count == 2);
    ASSERT_STR(t->ds[0].name, "rx"); ASSERT(t->ds[0].dst == DST_DERIVE);
    ASSERT_STR(t->ds[1].name, "tx"); ASSERT(t->ds[1].dst == DST_DERIVE);
}
TEST(lookup_unknown) { ASSERT(types_lookup("nope") == NULL); }
TEST_MAIN() RUN(lookup_percent); RUN(lookup_if_octets); RUN(lookup_unknown); TEST_RETURN()
```

- [ ] **Step 2: Run test → verify it fails** (`make test` or compile directly). Expected: compile error (`types.h` missing).

- [ ] **Step 3: Write `include/types.h`**
```c
/** @file types.h @brief collectd-compatible type → DS definitions (drop-in) */
#ifndef SVGD_COLLECT_TYPES_H
#define SVGD_COLLECT_TYPES_H
typedef enum { DST_GAUGE, DST_DERIVE, DST_COUNTER } dst_t;
typedef struct { const char *name; dst_t dst; } ds_def_t;
typedef struct { const char *type; const ds_def_t *ds; int ds_count; } type_def_t;
const type_def_t *types_lookup(const char *type);
#endif
```

- [ ] **Step 4: Write `src/types.c`** — the full table from spec §"Модель данных" (percent, if_octets, if_packets, if_errors, disk_ops, disk_octets, disk_time, ps_rss, ps_cputime, ps_count, df_complex, load, uptime, swap, temperature, tcp_connections). Example entries:
```c
#include "types.h"
static const ds_def_t ds_percent[] = {{"value", DST_GAUGE}};
static const ds_def_t ds_if_octets[] = {{"rx", DST_DERIVE}, {"tx", DST_DERIVE}};
static const ds_def_t ds_load[] = {{"shortterm", DST_GAUGE}, {"midterm", DST_GAUGE}, {"longterm", DST_GAUGE}};
/* ...all 16 types... */
static const type_def_t TABLE[] = {
    {"percent", ds_percent, 1}, {"if_octets", ds_if_octets, 2}, {"load", ds_load, 3},
    /* ...rest... */
};
const type_def_t *types_lookup(const char *type) {
    for (size_t i = 0; i < sizeof(TABLE)/sizeof(TABLE[0]); i++)
        if (strcmp(TABLE[i].type, type) == 0) return &TABLE[i];
    return NULL;
}
```
(Implementer fills all 16 entries from the spec table.)

- [ ] **Step 5: Run test → verify pass.**

- [ ] **Step 6: Commit** — `feat(types): collectd-compatible type dictionary`

---

### Task 3: metric_t model

**Files:** Create `include/metric.h`

**Interfaces:**
- Produces: `metric_t { const char *plugin, *plugin_instance, *type, *type_instance; int ds_count; double values[METRIC_MAX_DS]; }`; `#define METRIC_MAX_DS 4`; `typedef int (*metric_emit_fn)(const metric_t *, void *);`. Pure header (no .c, no test — it's a data type consumed by Tasks 4–9).

- [ ] **Step 1: Write `include/metric.h`** (full struct + emit callback typedef + METRIC_MAX_DS).

- [ ] **Step 2: Commit** — `feat(metric): value model + emit callback`

---

### Task 4: Path builder (isolated layout)

**Files:** Create `include/path.h`, `src/path.c`, `tests/test_path.c`

**Interfaces:**
- Consumes: `metric_t` (Task 3).
- Produces: `int metric_to_path(char *out, size_t n, const char *base, const char *host, const metric_t *m)` → returns 0 on success, builds `<base>/<host>/<plugin>[-<plugin_instance>]/<type>[-<type_instance>].rrd`. `NULL` instances skipped.

- [ ] **Step 1: Write `tests/test_path.c`**
```c
#include "minitest.h"
#include "metric.h"
#include "path.h"
TEST(simple) {
    metric_t m = {"cpu", NULL, "percent", "active", 1, {0}};
    char out[512];
    ASSERT(metric_to_path(out, sizeof out, "/var/rrd", "localhost", &m) == 0);
    ASSERT_STR(out, "/var/rrd/localhost/cpu/percent-active.rrd");
}
TEST(with_instances) {
    metric_t m = {"interface", "eth0", "if_octets", NULL, 2, {0}};
    char out[512];
    ASSERT(metric_to_path(out, sizeof out, "/d", "h", &m) == 0);
    ASSERT_STR(out, "/d/h/interface-eth0/if_octets.rrd");
}
TEST_MAIN() RUN(simple); RUN(with_instances); TEST_RETURN()
```

- [ ] **Step 2: Run → verify fail.**

- [ ] **Step 3: Write `include/path.h`** (signature above) and `src/path.c`:
```c
#include "path.h"
#include "metric.h"
#include <stdio.h>
#include <string.h>
static void catseg(char *out, size_t n, size_t *off, const char *a, const char *b) {
    /* appends "/a" or "/a-b" (b optional), bounds-checked */
    *off += (size_t)snprintf(out + *off, n - *off, "/%s%s%s", a, b ? "-" : "", b ? b : "");
}
int metric_to_path(char *out, size_t n, const char *base, const char *host, const metric_t *m) {
    if (!out || !base || !host || !m) return -1;
    size_t off = 0;
    off += (size_t)snprintf(out, n, "%s/%s", base, host);
    catseg(out, n, &off, m->plugin, m->plugin_instance);
    char file[256];
    int fl = snprintf(file, sizeof file, "%s%s%s.rrd",
                      m->type, m->type_instance ? "-" : "", m->type_instance ? m->type_instance : "");
    if (fl < 0 || (size_t)fl >= sizeof file) return -1;
    off += (size_t)snprintf(out + off, n - off, "/%s", file);
    return (off >= n) ? -1 : 0;
}
```

- [ ] **Step 4: Run → verify pass.**

- [ ] **Step 5: Commit** — `feat(path): collectd-layout RRD path builder`

---

### Task 5: RRA argument builder

**Files:** Create `include/rra.h`, `src/rra.c`, `tests/test_rra.c`

**Interfaces:**
- Consumes: `type_def_t` (Task 2), `metric_t` (Task 3).
- Produces: `int rra_args(const type_def_t *td, char **argv, int argv_max)` → fills `argv` with `DS:...` entries (one per DS) + `RRA:AVERAGE:0.1:<pdp>:<rows>` entries (one per timespan). Returns count, or -1 if `argv_max` too small. `pdp_per_row = ceil(timespan / (STEP * RRAROWS))`, `rows = timespan / (STEP * pdp)`. Constants: `STEP=5`, `XFF="0.1"`, `RRAROWS=2400`, timespans `{3600,86400,604800}`.

- [ ] **Step 1: Write `tests/test_rra.c`** asserting: for `percent` (1 GAUGE DS) the argv contains `"DS:value:GAUGE:10:U:U"` and exactly 3 `RRA:AVERAGE:0.1:...` strings; verify the first timespan (3600): `pdp = ceil(3600/(5*2400)) = ceil(0.3) = 1`, `rows = 3600/(5*1) = 720` → `"RRA:AVERAGE:0.1:1:720"`. (Implementer adds assertions for 86400 and 604800: pdp=1/rows=1728; pdp=1/rows=12096 — recompute in the test to the formula.)

- [ ] **Step 2: Run → fail.**

- [ ] **Step 3: Write `include/rra.h` + `src/rra.c`** with `#define STEP 5`, the formula, `snprintf` into a static/heap buffer per entry (caller passes `char *argv[]`; `rra.c` fills from an internal static buffer OR caller-allocated — choose static internal buffer, document non-reentrancy, fine for single-threaded). DS line format: `DS:<name>:<GAUGE|DERIVE|COUNTER>:<heartbeat=STEP*2>:U:U`.

- [ ] **Step 4: Run → pass.**

- [ ] **Step 5: Commit** — `feat(rra): RRA/DS argument builder matching collectd`

---

### Task 6: RRD writer

**Files:** Create `include/writer_rrd.h`, `src/writer_rrd.c`, `tests/test_writer.c`

**Interfaces:**
- Consumes: `types_lookup` (Task 2), `metric_to_path` (Task 4), `rra_args` (Task 5), librrd (`rrd_create_r`, `rrd_update_r`).
- Produces: `writer_t { char datadir[4096]; char host[128]; char rrdcached[256]; }`; `int writer_init(writer_t*, const char *datadir, const char *host, const char *rrdcached)`; `int writer_write(writer_t*, const metric_t*)` — resolves type, builds path, `access(path,F_OK)`→ if missing call `rrd_create_r(path, STEP, 0, argc, argv)` (argv from `rra_args`); then `rrd_update_r(path, NULL, 1, {"N:v1:v2..."})`. Returns 0 ok, -1 on error (log to stderr).

- [ ] **Step 1: Write `tests/test_writer.c`** — create a `writer_t` with `datadir` = a temp dir (`mkdtemp`), write a `percent` metric, then `rrd_fetch_r` the created file (librrd) and assert `ds_cnt==1` and `ds_names[0]=="value"` and `num_points>=1`. This proves drop-in: the RRD is valid and readable exactly as svgd reads it.

- [ ] **Step 2: Run → fail.**

- [ ] **Step 3: Write `include/writer_rrd.h` + `src/writer_rrd.c`**:
```c
#include "writer_rrd.h"
#include "types.h", "path.h", "rra.h", "metric.h"
#include <rrd.h>, <stdio.h>, <string.h>, <unistd.h>, <sys/stat.h>
int writer_write(writer_t *w, const metric_t *m) {
    const type_def_t *td = types_lookup(m->type);
    if (!td) { fprintf(stderr,"collect: unknown type %s\n", m->type); return -1; }
    char path[4096];
    if (metric_to_path(path, sizeof path, w->datadir, w->host, m) != 0) return -1;
    if (access(path, F_OK) != 0) {
        char *argv[64]; char buf[4096]; /* rra_args fills internal buffer */
        int n = rra_args(td, argv, 64);
        if (rrd_create_r(path, STEP, 0, n, (const char **)argv) != 0) {
            fprintf(stderr,"collect: rrd_create %s: %s\n", path, rrd_get_error()); rrd_clear_error(); }
    }
    char vbuf[256]; int o=0;
    o += snprintf(vbuf+o,sizeof vbuf-o,"N");
    for (int i=0;i<m->ds_count;i++) o += snprintf(vbuf+o,sizeof vbuf-o,":%.6g", m->values[i]);
    const char *uargv[1] = { vbuf };
    int rc = rrd_update_r(path, NULL, 1, uargv);
    if (rc != 0) { fprintf(stderr,"collect: rrd_update %s: %s\n", path, rrd_get_error()); rrd_clear_error(); }
    return rc;
}
```
(Implementer adds mkdir -p of the file's parent dir before create.)

- [ ] **Step 4: Run → pass** (RRD created + fetchable).

- [ ] **Step 5: Commit** — `feat(writer): librrd RRD create/update, drop-in`

> **rrdcached:** Phase-1 writer uses the direct librrd path (`rrd_create_r`/`rrd_update_r`). The `rrdcached_addr` config field is parsed (Task 7) but **rrdcached routing is deferred to the readers follow-up plan** — document this in `README.md` as a known Phase-1 limitation. (Direct-write is fine for embedded/small hosts; rrdcached matters only at scale.)

---

### Task 7: Config parser

**Files:** Create `include/config.h`, `src/config.c`, `tests/test_config.c`, `tests/fixtures/collect.json`

**Interfaces:**
- Produces: `collect_config_t { int interval; char datadir[4096]; char host[128]; char rrdcached[256]; int readers_enabled[16]; }`; `int config_load(collect_config_t*, const char *path)` returns 0 ok. Defaults: interval 5, datadir `"/var/lib/svgd-collect/rrd"`, host `"localhost"`, rrdcached `""`, all readers enabled.

- [ ] **Step 1: Write `tests/fixtures/collect.json`** with interval 10, custom datadir, hostname "web1", readers `["cpu","memory"]`.
- [ ] **Step 2: Write `tests/test_config.c`** asserting parsed values + only cpu/memory enabled.
- [ ] **Step 3: Run → fail.**
- [ ] **Step 4: Write `include/config.h` + `src/config.c`** — hand-rolled mini-parser: read file, `strstr` for top-level scalar fields (`"interval"`, `"datadir"`, `"hostname"`, `"rrdcached_addr"`), and a readers[] array parser (find `"readers"`, extract quoted strings until `]`). No nested-options parsing in Phase 1 (options ignored; spec allows flattening). Document the limitation.
- [ ] **Step 5: Run → pass.**
- [ ] **Step 6: Commit** — `feat(config): collect.json mini-parser`

---

### Task 8: Reader interface + cpu reader

**Files:** Create `include/reader.h`, `src/readers/cpu.c`, `tests/test_cpu.c`, `tests/fixtures/proc/stat`

**Interfaces:**
- Consumes: `metric_t`/`metric_emit_fn` (Task 3), `types` (Task 2 — implicitly via writer).
- Produces: `typedef struct { const char *name; int (*read)(const char *proc_base, metric_emit_fn emit, void *ud); } reader_t;` and `extern const reader_t cpu_reader;`. The `proc_base` param (default `"/proc"`) makes readers testable against fixtures.

- [ ] **Step 1: Create `tests/fixtures/proc/stat`** — a realistic `/proc/stat` first line + per-core lines, e.g.:
```
cpu  12345 678 9012 999999 456 0 78 0 0 0
cpu0 1000 50 700 250000 30 0 10 0 0 0
cpu1 ...
```
- [ ] **Step 2: Write `tests/test_cpu.c`** — call `cpu_reader.read("tests/fixtures/proc", emit, ud)` twice with a fake clock (or a delta) and assert it emits a `percent-active` metric for the host with plugin `cpu`, plugin_instance `total` (or `cpu0`...), and a value in [0,100]. NOTE: CPU% requires two samples (delta of `cpu` jiffies / delta of total). The reader must either (a) keep prior sample internally and emit on the 2nd+ call, or (b) take a delta. Design: reader keeps a static prev-sample; emits GAUGE percent on each call after the first. Test calls twice, asserts 2nd emits a metric in range.
- [ ] **Step 3: Run → fail.**
- [ ] **Step 4: Write `include/reader.h`** + `src/readers/cpu.c`: parse `<proc_base>/stat`, the `cpu` aggregate line = 10 fields; sum of fields 1..7 = busy, total = sum of all 10; store prev_busy/prev_total in a static; percent = 100*(busy-prev_busy)/(total-prev_total). Emit `{plugin:"cpu", plugin_instance:NULL|"cpuN", type:"percent", type_instance:"active", ds_count:1, values:[pct]}`. (For Phase 1, emit just the aggregate `cpu` total; per-core is a trivial extension in Plan 2.)
- [ ] **Step 5: Run → pass.**
- [ ] **Step 6: Commit** — `feat(readers): cpu reader (/proc/stat) + reader interface`

---

### Task 9: Main loop + integration + submodule wiring

**Files:** Create `src/main.c`, `src/collect.c`; (in svgd repo) `.gitmodules` + submodule registration + svgd `readme.md` note.

**Interfaces:**
- Consumes: config (Task 7), writer (Task 6), cpu_reader (Task 8). Produces: `bin/svgd-collect` daemon: parse args (`./bin/svgd-collect [config.json]`), load config, `signal(SIGTERM/SIGINT)` → set running=0, loop every `interval` s calling each enabled reader's `read("/proc", emit, &writer)` where `emit` calls `writer_write`.

- [ ] **Step 1: Write `src/collect.c`** — `collect_run(collect_config_t*)`: build `writer_t` from config; build an array of enabled `reader_t*` (Phase 1: just `&cpu_reader`); `while(running){ for each reader: reader->read("/proc", emit_cb, &writer); sleep(interval); }`. The `emit_cb` casts ud to `writer_t*` and calls `writer_write`.
- [ ] **Step 2: Write `src/main.c`** — `signal` handlers, `config_load`, `collect_run`. Graceful shutdown on SIGTERM.
- [ ] **Step 3: Build** — `make build` clean.
- [ ] **Step 4: Integration test (manual then automated).** Create a throwaway `collect.json` pointing `datadir` at a temp dir, run `./bin/svgd-collect collect.json &` for ~12s (3 ticks at interval 5), kill it. Then verify svgd reads the produced RRD: point svgd's `rrd.base_path` at `<datadir>/<host>` and `curl localhost:8080/cpu` (through svgd-gate) returns SVG, OR directly `ls <datadir>/localhost/cpu/percent-active.rrd` exists and `rrdtool fetch` it returns rows. Automate as a Go test under `svgd/tests/` (mirrors svgd's e2e style) OR a shell test in svgd-collect's `tests/` — implementer's choice; the assertion is: **after running svgd-collect, an RRD exists at `<datadir>/<host>/cpu/percent-active.rrd` and is readable by librrd**.
- [ ] **Step 5: Submodule wiring in svgd repo.** From the svgd repo root:
```bash
# svgd-collect repo already pushed to Pavelavl/svgd-collect
git submodule add https://github.com/Pavelavl/svgd-collect.git svgd-collect
```
Add to svgd `readme.md` a short note under the `## collectd` section: "Alternatively, svgd-collect (submodule `svgd-collect/`) can replace collectd as a lightweight drop-in — same RRD layout, no config changes."
- [ ] **Step 6: Commit (svgd-collect repo)** — `feat(core): main loop + integration, drop-in verified`
- [ ] **Step 7: Commit (svgd repo)** — `chore: add svgd-collect submodule` (no co-author)

---

## Self-Review

**Spec coverage (Phase 1 scope):**
- Role/boundary (submodule, RRD-only coupling) → Task 9 Step 5.
- Runtime (1 thread, interval, direct librrd + rrdcached optional) → Task 9 (rrdcached wiring deferred — rrdcached_addr parsed in Task 7 but writer uses direct path in Task 6; **GAP**: rrdcached support not wired in writer). → Action: add a note in Task 6 that rrdcached path is deferred to Plan 2/reader-followup, OR add a Step. Kept as known Phase-1 limitation; document in README.
- Data model + types dict → Tasks 2,3. ✅
- RRD output (path, RRA, drop-in) → Tasks 4,5,6. ✅ (verified by fetch in Task 6 test + integration in Task 9).
- cpu reader → Task 8. ✅
- Config → Task 7. ✅
- Error handling → writer/readers log+skip (Tasks 6,8); signals (Task 9). ✅
- Testing → unit tests Tasks 2,4,5,6,7,8 + integration Task 9. ✅
- Repo/build → Task 1. ✅
- Readers 2–11 → **intentionally Plan 2** (follow-up), reusing `reader_t` (Task 8). Documented.
- postgresql → out of scope (spec non-goal). ✅

**Placeholder scan:** Task 1's `makefile` `test` recipe has a `$$cflags_fixup` placeholder — **fix**: simplify the test recipe to a deterministic per-file compile (no `cflags_fixup`). Action applied below.

**Type consistency:** `types_lookup` signature — Task 2 test uses `const char *` arg; Task 2 impl written as `char *type`; **fix** to `const char *type`. `reader_t.read` takes `(const char *proc_base, metric_emit_fn, void*)` consistently across Tasks 8/9. `writer_write(writer_t*, const metric_t*)` consistent. ✅

Apply fixes: (a) Task 1 makefile `test` recipe simplified; (b) Task 2 `types_lookup(const char *type)`; (c) Task 6 README documents rrdcached support deferred.

## Execution

Phase 2 (readers 2–11: memory, swap, load, uptime, disk, interface, df, processes, tcpconns, thermal) will be a follow-up plan, each reader a task reusing the `reader_t` interface from Task 8 with its own fixture + test, following the exact TDD pattern of Task 8.
