/* SPDX-License-Identifier: MIT */
#include "pe.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void w16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void w32(uint8_t *p, uint32_t v) { for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i)); }
static void w64(uint8_t *p, uint64_t v) { w32(p, (uint32_t)v); w32(p + 4, (uint32_t)(v >> 32)); }

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

int main(int argc, char **argv)
{
    uint8_t bytes[1024]; struct pe_image im; make_pe(bytes);
    make_tables(bytes);
    assert(pe_parse(bytes, sizeof(bytes), &im) == PE_OK);
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
    bytes[0] = 0; assert(pe_parse(bytes, sizeof(bytes), &im) == PE_INVALID);
    make_pe(bytes); w32(bytes + 0x3c, UINT32_MAX);
    assert(pe_parse(bytes, sizeof(bytes), &im) != PE_OK);
    make_pe(bytes); w32(bytes + 0x80 + 24 + 56, UINT32_MAX);
    assert(pe_parse(bytes, sizeof(bytes), &im) != PE_OK);
    make_pe(bytes); make_tables(bytes); w32(bytes + 0x2c4, 7);
    assert(pe_parse(bytes, sizeof(bytes), &im) == PE_INVALID);
    make_pe(bytes); make_tables(bytes); w32(bytes + 0x22c, 0x2000);
    assert(pe_parse(bytes, sizeof(bytes), &im) == PE_INVALID);
    puts("PE unit checks passed");
    return 0;
}
