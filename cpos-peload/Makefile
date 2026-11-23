# SPDX-License-Identifier: MIT
CC ?= clang
FUZZ_SECONDS ?= 20
CFLAGS ?= -O1 -g
SANITIZERS = -fsanitize=address,undefined -fno-omit-frame-pointer

.PHONY: check fuzz clean
check:
	$(CC) $(CFLAGS) -std=c11 -Wall -Wextra -Werror $(SANITIZERS) pe.c test_pe.c -o test_pe
	./test_pe

fuzz: check
	$(CC) $(CFLAGS) -std=c11 -Wall -Wextra -Werror $(SANITIZERS) -fsanitize=fuzzer pe.c fuzz_pe.c -o fuzz_pe
	mkdir -p '.corpus'
	./test_pe '.corpus/minimal.pe'
	./fuzz_pe -max_total_time=$(FUZZ_SECONDS) -max_len=1048576 -timeout=2 '.corpus'

clean:
	rm -f test_pe fuzz_pe
