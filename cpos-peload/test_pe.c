/* SPDX-License-Identifier: MIT */
#define _GNU_SOURCE
#include "pe.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(CPOS_TEST_EXEC) && defined(__linux__) && defined(__x86_64__)
#include <asm/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

static void w16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void w32(uint8_t *p, uint32_t v) { for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i)); }
static void w64(uint8_t *p, uint64_t v) { w32(p, (uint32_t)v); w32(p + 4, (uint32_t)(v >> 32)); }

#ifdef CPOS_TEST_EXEC
static int read_fixture(const char *path, uint8_t *buffer, size_t capacity, size_t *length)
{
    FILE *in = fopen(path, "rb");
    if (!in) return 0;
    *length = fread(buffer, 1, capacity, in);
    int failed = ferror(in) || !feof(in);
    return fclose(in) == 0 && !failed;
}
#endif

static void make_pe(uint8_t *b)
{
    memset(b, 0, 1024); b[0] = 'M'; b[1] = 'Z'; w32(b + 0x3c, 0x80);
    memcpy(b + 0x80, "PE\0\0", 4); uint8_t *c = b + 0x84;
    w16(c, 0x8664); w16(c + 2, 1); w16(c + 16, 240);
    uint8_t *o = c + 20; w16(o, 0x20b); w32(o + 16, 0x1000);
    w64(o + 24, 0x140000000); w32(o + 32, 0x1000); w32(o + 36, 0x200);
    w32(o + 56, 0x2000); w32(o + 60, 0x200); w32(o + 108, 16);
    uint8_t *s = o + 240; memcpy(s, ".text", 5); w32(s + 8, 0x200); w32(s + 12, 0x1000);
    w32(s + 16, 0x200); w32(s + 20, 0x200); b[0x200] = 0xc3;
}

static void make_tables(uint8_t *bytes)
{
    uint8_t *dirs = bytes + 0x80 + 24 + 112;
    uint8_t *raw = bytes + 0x200;
    w32(dirs + 8, 0x1020); w32(dirs + 12, 40);
    w32(raw + 0x20, 0x1080); w32(raw + 0x2c, 0x1070); w32(raw + 0x30, 0x1090);
    memcpy(raw + 0x70, "fixture.dll", sizeof("fixture.dll"));
    w64(raw + 0x80, 0x10a0);
    memcpy(raw + 0xa2, "Fixture", sizeof("Fixture"));
    w32(dirs + 40, 0x10c0); w32(dirs + 44, 12);
    w32(raw + 0xc0, 0x1000); w32(raw + 0xc4, 12); w16(raw + 0xc8, 0xa010);
}

#if defined(CPOS_TEST_EXEC) && defined(__linux__) && defined(__x86_64__)
static void __attribute__((ms_abi)) fixture_hello(void)
{
    puts("CPOS PE hello");
}

static unsigned int fixture_lifecycle_step;

static unsigned int __attribute__((ms_abi)) fixture_host_ready(unsigned int step)
{
    assert(step == ++fixture_lifecycle_step);
    return 0x29U;
}
#endif

