/* SPDX-License-Identifier: MIT */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wingdi.h>
#include <stdio.h>
#include <wchar.h>

static BOOL CALLBACK find_fixture(const LOGFONTW *font, const TEXTMETRICW *metric,
                                 DWORD type, LPARAM data)
{
    (void)metric;
    (void)type;
    if (!lstrcmpiW(font->lfFaceName, L"CPOS Fixture Sans")) {
        *(BOOL *)data = TRUE;
        return 0;
    }
    return 1;
}

static int visible(void)
{
    LOGFONTW query = {0};
    HDC dc = CreateCompatibleDC(NULL);
    BOOL found = FALSE;
    if (!dc) {
        printf("CreateCompatibleDC failed: %lu\n", GetLastError());
        return -1;
    }
    query.lfCharSet = DEFAULT_CHARSET;
    lstrcpyW(query.lfFaceName, L"CPOS Fixture Sans");
    EnumFontFamiliesExW(dc, &query, find_fixture, (LPARAM)&found, 0);
    DeleteDC(dc);
    return found;
}

int wmain(int argc, wchar_t **argv)
{
    WCHAR path[MAX_PATH], source[MAX_PATH];
    HKEY key;
    if (argc != 2) return 2;
    if (!lstrcmpW(argv[1], L"absent")) {
        if (visible() != FALSE) { puts("persistent font absence not established"); return 1; }
        puts("persistent font absent initially");
        return 0;
    }
    if (!lstrcmpW(argv[1], L"install")) {
        DWORD length = GetEnvironmentVariableW(L"CPOS_TEST_FONT_FILE", source, ARRAYSIZE(source));
        if (!length || length >= ARRAYSIZE(source) || visible()) return 1;
        if (!GetWindowsDirectoryW(path, ARRAYSIZE(path))) return 1;
        if (wcscat_s(path, ARRAYSIZE(path), L"\\Fonts\\CPOS-Persistent.ttf")) return 1;
        if (!CopyFileW(source, path, TRUE)) return 1;
        if (RegCreateKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts", 0, NULL, 0,
            KEY_SET_VALUE, NULL, &key, NULL) != ERROR_SUCCESS) { DeleteFileW(path); return 1; }
        if (RegSetValueExW(key, L"CPOS Persistent Fixture", 0, REG_SZ,
            (const BYTE *)path, (DWORD)((wcslen(path) + 1) * sizeof(WCHAR))) != ERROR_SUCCESS) {
            RegCloseKey(key); DeleteFileW(path); return 1;
        }
        RegCloseKey(key);
        puts("persistent font installed and registered");
        return 0;
    }
    if (!lstrcmpW(argv[1], L"check")) {
        if (visible() != TRUE) { puts("persistent font not enumerated after server restart"); return 1; }
        puts("persistent font enumerated without AddFontResource");
        return 0;
    }
    if (!lstrcmpW(argv[1], L"cleanup")) {
        if (!GetWindowsDirectoryW(path, ARRAYSIZE(path))) return 1;
        if (wcscat_s(path, ARRAYSIZE(path), L"\\Fonts\\CPOS-Persistent.ttf")) return 1;
        if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts", 0,
            KEY_SET_VALUE, &key) != ERROR_SUCCESS) return 1;
        if (RegDeleteValueW(key, L"CPOS Persistent Fixture") != ERROR_SUCCESS) {
            RegCloseKey(key); return 1;
        }
        RegCloseKey(key);
        return DeleteFileW(path) ? 0 : 1;
    }
    return 2;
}
