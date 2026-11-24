/* SPDX-License-Identifier: MIT */
#define COBJMACROS
#include <windows.h>
#include <fusion.h>
#include <stdio.h>
#include <wchar.h>

typedef HRESULT (WINAPI *create_enum_fn)(IAssemblyEnum **, IUnknown *, IAssemblyName *, DWORD, LPVOID);
typedef HRESULT (WINAPI *create_name_fn)(IAssemblyName **, LPCWSTR, DWORD, LPVOID);
typedef HRESULT (WINAPI *create_cache_fn)(IAssemblyCache **, DWORD);

static int assembly_present(create_enum_fn create_enum, IAssemblyName *name, BOOL *present)
{
    IAssemblyEnum *enumerator = NULL;
    IAssemblyName *found = NULL;
    HRESULT hr;
    *present = FALSE;
    hr = create_enum(&enumerator, NULL, name, ASM_CACHE_GAC, NULL);
    if (FAILED(hr)) { wprintf(L"CreateAssemblyEnum: 0x%08lx\n", (unsigned long)hr); return 1; }
    hr = IAssemblyEnum_GetNextAssembly(enumerator, NULL, &found, 0);
    if (hr == S_OK) {
        *present = TRUE;
        IAssemblyName_Release(found);
    } else if (hr != S_FALSE) {
        wprintf(L"IAssemblyEnum::GetNextAssembly: 0x%08lx\n", (unsigned long)hr);
        IAssemblyEnum_Release(enumerator);
        return 1;
    }
    IAssemblyEnum_Release(enumerator);
    return 0;
}

int wmain(int argc, wchar_t **argv)
{
    IAssemblyCache *cache = NULL;
    IAssemblyName *name = NULL;
    BOOL installed = FALSE, present = FALSE;
    HRESULT hr;
    int result = 1;
    HMODULE fusion;
    create_cache_fn create_cache;
    create_enum_fn create_enum;
    create_name_fn create_name;

    if (argc != 4) {
        fwprintf(stderr, L"Usage: gac-probe.exe <fusion-dll> <fixture-dll> <full-assembly-display-name>\n");
        return 2;
    }
    fusion = LoadLibraryW(argv[1]);
    if (!fusion) { wprintf(L"LoadLibraryW(fusion): %lu\n", GetLastError()); return 1; }
    create_enum = (create_enum_fn)GetProcAddress(fusion, "CreateAssemblyEnum");
    create_name = (create_name_fn)GetProcAddress(fusion, "CreateAssemblyNameObject");
    create_cache = (create_cache_fn)GetProcAddress(fusion, "CreateAssemblyCache");
    if (!create_enum || !create_name || !create_cache) { puts("Missing required Fusion exports"); goto done; }
    hr = create_name(&name, argv[3], CANOF_PARSE_DISPLAY_NAME, NULL);
    if (FAILED(hr)) { wprintf(L"CreateAssemblyNameObject: 0x%08lx\n", (unsigned long)hr); goto done; }
    hr = create_cache(&cache, 0);
    if (FAILED(hr)) { wprintf(L"CreateAssemblyCache: 0x%08lx\n", (unsigned long)hr); goto done; }

    if (assembly_present(create_enum, name, &present) || present) {
        fputs("Unexpected initial GAC enumeration result\n", stderr);
        goto done;
    }
    hr = IAssemblyCache_InstallAssembly(cache, 0, argv[2], NULL);
    if (FAILED(hr)) { wprintf(L"IAssemblyCache::InstallAssembly: 0x%08lx\n", (unsigned long)hr); goto done; }
    installed = TRUE;
    if (assembly_present(create_enum, name, &present) || !present) {
        fwprintf(stderr, L"Installed assembly was not enumerated in the GAC.\n");
        goto cleanup;
    }
    result = 0;

cleanup:
    if (installed) {
        hr = IAssemblyCache_UninstallAssembly(cache, 0, argv[3], NULL, NULL);
        if (FAILED(hr)) {
            wprintf(L"IAssemblyCache::UninstallAssembly: 0x%08lx\n", (unsigned long)hr);
            result = 1;
        } else if (assembly_present(create_enum, name, &present) || present) {
            fwprintf(stderr, L"Assembly remains enumerable after uninstall.\n");
            result = 1;
        }
    }
done:
    if (cache) IAssemblyCache_Release(cache);
    if (name) IAssemblyName_Release(name);
    FreeLibrary(fusion);
    return result;
}
