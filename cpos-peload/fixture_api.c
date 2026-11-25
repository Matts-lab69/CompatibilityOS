/* SPDX-License-Identifier: MIT */
typedef int BOOL;
typedef void *HANDLE;
typedef unsigned long DWORD;

__declspec(dllimport) unsigned int CposDependencyValue(void);
__declspec(dllimport) unsigned int CposFixtureHostReady(unsigned int step);
static volatile unsigned int api_attached;

BOOL DllMain(HANDLE module, DWORD reason, void *reserved)
{
    (void)module;
    (void)reserved;
    if (reason == 1) api_attached = CposFixtureHostReady(2) == 0x29U;
    if (reason == 0) CposFixtureHostReady(3);
    return 1;
}

unsigned int CposApiDirect(void)
{
    return api_attached ? CposDependencyValue() + 1U : 0U;
}

unsigned int CposDllAttachState(void)
{
    return api_attached ? 0x51U : 0U;
}
