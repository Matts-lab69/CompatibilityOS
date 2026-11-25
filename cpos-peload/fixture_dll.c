/* SPDX-License-Identifier: MIT */
static unsigned int relocation_marker;
static unsigned int *const relocation_anchor __attribute__((used)) = &relocation_marker;
__declspec(dllimport) unsigned int CposApiDirect(void);
__declspec(dllimport) unsigned int CposDllAttachState(void);
__declspec(dllimport) unsigned int CposApiForward(void);

void fixture_dll_entry(void)
{
    if (CposApiDirect() != 0x2bU) __builtin_trap();
    if (CposDllAttachState() != 0x51U) __builtin_trap();
    if (CposApiForward() != 0x2aU) __builtin_trap();
}
