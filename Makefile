CC      = gcc
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

TARGET  = prog
SRC     = example.c
TEST_BIN = test_runner

all: $(TARGET)
	sstrip $(TARGET)
	@wc -c $(TARGET)

$(TARGET): $(SRC) tiny.ld
	$(CC) $(CFLAGS) -o $@ $<

# ── test targets ────────────────────────────────────────────────────────────
# test: build with -nostdlib + tiny.ld (matches prod build)
test: $(TEST_BIN)_nostdlib
	./$(TEST_BIN)_nostdlib
	@echo ""
	@echo "nostdlib test done"

# test_libc: build with libc (faster iteration, no linker script needed)
test_libc: $(TEST_BIN)_libc
	./$(TEST_BIN)_libc
	@echo ""
	@echo "libc test done"

# both: run both modes
test_all: test test_libc

$(TEST_BIN)_nostdlib: test.c tiny.h tiny.ld
	$(CC) $(CFLAGS) -o $@ test.c

$(TEST_BIN)_libc: test.c tiny.h
	$(CC) $(CFLAGS_LIBC) -o $@ test.c

clean:
	rm -f $(TARGET) $(TEST_BIN)_nostdlib $(TEST_BIN)_libc

.PHONY: all test test_libc test_all clean
