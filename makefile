CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -g -Iinclude -pthread
LDLIBS  = -lrrd -lm -pthread
BIN_DIR = bin

SRCS      = $(wildcard src/*.c) $(wildcard src/readers/*.c)
OBJS      = $(SRCS:.c=.o)
# main.o owns g_running; collect.o + http.o reference it and aren't unit-tested
# directly (collect.c is exercised via integration.sh, http.c via /metrics curl).
TEST_OBJS = $(filter-out src/main.o src/collect.o src/http.o, $(OBJS))
TEST_SRCS = $(wildcard tests/test_*.c)

.PHONY: all build test clean test-integration test-integration-rrdcached test-integration-metrics
all: build
build: $(BIN_DIR)/svgd-collect
$(BIN_DIR)/svgd-collect: $(OBJS) | $(BIN_DIR)
	$(CC) -o $@ $(OBJS) $(CFLAGS) $(LDLIBS)
$(BIN_DIR):
	mkdir -p $(BIN_DIR)
src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@
src/readers/%.o: src/readers/%.c
	$(CC) $(CFLAGS) -c $< -o $@
test: $(TEST_OBJS) | $(BIN_DIR)
	@if [ -z "$(TEST_SRCS)" ]; then echo "no tests"; exit 0; fi
	@for t in $(TEST_SRCS); do \
	  base=$$(basename $$t .c); \
	  echo "== $$base =="; \
	  $(CC) $(CFLAGS) $$t $(TEST_OBJS) -o $(BIN_DIR)/$$base $(LDLIBS) || exit 1; \
	  ./$(BIN_DIR)/$$base || exit 1; \
	done
clean:
	rm -rf $(BIN_DIR) $(OBJS)
.PHONY: test-integration
test-integration: build
	@sh tests/integration.sh
.PHONY: test-integration-rrdcached
test-integration-rrdcached: build
	@sh tests/integration_rrdcached.sh
.PHONY: test-integration-metrics
test-integration-metrics: build
	@sh tests/integration_metrics.sh
