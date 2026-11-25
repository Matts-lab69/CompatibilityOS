/* SPDX-License-Identifier: MIT */
#include <windows.h>
#include <shlobj.h>
#include <objbase.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static const CLSID fixture_clsid = {0x6f25d812,0x9374,0x4c86,{0x91,0x22,0xb4,0x51,0xd1,0xac,0x70,0x39}};

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

static int writable_windows(void)
{
    static const WCHAR *names[] = {
        L"C:\\Windows\\Temp\\cpos-probe.txt",
        L"C:\\Windows\\System32\\spool\\cpos-probe.txt",
        L"C:\\Windows\\Fonts\\cpos-probe.txt",
        L"C:\\Windows\\assembly\\cpos-probe.txt",
        L"C:\\Windows\\Microsoft.NET\\assembly\\cpos-probe.txt",
        L"C:\\Windows\\WinSxS\\cpos-probe.txt",
    };
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
    {
        HANDLE file = CreateFileW(names[i], GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file == INVALID_HANDLE_VALUE) { printf("Writable route %u: %lu\n", i, GetLastError()); return 1; }
        CloseHandle(file);
    }
    return 0;
}

static int check_service(SC_HANDLE scm, const WCHAR *name, DWORD start)
{
    union { QUERY_SERVICE_CONFIGW config; BYTE space[4096]; } buffer;
    SC_HANDLE service = OpenServiceW(scm, name, SERVICE_QUERY_CONFIG);
    DWORD needed;
    BOOL ok;
    if (!service) return 1;
    ok = QueryServiceConfigW(service, &buffer.config, sizeof(buffer), &needed);
    CloseServiceHandle(service);
    if (!ok || buffer.config.dwStartType != start) return 1;
    return 0;
}

static int wait_for_service(SC_HANDLE scm, const WCHAR *name, BOOL start)
{
    SERVICE_STATUS_PROCESS status;
    DWORD needed, error = 0, state = 0;
    SC_HANDLE service = OpenServiceW(scm, name, SERVICE_QUERY_STATUS | (start ? SERVICE_START : 0));

    if (!service) { wprintf(L"OpenServiceW(%ls): %lu\n", name, GetLastError()); return 1; }
    if (start && !StartServiceW(service, 0, NULL)) {
        error = GetLastError();
        if (error == ERROR_SERVICE_ALREADY_RUNNING) error = 0;
    }
    for (unsigned i = 0; !error && i < 50; ++i) {
        if (!QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, (BYTE *)&status, sizeof(status), &needed)) {
            error = GetLastError();
            break;
        }
        state = status.dwCurrentState;
        if (state == SERVICE_RUNNING) break;
        Sleep(100);
    }
    CloseServiceHandle(service);
    wprintf(L"SCM %ls: state=%lu error=%lu\n", name, state, error);
    return error || state != SERVICE_RUNNING;
}

static int services(void)
{
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    int bad;
    if (!scm) { printf("OpenSCManagerW: %lu\n", GetLastError()); return 1; }
    bad = check_service(scm, L"RpcSs", SERVICE_DEMAND_START) |
          check_service(scm, L"PlugPlay", SERVICE_AUTO_START);
    if (bad) puts("Unexpected RpcSs or PlugPlay configuration");
    bad |= wait_for_service(scm, L"PlugPlay", FALSE);
    bad |= wait_for_service(scm, L"RpcSs", TRUE);
    CloseServiceHandle(scm);
    return bad;
}

