/* SPDX-License-Identifier: MIT
 * Project-owned x86-64 MinGW fixture, built by build-delay-fixture.
 * ABI: PE32+ AMD64, Microsoft x64 ABI, no CRT. The loader compares the
 * dedicated IAT slot before and after invoking this entry twice.
 * The entry calls that dedicated IAT slot and traps unless CposDependencyValue
 * returns 0x2a. This is a fixture contract, not proof that metadata alone is
 * lazily resolved. No host API or injection interface is used.
 */

	.section .text
	.globl fixture_delay_entry
	.def fixture_delay_entry; .scl 2; .type 32; .endef
fixture_delay_entry:
	subq $40, %rsp
	call *fixture_delay_iat(%rip)
	addq $40, %rsp
	cmpl $0x2a, %eax
	jne 1f
	ret
1:
	ud2

	.section .rdata$didata,"dr"
	.p2align 3
	.globl fixture_delay_descriptor
fixture_delay_descriptor:
	.long 1                         /* dlattrRva: all descriptor pointers are RVAs */
	.rva fixture_delay_name
	.rva fixture_delay_module
	.rva fixture_delay_iat
	.rva fixture_delay_int
	.long 0                         /* bound IAT */
	.long 0                         /* unload IAT */
	.long 0                         /* timestamp */
	.long 0                         /* null descriptor terminator */
	.long 0
	.long 0
	.long 0
	.long 0
	.long 0
	.long 0
	.long 0

	.section .rdata$diname,"dr"
fixture_delay_name:
	.asciz "cposdep.dll"
	.p2align 1
fixture_delay_import_name:
	.short 0                         /* hint */
	.asciz "CposDependencyValue"

	.section .data$dimod,"dw"
	.p2align 3
fixture_delay_module:
	.quad 0
	.quad fixture_delay_entry        /* absolute reference for a DIR64 relocation */

	.section .data$diat,"dw"
	.p2align 3
	.globl fixture_delay_iat
fixture_delay_iat:
	.rva fixture_delay_import_name
	.long 0
	.quad 0                         /* null IAT terminator */

	.section .rdata$diint,"dr"
	.p2align 3
fixture_delay_int:
	.rva fixture_delay_import_name
	.long 0
	.quad 0                         /* null lookup terminator */
