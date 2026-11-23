<!-- SPDX-License-Identifier: MIT -->
# cpos-peload parser

This isolated experimental C component accepts PE32+ AMD64 images only. It validates bounded header and section ranges and checks relocation and import directory metadata. It does not resolve imports, apply relocations, map executable memory, or execute image bytes. `pe_stage` copies validated file content into ordinary zero-initialized, non-executable heap memory.

Run `make check` for synthetic unit inputs or `make fuzz` for a bounded libFuzzer session. Both targets use AddressSanitizer and UndefinedBehaviorSanitizer; fuzzing is limited to 20 seconds by default and one MiB per input. CI is the only authorized build/test environment for this project.
