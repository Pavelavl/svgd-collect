#ifndef SVGD_COLLECT_MINITEST_H
#define SVGD_COLLECT_MINITEST_H
#include <stdio.h>
#include <string.h>
static int __mt_failures = 0;
#define TEST(name) static void name(void)
#define RUN(name) do { printf("  %s ... ", #name); name(); printf("ok\n"); } while (0)
#define ASSERT(cond) do { if (!(cond)) { printf("FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); __mt_failures++; return; } } while (0)
#define ASSERT_STR(a, b) ASSERT(strcmp((a), (b)) == 0)
#define TEST_MAIN() int main(void) { __mt_failures = 0;
#define TEST_RETURN() printf("%d failure(s)\n", __mt_failures); return __mt_failures ? 1 : 0; }
#endif