int main(int argc, char **argv)
{
    uint8_t bytes[1024]; struct pe_image im; make_pe(bytes);
    make_tables(bytes);
    assert(pe_parse(bytes, sizeof(bytes), &im) == PE_OK);
#ifdef CPOS_TEST_EXEC
    if (argc == 5 && (!strcmp(argv[1], "--execute-dll-fixture") ||
                      !strcmp(argv[1], "--execute-delay-fixture"))) {
        uint8_t exe[65536], api[65536], dep[65536];
        size_t exe_size, api_size, dep_size;
        if (!read_fixture(argv[2], exe, sizeof(exe), &exe_size) ||
            !read_fixture(argv[3], api, sizeof(api), &api_size) ||
            !read_fixture(argv[4], dep, sizeof(dep), &dep_size)) return 1;
        const struct pe_fixture_dll dlls[] = {
            {"cposapi.dll", api, api_size}, {"cposdep.dll", dep, dep_size}
        };
        const struct pe_import host_import = {
            "cposhost.dll", "CposFixtureHostReady", (void *)fixture_host_ready
        };
        const struct pe_exec_options dll_options = {&host_import, 1, 1, dlls, 2};
        enum pe_status dll_status = pe_execute_fixture(exe, exe_size, &dll_options);
        if (dll_status != PE_OK) fprintf(stderr, "MinGW DLL fixture rejected: %d\n", dll_status);
        if (dll_status == PE_OK && fixture_lifecycle_step != 4) {
            fputs("Missing DLL lifecycle callback\n", stderr);
            return 1;
        }
        return dll_status != PE_OK;
    }
    if (argc == 3 && (!strcmp(argv[1], "--execute-fixture") ||
                      !strcmp(argv[1], "--execute-import-fixture"))) {
        uint8_t fixture[65536];
        size_t n;
        if (!read_fixture(argv[2], fixture, sizeof(fixture), &n)) return 1;
        const int import_fixture = !strcmp(argv[1], "--execute-import-fixture");
#if defined(__linux__) && defined(__x86_64__)
        struct pe_import allowed = {"cposhello.dll", "CposPeHello", (void *)fixture_hello};
#else
        struct pe_import allowed = {0};
#endif
        const struct pe_exec_options options = {import_fixture ? &allowed : NULL,
                                                import_fixture ? 1 : 0, 1, NULL, 0};
#if defined(__linux__) && defined(__x86_64__)
        uintptr_t original_gs = 0, restored_gs = 0;
        if (syscall(SYS_arch_prctl, ARCH_GET_GS, &original_gs) != 0) return 1;
#endif
        enum pe_status status = pe_execute_fixture(fixture, n, &options);
#if defined(__linux__) && defined(__x86_64__)
        if (syscall(SYS_arch_prctl, ARCH_GET_GS, &restored_gs) != 0 ||
            restored_gs != original_gs) return 1;
#endif
        if (import_fixture) {
            if (status != PE_OK) fprintf(stderr, "MinGW import fixture rejected: %d\n", status);
            return status != PE_OK;
        }
        printf("MinGW fixture entry: %d\n", status);
        return status != PE_OK;
    }
    void (*fixture_import)(void) = (void (*)(void))main;
    struct pe_import allowed = { "fixture.dll", "Fixture", (void *)fixture_import };
    struct pe_exec_options exec_options = { &allowed, 1, 0, NULL, 0 };
    assert(pe_execute_fixture(bytes, sizeof(bytes), &exec_options) == PE_OK);
#endif
    if (argc == 2) {
        FILE *out = fopen(argv[1], "wb");
        if (!out) return 1;
        const int written = fwrite(bytes, sizeof(bytes), 1, out) == 1;
        return fclose(out) != 0 || !written;
    }
    uint8_t *stage = NULL; size_t length = 0;
    assert(pe_stage(bytes, sizeof(bytes), &im, &stage, &length) == PE_OK);
    assert(length == 0x2000 && stage[0x1000] == 0xc3);
    pe_stage_free(stage, length);
    struct pe_image forged = im;
    forged.sections[0].rva = UINT32_MAX - 15;
    assert(pe_stage(bytes, sizeof(bytes), &forged, &stage, &length) == PE_INVALID);
    forged = im;
    forged.sections[0].raw_offset = UINT32_MAX - 15;
    assert(pe_stage(bytes, sizeof(bytes), &forged, &stage, &length) == PE_INVALID);
    bytes[0] = 0; assert(pe_parse(bytes, sizeof(bytes), &im) == PE_INVALID);
    make_pe(bytes); w32(bytes + 0x3c, UINT32_MAX);
    assert(pe_parse(bytes, sizeof(bytes), &im) != PE_OK);
    make_pe(bytes); w32(bytes + 0x80 + 24 + 56, UINT32_MAX);
    assert(pe_parse(bytes, sizeof(bytes), &im) != PE_OK);
    make_pe(bytes); make_tables(bytes); w32(bytes + 0x2c4, 7);
    assert(pe_parse(bytes, sizeof(bytes), &im) == PE_INVALID);
    make_pe(bytes); make_tables(bytes); w32(bytes + 0x22c, 0x2000);
    assert(pe_parse(bytes, sizeof(bytes), &im) == PE_INVALID);
    make_pe(bytes);
    assert(pe_parse(bytes, sizeof(bytes), &im) == PE_OK && !im.is_dll);
    bytes[0x80 + 23] |= 0x20;
    assert(pe_parse(bytes, sizeof(bytes), &im) == PE_OK && im.is_dll);
#ifdef CPOS_TEST_EXEC
    assert(pe_execute_fixture(bytes, sizeof(bytes), &exec_options) == PE_INVALID);
#endif
    puts("PE unit checks passed");
    return 0;
}
