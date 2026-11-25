# SPDX-License-Identifier: MIT
CC ?= clang
FUZZ_ITERATIONS ?= 10000000
CFLAGS ?= -O1 -g
SANITIZERS = -fsanitize=address,undefined -fno-omit-frame-pointer

.PHONY: check check-exec build-dll-fixtures build-delay-fixture fuzz clean
check:
	$(CC) $(CFLAGS) -std=c11 -Wall -Wextra -Werror $(SANITIZERS) pe.c test_pe.c -o test_pe
	./test_pe

check-exec:
	$(CC) $(CFLAGS) -std=c11 -Wall -Wextra -Werror $(SANITIZERS) -DCPOS_TEST_EXEC pe.c pe_exec.c test_pe.c -o test_exec
	./test_exec
	x86_64-w64-mingw32-gcc -Wall -Wextra -Werror -nostdlib -Wl,--entry,fixture_entry,--image-base,0x140000000 fixture.c -o fixture.exe
	timeout 10 ./test_exec --execute-fixture fixture.exe
	x86_64-w64-mingw32-dlltool -d fixture_import.def -l fixture_import.lib -D cposhello.dll
	x86_64-w64-mingw32-gcc -Wall -Wextra -Werror -nostdlib -Wl,--entry,fixture_entry,--image-base,0x140000000 fixture_import.c fixture_import.lib -o fixture_import.exe
	timeout 10 ./test_exec --execute-import-fixture fixture_import.exe > 'fixture_import.out'
	printf 'CPOS PE hello\n' | cmp - 'fixture_import.out'
	$(MAKE) build-dll-fixtures
	timeout 10 ./test_exec --execute-dll-fixture fixture_dll.exe cposapi.dll cposdep.dll
	$(MAKE) build-delay-fixture
	timeout 10 ./test_exec --execute-delay-fixture fixture_delay.exe cposapi.dll cposdep.dll

build-dll-fixtures:
	x86_64-w64-mingw32-dlltool -d fixture_host.def -l fixture_host.lib -D cposhost.dll
	x86_64-w64-mingw32-gcc -Wall -Wextra -Werror -nostdlib -shared -Wl,--entry,DllMain,--out-implib,fixture_dep.lib fixture_dep.c fixture_host.lib -o cposdep.dll
	x86_64-w64-mingw32-dlltool -d fixture_api.def -l fixture_api.lib -D cposapi.dll
	x86_64-w64-mingw32-gcc -Wall -Wextra -Werror -nostdlib -shared -Wl,--entry,DllMain fixture_api.c fixture_api.def fixture_dep.lib fixture_host.lib -o cposapi.dll
	x86_64-w64-mingw32-gcc -Wall -Wextra -Werror -nostdlib -Wl,--entry,fixture_dll_entry,--image-base,0x140000000 fixture_dll.c fixture_api.lib -o fixture_dll.exe

build-delay-fixture:
	x86_64-w64-mingw32-gcc -Wall -Wextra -Werror -nostdlib -Wl,--entry,fixture_delay_entry,--image-base,0x140000000 fixture_delay_import.S -o fixture_delay.exe
	python3 prepare_delay_fixture.py fixture_delay.exe

fuzz: check
	$(CC) $(CFLAGS) -std=c11 -Wall -Wextra -Werror $(SANITIZERS) -fsanitize=fuzzer pe.c fuzz_pe.c -o fuzz_pe
	mkdir -p '.corpus'
	./test_pe '.corpus/minimal.pe'
	python3 'make_corpus.py' '.corpus'
	bash -o pipefail -c './fuzz_pe -runs=$(FUZZ_ITERATIONS) -max_len=1048576 -timeout=2 -print_final_stats=1 .corpus 2>&1 | tee .fuzz.log && grep -Ex "stat::number_of_executed_units: [1-9][0-9]{7,}" .fuzz.log'

clean:
	rm -f test_pe test_exec fuzz_pe fixture.exe fixture_import.exe fixture_import.lib fixture_import.out cposdep.dll cposapi.dll fixture_dep.lib fixture_host.lib fixture_api.lib fixture_dll.exe fixture_delay.exe .fuzz.log
