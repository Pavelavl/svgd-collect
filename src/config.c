/** @file config.c @brief collect.json mini-parser */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void extract_string(const char *json, const char *key, char *out, size_t n) {
    const char *p = strstr(json, key);
    if (!p) return;
    p = strchr(p + strlen(key), ':');
    if (!p) return;
    p = strchr(p + 1, '"');
    if (!p) return;
    const char *e = strchr(++p, '"');
    if (!e) return;
    size_t len = (size_t)(e - p);
    if (len >= n) len = n - 1;
    memcpy(out, p, len); out[len] = '\0';
}
static int extract_int(const char *json, const char *key, int def) {
    const char *p = strstr(json, key);
    if (!p) return def;
    p = strchr(p + strlen(key), ':');
    if (!p) return def;
    return atoi(p + 1);
}
int config_load(collect_config_t *c, const char *path) {
    c->interval = 5;
    snprintf(c->datadir, sizeof c->datadir, "%s", "/var/lib/svgd-collect/rrd");
    snprintf(c->host, sizeof c->host, "%s", "localhost");
    c->rrdcached[0] = '\0';
    c->readers_count = 0;
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return -1; }
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return -1; }
    size_t rd = fread(buf, 1, (size_t)sz, f); buf[rd] = '\0'; fclose(f);

    c->interval = extract_int(buf, "\"interval\"", c->interval);
    extract_string(buf, "\"datadir\"", c->datadir, sizeof c->datadir);
    extract_string(buf, "\"hostname\"", c->host, sizeof c->host);
    extract_string(buf, "\"rrdcached_addr\"", c->rrdcached, sizeof c->rrdcached);

    const char *rp = strstr(buf, "\"readers\"");
    if (rp) {
        const char *lb = strchr(rp, '[');
        const char *rb = lb ? strchr(lb, ']') : NULL;   /* array bound; stop at ']' */
        const char *cur = lb ? lb + 1 : NULL;
        while (cur && c->readers_count < 16) {
            const char *q1 = strchr(cur, '"');
            if (!q1 || (rb && q1 >= rb)) break;
            const char *q2 = strchr(q1 + 1, '"');
            if (!q2) break;
            size_t len = (size_t)(q2 - q1 - 1);
            if (len >= sizeof c->readers[0]) len = sizeof c->readers[0] - 1;
            memcpy(c->readers[c->readers_count], q1 + 1, len);
            c->readers[c->readers_count][len] = '\0';
            c->readers_count++;
            cur = q2 + 1;
        }
    }
    free(buf);
    return 0;
}
