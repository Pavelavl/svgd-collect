CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -g -Iinclude
LDLIBS  = -lrrd
BIN_DIR = bin

SRCS      = $(wildcard src/*.c) $(wildcard src/readers/*.c)
OBJS      = $(SRCS:.c=.o)
TEST_OBJS = $(filter-out src/main.o src/collect.o, $(OBJS))
TEST_SRCS = $(wildcard tests/test_*.c)

.PHONY: all build test clean
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
