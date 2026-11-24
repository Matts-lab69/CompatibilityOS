/* SPDX-License-Identifier: MIT */
__declspec(dllimport) void CposPeHello(void);

static void call_fixture_import(void)
{
    CposPeHello();
}

/* Keep a DIR64 relocation for execution away from the preferred base. */
static void (*volatile entry_target)(void) = call_fixture_import;

void fixture_entry(void)
{
    entry_target();
}
