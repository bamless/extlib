CC      ?= $(CC)
CFLAGS  ?= -Wall -Wextra -Wno-override-init -std=c99 -ggdb
LDFLAGS ?=

.PHONY: all
all: examples

.PHONY: examples
examples: examples/01_dynamic_array   \
	examples/01_inline_dynamic_array  \
	examples/02_hashmap               \
	examples/02_inline_hashmap        \
	examples/03_arena                 \
	examples/03_arena.wasm            \
	examples/04_cat                   \
	examples/05_ls

examples/%: examples/%.c extlib.h
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@

examples/03_arena.wasm: examples/03_arena.c extlib.h
	clang $(CFLAGS) -DEXTLIB_WASM=1                                   \
		-std=c99 -fno-builtin --target=wasm32 --no-standard-libraries \
		-Wl,--no-entry                                                \
		-Wl,--allow-undefined                                         \
		-Wl,--export=eval_expr                                        \
		-Wl,--export=ext_temp_alloc                                   \
		-Wl,--export=ext_temp_reset                                   \
		$< -o $@

.PHONY: test
test: test/test
	./test/test

test/test: ./test/test.c ./test/ctest.h extlib.h
	$(CC) $(CFLAGS) -Wno-attributes -Wno-pragmas -std=c99 $(LDFLAGS) -I./test/ ./test/test.c -o test/test

.PHONY: clean
clean:
	rm -rf test/test
	find ./examples -type f -executable -exec rm {} \;

.PHONY: format
format:
	find . -type f -iname '*.[ch]' -exec clang-format -i {} \;
