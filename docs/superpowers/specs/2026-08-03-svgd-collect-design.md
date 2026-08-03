# svgd-collect — дизайн

- **Дата:** 2026-08-03
- **Статус:** утверждён (brainstorm), ожидает плана реализации
- **Подход:** A — «фокус-коллектор → RRD, drop-in совместимый с collectd»

## Контекст и мотивация

svgd сегодня на 100% зависит от **collectd** как источника данных: каждый шаблон метрики в `config.json` кодирует файловую раскладку collectd (`cpu-total/percent-active.rrd`, `processes-%s/ps_rss.rrd` …), а `rrd.base_path` указывает на каталог collectd. collectd при этом **фактически в стазисе** — последний стабильный релиз 5.12.0 (сентябрь 2021, 4+ года назад), 6.0 застрял в RC, разработка «somewhat stuck» ([issue #4186](https://github.com/collectd/collectd/issues/4186)). Зависеть от протухающего внешнего компонента — стратегический риск для проекта, претендующего на зрелость.

`svgd-collect` — собственный лёгкий C-коллектор системных метрик, который **заменяет collectd** в стеке svgd, делая его самодостаточным («один набор бинарников = полный мониторинг»). Пишет RRD в **точно такой же раскладке**, что и collectd → `config.json` svgd остаётся без изменений (drop-in). Это убирает жёсткую зависимость, усиливает бренд «экстремальная эффективность» и даёт story для edge/IoT/embedded.

Подробное исследование — `RESEARCH-strategy.md` §4.

## Цели / не-цели

**Цели (v1):**
- Покрыть 11 семейств метрик, которые svgd уже использует (см. Readers).
- Drop-in: RRD-вывод, неотличимый от collectd (раскладка, DS-имена, RRA) — svgd работает без правок.
- Отдельный git-субмодуль, развязанный от svgd (связь только через RRD-файлы).
- Linux, `/proc`+`/sys`, один бинарь, минимум зависимостей (librrd).

**Не-цели (осознанно вне v1):**
- Кросс-ОС (BSD/macOS/Windows) — Linux-only.
- Плагин-система / расширение сторонними ридерами (YAGNI; svgd нужно ровно эти метрики).
- Своё TSDB-хранилище без RRD — это Phase 3 (отдельный проект).
- PostgreSQL reader (нужен libpq) — опционально позже.
- Абсолютные исторические окна (`from`/`to` в прошлом, не к now) — collector пишет «последние N секунд к сейчас», как collectd.

## Архитектура

### Роль и граница модуля
- Отдельный репозиторий `Pavelavl/svgd-collect`, подключается в репо svgd как **git-субмодуль** в каталог `svgd-collect/` (рядом с `lsrp/`).
- Один C-бинарь `svgd-collect`. Зависимости: **librrd** (как у svgd). **Не зависит** от svgd, lsrp, collectd.
- Связь с svgd — **только через RRD-файлы** в общем каталоге (`datadir` = тот же, что `rrd.base_path` у svgd). svgd читает их существующим конвейером; ноль API/протоколов. Коллектор юзабелен и без svgd (любым RRD-инструментом).

### Runtime
- Один процесс, один поток, interval-loop (default 5s, configurable).
- На каждом тике: прогон включённых readers → каждый возвращает массив `metric_t` → writer создаёт (lazy) и обновляет RRD.
- Writer: **direct librrd** (`rrd_create_r`/`rrd_update_r`) по умолчанию; опционально **rrdcached** (`rrdc_create`/`rrdc_update`) для нагрузки — svgd уже умеет `rrdcached_addr`.
- v1: один поток (на 5s interval чтение `/proc` + librrd достаточно). Вынос writer в отдельный поток с очередью — **hook на будущее**, не реализуем сейчас (YAGNI).
- Сигналы: `SIGTERM`/`SIGINT` → корректный shutdown (flush через rrdcached если есть); `SIGPIPE` → игнор (`SIG_IGN`); `SIGHUP` → (опц.) reload конфига — v1 можно пропустить.

## Модель данных

Компактный собственный тип вместо таскания `value_list_t`/`types.db` из collectd:

```c
typedef struct {
    const char *plugin;             /* "cpu", "memory", "interface", ...      */
    const char *plugin_instance;    /* "total", "eth0", "postgres", NULL если нет */
    const char *type;               /* "percent", "if_octets", "ps_rss", ...  */
    const char *type_instance;      /* "active", "rx", NULL ...               */
    int         ds_count;           /* кол-во значений = кол-во DS в RRD       */
    double      values[MAX_DS];     /* сами значения                            */
} metric_t;
```

`MAX_DS` = 4 (покрывает все нужные типы; чаще 1–2).

**Ключ drop-in — словарь типов.** DS-имена и типы (GAUGE/DERIVE/COUNTER) обязаны совпадать с collectd, иначе svgd получит другие `series_names`. Бандлим явный мини-словарь (~15 типов, заимствованных из `types.db` collectd):

| type | DS-имена | DST |
|---|---|---|
| `percent` | value | GAUGE |
| `if_octets` | rx, tx | DERIVE |
| `if_packets` | rx, tx | DERIVE |
| `if_errors` | rx, tx | DERIVE |
| `disk_ops` | reads, writes | DERIVE |
| `disk_octets` | read, write | DERIVE |
| `disk_time` | read, write | DERIVE |
| `ps_rss` | value | GAUGE |
| `ps_cputime` | value | DERIVE |
| `ps_count` | processes | GAUGE |
| `df_complex` | value | GAUGE |
| `load` | shortterm, midterm, longterm | GAUGE |
| `uptime` | value | GAUGE |
| `swap` | used, free | GAUGE |
| `temperature` | value | GAUGE |
| `tcp_connections` | value | GAUGE |

Это единственное заимствование из collectd — явно, минималистично, без парсера `types.db`.

## RRD-вывод (drop-in)

- **Путь:** `<datadir>/<host>/<plugin>[-<plugin_instance>]/<type>[-<type_instance>].rrd` — идентично collectd (`FORMAT_VL`). Примеры: `localhost/cpu-total/percent-active.rrd`, `localhost/processes-postgres/ps_rss.rrd`.
- **Path-билдер изолирован в одной функции** (`metric_to_path`) — смена раскладки на собственный формат позже = правка в одном месте (не закрываем дверь, но без upfront-механизмов).
- **Создание (lazy, при первом значении):** DS из словаря типов (имена + DST + heartbeat = `step*2` по умолчанию, как collectd). **RRA-конфиг = конфиг svgd `.infra/collectd`**:
  - `step` = 5
  - `xff` = 0.1
  - генерация RRA повторяет логику collectd `RRATimespan`: для каждого timespan из `{3600, 86400, 604800}` один RRA `AVERAGE` с `pdp_per_row = ceil(timespan / (step * rrarows))`, `rows = timespan / (step * pdp_per_row)`, `rrarows = 2400`. Дополнительно RRA `MIN`/`MAX` НЕ создаём (collectd по умолчанию тоже только AVERAGE, если не включено явно).
  - Цель — чтобы svgd'овский `select_optimal_step` видел **те же RRA**, что от collectd → идентичное поведение.
- **Update:** `rrd_update_r` (или `rrdc_update` с rrdcached). На каждый тик — одно `rrd_update` на файл с N значениями.
- **Orphan-файлы** (исчезнувшие инстансы — отмонтированный диск, убитый процесс): не удаляем (как collectd), файл остаётся.

## Readers (11 семейств, Linux)

| Семейство | Endpoint(s) svgd | Источник в ядре | type |
|---|---|---|---|
| CPU | `cpu`, `cpu/process` | `/proc/stat` (total + per-core) | percent |
| Memory | `ram`, `ram/cached`, `ram/buffered` | `/proc/meminfo` | percent |
| Swap | `swap/bytes`, `swap/percent` | `/proc/meminfo`, `/proc/swaps` | swap |
| Load | `system/load` | `/proc/loadavg` | load |
| Uptime | `system/uptime` | `/proc/uptime` (или `sysinfo(2)`) | uptime |
| Disk | `disk`, `disk/throughput`, `disk/io_time` | `/proc/diskstats` | disk_ops / disk_octets / disk_time |
| Interface | `network`, `network/packets`, `network/errors` | `/proc/net/dev` | if_octets / if_packets / if_errors |
| Filesystem | `filesystem`, `filesystem/free` | `statvfs(2)` по точкам монтирования | df_complex |
| Processes | `ram/process`, `cpu/process`, `process/count` | `/proc/[pid]/{stat,statm}` агрегация по имени | ps_rss / ps_cputime / ps_count |
| TCP conns | `tcp/connections`, `tcp/time_wait` | `/proc/net/tcp` (и `tcp6`) | tcp_connections |
| Thermal | `thermal` | `/sys/class/thermal/thermal_zone*/temp` | temperature |

Детали парсинга и нормализации (фильтр реальных блочных устройств из `/proc/diskstats`, агрегация процессов по `comm`, маппинг TCP-состояний) — в плане реализации.

## Конфиг `collect.json`

```json
{
  "interval": 5,
  "datadir": "/var/lib/svgd-collect/rrd",
  "hostname": "localhost",
  "rrdcached_addr": "",
  "readers": ["cpu","memory","swap","load","uptime","disk","interface","df","processes","tcpconns","thermal"],
  "options": {
    "disk":      { "include": ["sd*", "nvme*", "vd*"] },
    "interface": { "include": ["eth*", "en*", "wl*", "vmbr*"], "exclude": ["lo"] },
    "processes": { "include": ["postgres", "nginx", "systemd"] },
    "df":        { "exclude_fstypes": ["tmpfs","devtmpfs","proc","sysfs"] }
  }
}
```

Парсинг — собственный мини-JSON-парсер **без новых зависимостей** (конфиг скромный; если вложенность `options.*.include[]` окажется неудобной для ручного парсера — упрощаем/уплощаем схему конфига).

## Обработка ошибок

- Reader-ошибка (парсинг, отсутствие файла) → `fprintf(stderr, ...)` + skip этого reader на тике; демон не падает.
- Отсутствие `/proc/*` (другое ядро/ОС) → skip с warning при старте.
- RRD write fail → лог + продолжаем (next tick попробует снова).
- OOM → деградация (skip), не краш.
- Фатальные (bind/сокет/нет прав на `datadir`) → exit с понятным сообщением.
- Рекомендация: запуск под отдельным пользователем, права на `datadir`.

## Тестирование

- **Unit (чистый C):** парсеры `/proc` на фикстур-файлах (`tests/fixtures/proc/stat` и т.д.) → ожидаемые значения; генерация RRD-пути и DS-строк; логика RRA `pdp_per_row`.
- **Integration:** прогон `svgd-collect` против смонтированного fixture-`/proc` (через env/опцию путей) → проверить, что RRD-файлы созданы в drop-in раскладке **и читаются svgd'ом** (тот же конвейер, `select_optimal_step`, корректные `series_names`). Это главный smoke-тест drop-in совместимости.
- **Формат тестов:** Go (консистентно с svgd, гоняет бинарь как сабпровер) — предпочтительно; детали в плане.

## Структура репозитория и сборка

```
svgd-collect/
├── src/
│   ├── main.c            # entry, signals, loop
│   ├── collect.c         # interval loop, читает readers[], гонит в writer
│   ├── config.c          # парсинг collect.json
│   ├── types.c           # словарь типов (DS-имена, DST)
│   ├── writer_rrd.c      # metric_to_path + rrd_create/update (path-билдер изолирован тут)
│   └── readers/
│       ├── cpu.c memory.c swap.c load.c uptime.c
│       ├── disk.c interface.c df.c processes.c tcpconns.c thermal.c
├── include/
├── makefile              # make build -> bin/svgd-collect
├── tests/                # fixtures + Go/C тесты
├── README.md
├── LICENSE               # MIT
└── .gitignore
```

- `make build` → `bin/svgd-collect`. Зависимости: `librrd-dev gcc make`.
- **Интеграция в svgd:** запись в `.gitmodules` + каталог `svgd-collect/`. svgd-makefile коллектор **не линкует** (отдельный бинарь, отдельный жизненный цикл). В README svgd — раздел про опциональную замену collectd на svgd-collect.

## Будущее (явно вне v1)

- **Phase 2:** плагинабельный reader в svgd (`metric_source_t`: rrd | proc | prometheus) — svgd становится универсальным визуализатором.
- **Phase 3:** собственный формат хранения (отказ от librrd) или окончательное решение «RRD навсегда».
- Свои не-drop-in раскладки через `metric_to_path` (дешёвая правка).
