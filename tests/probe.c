/* SPDX-License-Identifier: MIT */
#include <windows.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    const char *key = "Software\\CompatibilityOS\\ViewProbe";
    DWORD bits = sizeof(void *) * 8, value = 0, size = sizeof(value);
    HKEY handle;
    HANDLE file;

    if (argc > 1 && !strcmp(argv[1], "hold")) { Sleep(8000); return 0; }
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
