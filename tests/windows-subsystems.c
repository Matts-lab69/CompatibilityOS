/* SPDX-License-Identifier: MIT */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wingdi.h>
#include <winspool.h>
#include <setupapi.h>
#include <winreg.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static int CALLBACK font_callback(const LOGFONTW *, const TEXTMETRICW *, DWORD, LPARAM);

static int fixture_font_visible(void)
{
    LOGFONTW query = {0};
    HDC dc = CreateCompatibleDC(NULL);
    BOOL found = FALSE;

    if (!dc) return -1;
    query.lfCharSet = DEFAULT_CHARSET;
    lstrcpyW(query.lfFaceName, L"CPOS Fixture Sans");
    EnumFontFamiliesExW(dc, &query, font_callback, (LPARAM)&found, 0);
    DeleteDC(dc);
    return found;
}

static int font_fixture(void)
{
    typedef LSTATUS (WINAPI *reg_create_key_fn)(HKEY, LPCWSTR, DWORD, LPWSTR, DWORD,
                                                REGSAM, const LPSECURITY_ATTRIBUTES,
                                                PHKEY, LPDWORD);
    typedef LSTATUS (WINAPI *reg_set_value_fn)(HKEY, LPCWSTR, DWORD, DWORD,
                                               const BYTE *, DWORD);
    typedef LSTATUS (WINAPI *reg_query_value_fn)(HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
    typedef LSTATUS (WINAPI *reg_delete_value_fn)(HKEY, LPCWSTR);
    typedef LSTATUS (WINAPI *reg_close_key_fn)(HKEY);
    WCHAR source[1024], target[1300], registered[1300];
    WCHAR value_name[64], registry_path[] = L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts";
    DWORD length, registered_size = sizeof(registered), type = 0, expected_size;
    HMODULE advapi = NULL;
    reg_create_key_fn reg_create_key;
    reg_set_value_fn reg_set_value;
    reg_query_value_fn reg_query_value;
    reg_delete_value_fn reg_delete_value;
    reg_close_key_fn reg_close_key;
    HKEY key = NULL;
    BOOL copied = FALSE, added = FALSE, value_created = FALSE;
    int result = 1;

    length = GetEnvironmentVariableW(L"CPOS_TEST_FONT_FILE", source, ARRAYSIZE(source));
    if (!length) {
        puts("fonts: no optional fixture configured");
        return 0;
    }
    if (length >= ARRAYSIZE(source)) {
        puts("fonts: CPOS_TEST_FONT_FILE path is too long");
        return 1;
    }
    if (fixture_font_visible() != FALSE) {
        puts("fonts: fixture family must be absent before loading");
        return 1;
    }
    if (swprintf(target, ARRAYSIZE(target), L"C:\\Windows\\Fonts\\CPOS-CI-%lu.ttf", GetCurrentProcessId()) < 0 ||
        swprintf(value_name, ARRAYSIZE(value_name), L"CPOS CI Probe %lu", GetCurrentProcessId()) < 0) {
        puts("fonts: could not construct fixture paths");
        return 1;
    }
    if (!CopyFileW(source, target, TRUE)) {
        printf("fonts: copy fixture to per-user Fonts directory failed (%lu)\n", GetLastError());
        return 1;
    }
    copied = TRUE;

    advapi = LoadLibraryW(L"advapi32.dll");
    if (!advapi) { printf("fonts: load advapi32 failed (%lu)\n", GetLastError()); goto cleanup; }
    reg_create_key = (reg_create_key_fn)GetProcAddress(advapi, "RegCreateKeyExW");
    reg_set_value = (reg_set_value_fn)GetProcAddress(advapi, "RegSetValueExW");
    reg_query_value = (reg_query_value_fn)GetProcAddress(advapi, "RegQueryValueExW");
    reg_delete_value = (reg_delete_value_fn)GetProcAddress(advapi, "RegDeleteValueW");
    reg_close_key = (reg_close_key_fn)GetProcAddress(advapi, "RegCloseKey");
    if (!reg_create_key || !reg_set_value || !reg_query_value || !reg_delete_value || !reg_close_key) {
        puts("fonts: required registry API unavailable");
        goto cleanup;
    }
    if (reg_create_key(HKEY_CURRENT_USER, registry_path, 0, NULL, 0, KEY_SET_VALUE | KEY_QUERY_VALUE,
                       NULL, &key, NULL) != ERROR_SUCCESS) {
        puts("fonts: could not open per-user Fonts registry key");
        goto cleanup;
    }
    expected_size = (DWORD)((wcslen(target) + 1) * sizeof(WCHAR));
    if (reg_set_value(key, value_name, 0, REG_SZ, (const BYTE *)target, expected_size) != ERROR_SUCCESS) {
        puts("fonts: could not create CI registry entry");
        goto cleanup;
    }
    value_created = TRUE;
    if (reg_query_value(key, value_name, NULL, &type, (BYTE *)registered, &registered_size) != ERROR_SUCCESS ||
        type != REG_SZ || registered_size != expected_size || memcmp(target, registered, expected_size)) {
        puts("fonts: per-user registry value did not match the copied fixture path");
        goto cleanup;
    }
    if (!AddFontResourceExW(target, FR_PRIVATE, NULL)) {
        printf("fonts: AddFontResourceExW failed (%lu)\n", GetLastError());
        goto cleanup;
    }
    added = TRUE;
    if (fixture_font_visible() != TRUE) {
        puts("fonts: fixture family is not visible after loading");
        goto cleanup;
    }
    puts("fonts: private fixture loaded and per-user registry entry created");
    result = 0;

cleanup:
    if (added && !RemoveFontResourceExW(target, FR_PRIVATE, NULL)) {
        printf("fonts: RemoveFontResourceExW failed (%lu)\n", GetLastError());
        result = 1;
    } else if (added && fixture_font_visible() != FALSE) {
        puts("fonts: fixture family remains visible after removal");
        result = 1;
    }
    if (key) {
        if (value_created && reg_delete_value(key, value_name) != ERROR_SUCCESS) {
            puts("fonts: failed to remove CI registry entry");
            result = 1;
        }
        if (reg_close_key(key) != ERROR_SUCCESS) {
            puts("fonts: failed to close Fonts registry key");
            result = 1;
        }
    }
    if (copied && !DeleteFileW(target)) {
        printf("fonts: failed to remove copied fixture (%lu)\n", GetLastError());
        result = 1;
    }
    if (advapi) FreeLibrary(advapi);
    return result;
}

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
    (void)metric; (void)type;
    if (data && !lstrcmpiW(font->lfFaceName, L"CPOS Fixture Sans")) {
        *(BOOL *)data = TRUE;
        return 0;
    }
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
    RUN("font-fixture", font_fixture);
    RUN("spool", spool);
    RUN("plug-and-play-fixture", plug_and_play);
    RUN("fusion", fusion);
    return failed ? 1 : 0;
}
