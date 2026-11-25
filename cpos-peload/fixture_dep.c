/* SPDX-License-Identifier: MIT */
typedef int BOOL;
typedef void *HANDLE;
typedef unsigned long DWORD;

__declspec(dllimport) unsigned int CposFixtureHostReady(unsigned int step);
static volatile unsigned int dep_attached;

BOOL DllMain(HANDLE module, DWORD reason, void *reserved)
{
    (void)module;
    (void)reserved;
    if (reason == 1) dep_attached = CposFixtureHostReady(1) == 0x29U;
    if (reason == 0) CposFixtureHostReady(4);
    return 1;
}

__declspec(dllexport) unsigned int CposDependencyValue(void)
{
    return dep_attached ? 0x2aU : 0U;
}
