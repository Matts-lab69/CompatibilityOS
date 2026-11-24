<!-- SPDX-License-Identifier: MIT -->
# cpos-peload parser

This isolated experimental C component accepts PE32+ AMD64 images only. The parser and `pe_stage` remain non-executable and suitable for parser/fuzz testing. `pe_execute_fixture` is a separate Linux/x86-64 fixture runner: it applies AMD64 DIR64 relocations, resolves exact `(module, symbol)` pairs only from the caller-provided allowlist, maps from anonymous memory, and changes section permissions without permitting a section to be both writable and executable. Entry invocation is opt-in and uses a deliberately narrow, no-argument fixture contract.

The fixture runner supports bounded process-attach TLS callbacks, but not static TLS or TEB/PEB emulation. It does not support arbitrary PE files, OS DLLs, Linux library fallback, Windows process initialization or exception unwinding. Entrypoint code runs natively; use only project-owned synthetic fixtures in isolated CI. Do not invoke untrusted images. Unsupported platforms return `PE_UNSUPPORTED`.

CI runs `make check` for synthetic parser inputs, `make check-exec` for isolated execution, and `make fuzz` for at least ten million libFuzzer iterations with synthetic seeds and one MiB per input. Targets use AddressSanitizer and UndefinedBehaviorSanitizer. Do not build or execute them locally.
