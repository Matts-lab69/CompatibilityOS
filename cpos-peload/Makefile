# SPDX-License-Identifier: MIT
CC ?= clang
FUZZ_SECONDS ?= 20
CFLAGS ?= -O1 -g
SANITIZERS = -fsanitize=address,undefined -fno-omit-frame-pointer

.PHONY: check check-exec fuzz clean
check:
	$(CC) $(CFLAGS) -std=c11 -Wall -Wextra -Werror $(SANITIZERS) pe.c test_pe.c -o test_pe
	./test_pe

check-exec:
	$(CC) $(CFLAGS) -std=c11 -Wall -Wextra -Werror $(SANITIZERS) -DCPOS_TEST_EXEC pe.c pe_exec.c test_pe.c -o test_exec
	./test_exec
	x86_64-w64-mingw32-gcc -Wall -Wextra -Werror -nostdlib -Wl,--entry,fixture_entry,--image-base,0x140000000 fixture.c -o fixture.exe
	timeout 10 ./test_exec --execute-fixture fixture.exe

fuzz: check
	$(CC) $(CFLAGS) -std=c11 -Wall -Wextra -Werror $(SANITIZERS) -fsanitize=fuzzer pe.c fuzz_pe.c -o fuzz_pe
	mkdir -p '.corpus'
	./test_pe '.corpus/minimal.pe'
	./fuzz_pe -max_total_time=$(FUZZ_SECONDS) -max_len=1048576 -timeout=2 '.corpus'

clean:
	rm -f test_pe test_exec fuzz_pe fixture.exe
