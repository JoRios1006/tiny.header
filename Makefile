CC      = gcc
CPPFLAGS =
CFLAGS  = -nostdlib -g -fno-omit-frame-pointer -fno-builtin \
           -O2 -ffunction-sections -fdata-sections \
           -fno-unwind-tables -fno-asynchronous-unwind-tables \
           -fno-stack-protector -fno-ident \
           -Wl,--gc-sections \
           -Wl,--build-id=none \
           -Wl,-T,tiny.ld \
           -D_TINY_NOSTDLIB

# libc flags for quick test builds (no tiny.ld, no _start)
CFLAGS_LIBC = -g -fno-omit-frame-pointer -fno-builtin -O2
CFLAGS_TEST = -std=gnu11 -g -O2 -fno-omit-frame-pointer \
              -Wall -Wextra -Werror

TARGET  = prog
SRC     = example.c
TEST_BIN = test_runner
UNIT_TEST_BIN = build/unit_tests
UNIT_TEST_SRC = tests/test_tiny.c

all: $(TARGET)
	sstrip $(TARGET)
	@wc -c $(TARGET)

$(TARGET): $(SRC) tiny.ld
	$(CC) $(CFLAGS) -o $@ $<

# ── test targets ────────────────────────────────────────────────────────────
# test: deterministic unit tests for the pure tiny.h API
test: $(UNIT_TEST_BIN)
	./$(UNIT_TEST_BIN)

$(UNIT_TEST_BIN): $(UNIT_TEST_SRC) tiny.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS_TEST) -I. -o $@ $(UNIT_TEST_SRC)

# test_nostdlib: low-level integration suite (requires tiny.ld)
test_nostdlib: $(TEST_BIN)_nostdlib
	./$(TEST_BIN)_nostdlib
	@echo ""
	@echo "nostdlib test done"

# test_integration_libc: raw-syscall integration suite with a libc entry point
test_integration_libc: $(TEST_BIN)_libc
	./$(TEST_BIN)_libc
	@echo ""
	@echo "libc test done"

# test_all: unit tests plus both low-level integration modes
test_all: test test_nostdlib test_integration_libc

$(TEST_BIN)_nostdlib: test.c tiny.h tiny.ld
	$(CC) $(CFLAGS) -o $@ test.c

$(TEST_BIN)_libc: test.c tiny.h
	$(CC) $(CFLAGS_LIBC) -o $@ test.c

clean:
	rm -rf build
	rm -f $(TARGET) $(TEST_BIN)_nostdlib $(TEST_BIN)_libc

.PHONY: all test test_nostdlib test_integration_libc test_all clean