static int rpcss_com(void)
{
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    SC_HANDLE rpc;
    SERVICE_STATUS_PROCESS before, after;
    DWORD needed, module_length, disposition;
    IUnknown *object = NULL;
    HRESULT hr;
    WCHAR module[MAX_PATH], command[MAX_PATH + 32];
    HKEY key = NULL, server_key = NULL;
    const WCHAR *clsid_path = L"Software\\Classes\\CLSID\\{6F25D812-9374-4C86-9122-B451D1AC7039}";
    int bad = 0, attempted = 0, initialized = 0;
    if (!scm) { printf("OpenSCManagerW: %lu\n", GetLastError()); return 1; }
    rpc = OpenServiceW(scm, L"RpcSs", SERVICE_QUERY_STATUS);
    if (!rpc || !QueryServiceStatusEx(rpc, SC_STATUS_PROCESS_INFO, (BYTE *)&before,
                                      sizeof(before), &needed)) {
        printf("Cannot observe RpcSs before COM activation: %lu\n", GetLastError());
        if (rpc) CloseServiceHandle(rpc);
        CloseServiceHandle(scm);
        return 1;
    }
    if (before.dwCurrentState != SERVICE_STOPPED) {
        printf("RpcSs COM attribution unavailable: service already in state %lu\n", before.dwCurrentState);
        CloseServiceHandle(rpc);
        CloseServiceHandle(scm);
        return 1;
    }
    module_length = GetModuleFileNameW(NULL, module, MAX_PATH);
    if (!module_length || module_length >= MAX_PATH ||
        swprintf(command, sizeof(command) / sizeof(command[0]), L"\"%ls\" com-server", module) < 0 ||
        RegCreateKeyExW(HKEY_LOCAL_MACHINE, clsid_path, 0, NULL, 0, KEY_WRITE, NULL, &key, &disposition) != ERROR_SUCCESS ||
        disposition != REG_CREATED_NEW_KEY) {
        puts("Could not create a new ephemeral COM fixture registration");
        bad = 1;
    } else {
        attempted = 1;
        if (RegCreateKeyExW(key, L"LocalServer32", 0, NULL, 0, KEY_WRITE, NULL, &server_key, NULL) != ERROR_SUCCESS ||
            RegSetValueExW(server_key, NULL, 0, REG_SZ, (const BYTE *)command,
                           (lstrlenW(command) + 1) * sizeof(WCHAR)) != ERROR_SUCCESS) {
            puts("Could not register ephemeral COM fixture");
            bad = 1;
        }
    }
    if (server_key) RegCloseKey(server_key);
    if (key) RegCloseKey(key);
    if (!bad) {
        hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
        if (FAILED(hr)) { printf("CoInitializeEx failed: 0x%08lx\n", (unsigned long)hr); bad = 1; }
        else {
            initialized = 1;
            if (!QueryServiceStatusEx(rpc, SC_STATUS_PROCESS_INFO, (BYTE *)&before,
                                      sizeof(before), &needed)) {
                printf("Cannot observe RpcSs immediately before CoCreateInstance: %lu\n", GetLastError());
                bad = 1;
            } else if (before.dwCurrentState != SERVICE_STOPPED) {
                printf("RpcSs state after CoInitializeEx: %lu (activation attribution unavailable)\n",
                       before.dwCurrentState);
                bad = 1;
            } else {
                hr = CoCreateInstance(&fixture_clsid, NULL, CLSCTX_LOCAL_SERVER,
                                      &IID_IUnknown, (void **)&object);
                if (SUCCEEDED(hr)) object->lpVtbl->Release(object);
                else { printf("Synthetic COM activation failed: 0x%08lx\n", (unsigned long)hr); bad = 1; }
            }
        }
    }
    if (!QueryServiceStatusEx(rpc, SC_STATUS_PROCESS_INFO, (BYTE *)&after, sizeof(after), &needed)) {
        printf("Cannot observe RpcSs after COM activation: %lu\n", GetLastError());
        bad = 1;
    } else if (after.dwCurrentState != SERVICE_RUNNING || before.dwCurrentState == after.dwCurrentState) {
        printf("COM activation did not establish RpcSs running (before=%lu after=%lu)\n",
               before.dwCurrentState, after.dwCurrentState);
        bad = 1;
    } else if (!bad) puts("COM activation transitioned RpcSs from stopped to running");
    if (initialized) CoUninitialize();
    if (attempted && RegDeleteTreeW(HKEY_LOCAL_MACHINE, clsid_path) != ERROR_SUCCESS) {
        puts("Could not remove ephemeral COM fixture registration");
        bad = 1;
    }
    CloseServiceHandle(rpc);
    CloseServiceHandle(scm);
    return bad;
}

