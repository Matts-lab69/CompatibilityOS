/* SPDX-License-Identifier: MIT */
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>

static int known_folders(void)
{
    static const GUID ids[] = {
        {0xfdd39ad0,0x238f,0x46af,{0xad,0xb4,0x6c,0x85,0x48,0x03,0x69,0xc7}},
        {0xb4bfcc3a,0xdb2c,0x424c,{0xb0,0x29,0x7f,0xe9,0x9a,0x87,0xc6,0x41}},
        {0x374de290,0x123f,0x4565,{0x91,0x64,0x39,0xc4,0x92,0x5e,0x46,0x7b}},
    };
    for (unsigned i = 0; i < sizeof(ids) / sizeof(ids[0]); ++i)
    {
        WCHAR *root = NULL, name[MAX_PATH];
        HANDLE file;
        HRESULT status = SHGetKnownFolderPath(&ids[i], 0, NULL, &root);
        if (FAILED(status)) { printf("Known folder %u: 0x%lx\n", i, (unsigned long)status); return 1; }
        if (lstrlenW(root) > MAX_PATH - 32) { CoTaskMemFree(root); return 1; }
        lstrcpyW(name, root);
        CoTaskMemFree(root);
        lstrcatW(name, L"\\cpos-known-folder.txt");
        file = CreateFileW(name, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file == INVALID_HANDLE_VALUE) { printf("Known folder %u: %lu\n", i, GetLastError()); return 1; }
        CloseHandle(file);
    }
    return 0;
}

int main(int argc, char **argv)
{
    const char *key = "Software\\CompatibilityOS\\ViewProbe";
    DWORD bits = sizeof(void *) * 8, value = 0, size = sizeof(value);
    HKEY handle;
    HANDLE file;

    if (argc > 1 && !strcmp(argv[1], "hold")) { Sleep(8000); return 0; }
    if (argc > 1 && !strcmp(argv[1], "known-folders")) return known_folders();
    if (argc > 1 && !strcmp(argv[1], "path"))
    {
        file = CreateFileW(L"C:\\Windows\\Installer\\cpos-probe.txt", GENERIC_WRITE, 0, NULL,
                          CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file == INVALID_HANDLE_VALUE) { printf("CreateFileW: %lu\n", GetLastError()); return 1; }
        CloseHandle(file);
        return 0;
    }
    printf("PE%lu started\n", bits);
    if (argc < 2) return 0;
    if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, key, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &handle, NULL)) return 2;
    if (!strcmp(argv[1], "write"))
    {
        if (RegSetValueExA(handle, "Bits", 0, REG_DWORD, (const BYTE *)&bits, sizeof(bits))) return 3;
    }
    else if (RegQueryValueExA(handle, "Bits", NULL, NULL, (BYTE *)&value, &size) || value != bits)
    {
        printf("view mismatch: %lu != %lu\n", value, bits);
        return 4;
    }
    RegCloseKey(handle);
    return 0;
}
