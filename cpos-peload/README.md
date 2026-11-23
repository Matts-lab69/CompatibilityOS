<!-- SPDX-License-Identifier: MIT -->
# cpos-peload parser

This isolated experimental C component accepts PE32+ AMD64 images only. The parser and `pe_stage` remain non-executable and suitable for parser/fuzz testing. `pe_execute_fixture` is a separate Linux/x86-64 fixture runner: it applies AMD64 DIR64 relocations, resolves exact `(module, symbol)` pairs only from the caller-provided allowlist, maps from anonymous memory, and changes section permissions without permitting a section to be both writable and executable. Entry invocation is opt-in and uses a deliberately narrow, no-argument fixture contract.

The fixture runner supports bounded process-attach TLS callbacks, but not static TLS or TEB/PEB emulation. It does not support arbitrary PE files, OS DLLs, Linux library fallback, Windows process initialization or exception unwinding. Entrypoint code runs natively; use only project-owned synthetic fixtures in isolated CI. Do not invoke untrusted images. Unsupported platforms return `PE_UNSUPPORTED`.

Run `make check` for parser synthetic inputs, `make check-exec` for the isolated allowlist/relocation mapping fixture, or `make fuzz` for a bounded libFuzzer session. Targets use AddressSanitizer and UndefinedBehaviorSanitizer; fuzzing is limited to 20 seconds by default and one MiB per input. CI is the only authorized build/test environment for this project; do not build or execute locally.
