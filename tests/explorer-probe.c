/* SPDX-License-Identifier: MIT */
#include <windows.h>
#include <stdio.h>

#define FIXTURE_DESKTOP_NAME L"CPOSExplorerFixture"

static DWORD WINAPI probe_desktop(LPVOID parameter)
{
    HDESK desktop = (HDESK)parameter;
    HWND window;

    if (!SetThreadDesktop(desktop)) {
        fwprintf(stderr, L"SetThreadDesktop failed: %lu\n", GetLastError());
        return 1;
    }

    window = GetDesktopWindow();
    if (!window || !IsWindow(window)) {
        fwprintf(stderr, L"GetDesktopWindow returned an invalid HWND\n");
        return 1;
    }

    wprintf(L"Explorer probe passed: desktop=%ls HWND=%p\n",
            FIXTURE_DESKTOP_NAME, (void *)window);
    return 0;
}

int wmain(void)
{
    HDESK desktop = NULL;
    HANDLE thread = NULL;
    DWORD thread_result = 1;
    int result = 1;

    desktop = CreateDesktopW(FIXTURE_DESKTOP_NAME, NULL, NULL, 0,
                             DESKTOP_CREATEWINDOW | DESKTOP_READOBJECTS |
                                 DESKTOP_WRITEOBJECTS,
                             NULL);
    if (!desktop) {
        fwprintf(stderr, L"CreateDesktopW failed: %lu\n", GetLastError());
        goto done;
    }

    thread = CreateThread(NULL, 0, probe_desktop, desktop, 0, NULL);
    if (!thread) {
        fwprintf(stderr, L"CreateThread failed: %lu\n", GetLastError());
        goto done;
    }

    if (WaitForSingleObject(thread, INFINITE) != WAIT_OBJECT_0) {
        fwprintf(stderr, L"WaitForSingleObject failed: %lu\n", GetLastError());
        goto done;
    }
    if (!GetExitCodeThread(thread, &thread_result)) {
        fwprintf(stderr, L"GetExitCodeThread failed: %lu\n", GetLastError());
        goto done;
    }
    result = thread_result == 0 ? 0 : 1;

done:
    if (thread && !CloseHandle(thread)) {
        fwprintf(stderr, L"CloseHandle failed: %lu\n", GetLastError());
        result = 1;
    }
    if (desktop && !CloseDesktop(desktop)) {
        fwprintf(stderr, L"CloseDesktop failed: %lu\n", GetLastError());
        result = 1;
    }
    return result;
}
