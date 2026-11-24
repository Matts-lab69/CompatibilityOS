/* SPDX-License-Identifier: MIT */
#include <windows.h>
#include <winspool.h>
#include <stdio.h>

static const BYTE job_data[] =
    "CPOS spool fixture\r\n"
    "RAW printer transport test\r\n"
    "0123456789abcdef\r\n";

int wmain(int argc, wchar_t **argv)
{
    const wchar_t *queue_name = argc > 1 ? argv[1] : L"CPOSFixture";
    HANDLE printer = NULL;
    DOC_INFO_1W document = { L"CPOS spool probe", NULL, L"RAW" };
    DWORD job_id, written = 0;
    BOOL document_started = FALSE;
    int result = 1;

    if (argc > 2) {
        fwprintf(stderr, L"Usage: spool-probe.exe [printer-name]\n");
        return 2;
    }
    if (!OpenPrinterW((LPWSTR)queue_name, &printer, NULL)) {
        fwprintf(stderr, L"OpenPrinterW failed: %lu\n", GetLastError());
        goto done;
    }
    job_id = StartDocPrinterW(printer, 1, (LPBYTE)&document);
    if (!job_id) {
        fwprintf(stderr, L"StartDocPrinterW failed: %lu\n", GetLastError());
        goto done;
    }
    document_started = TRUE;
    if (!WritePrinter(printer, (LPVOID)job_data, (DWORD)(sizeof(job_data) - 1), &written) ||
        written != sizeof(job_data) - 1) {
        fwprintf(stderr, L"WritePrinter failed or wrote %lu of %lu bytes (error %lu)\n",
                 written, (DWORD)(sizeof(job_data) - 1), GetLastError());
        goto done;
    }
    if (!EndDocPrinter(printer)) {
        document_started = FALSE;
        fwprintf(stderr, L"EndDocPrinter failed: %lu\n", GetLastError());
        goto done;
    }
    document_started = FALSE;
    puts("Spool probe passed");
    result = 0;

done:
    if (document_started && !EndDocPrinter(printer))
        fwprintf(stderr, L"EndDocPrinter cleanup failed: %lu\n", GetLastError());
    if (printer && !ClosePrinter(printer)) {
        fwprintf(stderr, L"ClosePrinter failed: %lu\n", GetLastError());
        result = 1;
    }
    return result;
}
