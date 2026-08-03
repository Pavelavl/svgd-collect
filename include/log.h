/** @file log.h @brief minimal stderr logging for non-fatal reader/writer errors
 *
 * Readers and the writer must not fail silently: a missing /proc file, a parse
 * error, or an unreachable rrdcached should leave a clear one-line message on
 * stderr. These helpers enforce a consistent "svgd-collect[<ctx>]: ..." prefix
 * so the messages are greppable and attributable. They never abort the process.
 */
#ifndef SVGD_COLLECT_LOG_H
#define SVGD_COLLECT_LOG_H
/** Log a formatted error to stderr, prefixed "svgd-collect[<ctx>]: ".
 *  @param ctx short component name (reader/plugin name, or "writer"/"registry").
 *  @param fmt printf-style format string. */
void log_err(const char *ctx, const char *fmt, ...);
/** Log "<path>: <strerror(errno)>" — the common open/stat failure path.
 *  Reads errno, so call it immediately after the failing libc call. */
void log_errno(const char *ctx, const char *path);
#endif
