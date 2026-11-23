/* SPDX-License-Identifier: MIT */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wingdi.h>
#include <winspool.h>
#include <setupapi.h>
#include <stdio.h>
#include <string.h>

static int CALLBACK font_callback(const LOGFONTW *, const TEXTMETRICW *, DWORD, LPARAM);

static int fonts(void)
{
    LOGFONTW lf;
    HDC dc = CreateCompatibleDC(NULL);
    int count;
    if (!dc) { printf("CreateCompatibleDC: %lu\n", GetLastError()); return 1; }
    ZeroMemory(&lf, sizeof(lf));
    lf.lfCharSet = DEFAULT_CHARSET;
    count = EnumFontFamiliesExW(dc, &lf, (FONTENUMPROCW)font_callback, 0, 0);
    DeleteDC(dc);
    if (count == GDI_ERROR) { printf("EnumFontFamiliesExW: %lu\n", GetLastError()); return 1; }
    printf("fonts: EnumFontFamiliesExW returned %d\n", count);
    return 0;
}

static int CALLBACK font_callback(const LOGFONTW *font, const TEXTMETRICW *metric,
                                  DWORD type, LPARAM data)
{
    (void)font; (void)metric; (void)type; (void)data;
    return 1;
}

static int spool(void)
{
    DWORD needed = 0, returned = 0;
    BOOL ok = EnumPrintersW(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 4,
                            NULL, 0, &needed, &returned);
    if (ok && !needed && !returned) {
        puts("spool: no printers configured");
        return 0;
    }
    if (ok || GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        printf("EnumPrintersW sizing: unexpected result %d, error %lu\n", ok, GetLastError());
        return 1;
    }
    if (!needed) { puts("EnumPrintersW sizing returned no buffer requirement"); return 1; }
    {
        BYTE *buffer = (BYTE *)HeapAlloc(GetProcessHeap(), 0, needed);
        if (!buffer) { puts("HeapAlloc failed"); return 1; }
        ok = EnumPrintersW(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 4,
                           buffer, needed, &needed, &returned);
        HeapFree(GetProcessHeap(), 0, buffer);
    }
    if (!ok) { printf("EnumPrintersW: %lu\n", GetLastError()); return 1; }
    printf("spool: EnumPrintersW returned %lu printers\n", returned);
    return 0;
}

static int plug_and_play(void)
{
    GUID class_id = {0x7f4c6e20,0x4e8d,0x4f35,{0x91,0x23,0x4a,0x6b,0x7c,0x8d,0x9e,0xaf}};
    HDEVINFO set = SetupDiCreateDeviceInfoList(&class_id, NULL);
    SP_DEVINFO_DATA info;
    BOOL ok;
    if (set == INVALID_HANDLE_VALUE) { printf("SetupDiCreateDeviceInfoList: %lu\n", GetLastError()); return 1; }
    ZeroMemory(&info, sizeof(info));
    info.cbSize = sizeof(info);
    ok = SetupDiCreateDeviceInfoW(set, L"CPOS_TEST_DEVICE", &class_id, NULL, NULL,
                                  DICD_GENERATE_ID, &info);
    if (!ok) printf("SetupDiCreateDeviceInfoW: %lu\n", GetLastError());
    SetupDiDestroyDeviceInfoList(set);
    if (!ok) return 1;
    puts("Plug and Play: created and discarded a synthetic device-info fixture");
    return 0;
}

static int fusion(void)
{
    typedef HRESULT (WINAPI *create_cache_fn)(void **, DWORD);
    typedef ULONG (WINAPI *release_fn)(void *);
    WCHAR path[1024];
    DWORD length = GetEnvironmentVariableW(L"CPOS_TEST_FUSION_DLL", path, ARRAYSIZE(path));
    HMODULE dll;
    create_cache_fn create_cache;
    void *cache = NULL;
    HRESULT hr;
    if (!length || length >= ARRAYSIZE(path)) {
        puts("Fusion: expected the packaged builtin DLL path");
        return 1;
    }
    dll = LoadLibraryW(path);
    if (!dll) { printf("Fusion: packaged fusion.dll could not be loaded (%lu)\n", GetLastError()); return 1; }
    create_cache = (create_cache_fn)GetProcAddress(dll, "CreateAssemblyCache");
    if (!create_cache) { printf("Fusion: CreateAssemblyCache unavailable (%lu)\n", GetLastError()); FreeLibrary(dll); return 1; }
    hr = create_cache(&cache, 0);
    if (FAILED(hr) || !cache) { printf("Fusion: CreateAssemblyCache returned 0x%08lx\n", (unsigned long)hr); FreeLibrary(dll); return 1; }
    ((release_fn)(*(void ***)cache)[2])(cache);
    FreeLibrary(dll);
    puts("Fusion: CreateAssemblyCache succeeded (runtime activation not tested)");
    return 0;
}

int main(void)
{
    int failed = 0;
#define RUN(name, fn) do { int result = (fn)(); printf("%s %s\n", result ? "FAIL" : "ok", name); failed |= result; } while (0)
    RUN("fonts", fonts);
    RUN("spool", spool);
    RUN("plug-and-play-fixture", plug_and_play);
    RUN("fusion", fusion);
    return failed ? 1 : 0;
}
