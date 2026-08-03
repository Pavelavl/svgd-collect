/** @file log.c @brief minimal stderr logging for non-fatal reader/writer errors */
#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>

void log_err(const char *ctx, const char *fmt, ...)
{
    fprintf(stderr, "svgd-collect[%s]: ", ctx ? ctx : "?");
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt ? fmt : "", ap);
    va_end(ap);
    fputc('\n', stderr);
}

void log_errno(const char *ctx, const char *path)
{
    fprintf(stderr, "svgd-collect[%s]: %s: %s\n",
            ctx ? ctx : "?", path ? path : "(null)", strerror(errno));
}