typedef struct { IClassFactory iface; LONG refs; } fixture_factory;
typedef struct { IUnknown iface; LONG refs; } fixture_object;

static HRESULT STDMETHODCALLTYPE object_query(IUnknown *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualIID(iid, &IID_IUnknown)) return E_NOINTERFACE;
    *out = iface;
    iface->lpVtbl->AddRef(iface);
    return S_OK;
}
static ULONG STDMETHODCALLTYPE object_addref(IUnknown *iface) { return InterlockedIncrement(&((fixture_object *)iface)->refs); }
static ULONG STDMETHODCALLTYPE object_release(IUnknown *iface)
{
    LONG refs = InterlockedDecrement(&((fixture_object *)iface)->refs);
    if (!refs) HeapFree(GetProcessHeap(), 0, iface);
    return refs;
}
static const IUnknownVtbl object_vtbl = { object_query, object_addref, object_release };
static HRESULT STDMETHODCALLTYPE factory_query(IClassFactory *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualIID(iid, &IID_IUnknown) && !IsEqualIID(iid, &IID_IClassFactory)) return E_NOINTERFACE;
    *out = iface;
    iface->lpVtbl->AddRef(iface);
    return S_OK;
}
static ULONG STDMETHODCALLTYPE factory_addref(IClassFactory *iface) { return InterlockedIncrement(&((fixture_factory *)iface)->refs); }
static ULONG STDMETHODCALLTYPE factory_release(IClassFactory *iface) { return InterlockedDecrement(&((fixture_factory *)iface)->refs); }
static HRESULT STDMETHODCALLTYPE factory_create(IClassFactory *iface, IUnknown *outer, REFIID iid, void **out)
{
    fixture_object *object;
    HRESULT hr;
    (void)iface;
    if (outer) return CLASS_E_NOAGGREGATION;
    object = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*object));
    if (!object) return E_OUTOFMEMORY;
    object->iface.lpVtbl = &object_vtbl;
    object->refs = 1;
    hr = object_query(&object->iface, iid, out);
    object_release(&object->iface);
    return hr;
}
static HRESULT STDMETHODCALLTYPE factory_lock(IClassFactory *iface, BOOL lock) { (void)iface; (void)lock; return S_OK; }
static const IClassFactoryVtbl factory_vtbl = { factory_query, factory_addref, factory_release, factory_create, factory_lock };
static int com_server(void)
{
    fixture_factory factory = {{&factory_vtbl}, 1};
    DWORD cookie;
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        printf("COM fixture server initialization failed: 0x%08lx\n", (unsigned long)hr);
        return 1;
    }
    hr = CoRegisterClassObject(&fixture_clsid, (IUnknown *)&factory, CLSCTX_LOCAL_SERVER,
                               REGCLS_MULTIPLEUSE, &cookie);
    if (SUCCEEDED(hr)) {
        MSG message;
        DWORD idle = GetTickCount();
        for (;;) {
            while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
                if (message.message == WM_QUIT) goto done;
                DispatchMessageW(&message);
                idle = GetTickCount();
            }
            if (GetTickCount() - idle >= 10000) break;
            MsgWaitForMultipleObjectsEx(0, NULL, 1000, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        }
done:
        CoRevokeClassObject(cookie);
    } else printf("COM fixture class registration failed: 0x%08lx\n", (unsigned long)hr);
    CoUninitialize();
    return FAILED(hr);
}

int main(int argc, char **argv)
{
    const char *key = "Software\\CompatibilityOS\\ViewProbe";
    DWORD bits = sizeof(void *) * 8, value = 0, size = sizeof(value);
    HKEY handle;
    HANDLE file;

    if (argc > 1 && !strcmp(argv[1], "hold")) { Sleep(8000); return 0; }
    if (argc > 1 && !strcmp(argv[1], "known-folders")) return known_folders();
    if (argc > 1 && !strcmp(argv[1], "writable-windows")) return writable_windows();
    if (argc > 1 && !strcmp(argv[1], "services")) return services();
    if (argc > 1 && !strcmp(argv[1], "rpcss-com")) return rpcss_com();
    if (argc > 1 && !strcmp(argv[1], "com-server")) return com_server();
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
