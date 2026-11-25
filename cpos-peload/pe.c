/* SPDX-License-Identifier: MIT */
#include "pe.h"

#include <stdlib.h>
#include <string.h>

#define MAX_IMAGE_SIZE (128u * 1024u * 1024u)
#define DIR_COUNT 16u
#define DIR_RELOC 5u
#define DIR_IMPORT 1u

static int range(size_t total, uint64_t offset, uint64_t length)
{
    return offset <= total && length <= (uint64_t)total - offset;
}

static uint16_t u16(const uint8_t *p) { return (uint16_t)p[0] | (uint16_t)p[1] << 8; }
static uint32_t u32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint64_t u64(const uint8_t *p) { return (uint64_t)u32(p) | (uint64_t)u32(p + 4) << 32; }

static int rva_file(const uint8_t *data, size_t size, const struct pe_image *im,
                    uint32_t rva, uint32_t length, size_t *offset)
{
    if (rva < im->headers_size && range(im->headers_size, rva, length) && range(size, rva, length)) { *offset = rva; return 1; }
    for (uint16_t i = 0; i < im->section_count; ++i) {
        const struct pe_section *s = &im->sections[i];
        uint32_t span = s->virtual_size > s->raw_size ? s->virtual_size : s->raw_size;
        if (rva >= s->rva && (uint64_t)rva + length <= (uint64_t)s->rva + span &&
            (uint64_t)rva + length <= (uint64_t)s->rva + s->raw_size) {
            uint64_t off = (uint64_t)s->raw_offset + rva - s->rva;
            if (range(size, off, length)) { *offset = (size_t)off; return 1; }
        }
    }
    (void)data;
    return 0;
}

static int validate_imports(const uint8_t *d, size_t n, const struct pe_image *im,
                            uint32_t rva, uint32_t size)
{
    if (!rva && !size) return 1;
    if (!rva || size < 20 || size > MAX_IMAGE_SIZE || (uint64_t)rva + size > im->image_size) return 0;
    uint32_t budget = 65536;
    for (uint32_t pos = 0; pos + 20 <= size; pos += 20) {
        size_t off;
        if (!rva_file(d, n, im, rva + pos, 20, &off)) return 0;
        uint32_t oft = u32(d + off), name = u32(d + off + 12), ft = u32(d + off + 16);
        if (!oft && !name && !ft) return !u32(d + off + 4) && !u32(d + off + 8);
        size_t no;
        if (!name || !ft || !rva_file(d, n, im, name, 1, &no)) return 0;
        int terminated = 0;
        for (uint32_t j = 1; j < 256 && rva_file(d, n, im, name, j + 1, &no); ++j) if (d[no] && !d[no + j]) { terminated = 1; break; }
        if (!terminated) return 0;
        uint32_t thunk = oft ? oft : ft;
        for (uint32_t j = 0; j < 65536; ++j) {
            size_t to;
            uint64_t thunk_rva = (uint64_t)thunk + (uint64_t)j * 8;
            if (!budget-- || thunk_rva > UINT32_MAX || !rva_file(d, n, im, (uint32_t)thunk_rva, 8, &to) ||
                (uint64_t)ft + (uint64_t)j * 8 + 8 > im->image_size) return 0;
            uint64_t value = u64(d + to);
            if (!value) break;
            if (!(value & (UINT64_C(1) << 63))) {
                size_t hn;
                if (value > UINT32_MAX || !rva_file(d, n, im, (uint32_t)value, 3, &hn)) return 0;
                int end = 0;
                for (uint32_t k = 3; k < 258 && rva_file(d, n, im, (uint32_t)value, k + 1, &hn); ++k) if (d[hn + 2] && !d[hn + k]) { end = 1; break; }
                if (!end) return 0;
            } else if (value & UINT64_C(0x7fffffffffff0000)) return 0;
            if (j == 65535) return 0;
        }
    }
    return 0;
}

static int validate_relocs(const uint8_t *d, size_t n, const struct pe_image *im,
                           uint32_t rva, uint32_t size)
{
    if (!rva && !size) return 1;
    if (!rva || size < 8 || size > MAX_IMAGE_SIZE || (uint64_t)rva + size > im->image_size) return 0;
    uint32_t pos = 0;
    while (pos < size) {
        size_t off;
        if (size - pos < 8 || !rva_file(d, n, im, rva + pos, 8, &off)) return 0;
        uint32_t page = u32(d + off), block = u32(d + off + 4);
        if (block < 8 || (block & 1) || block > size - pos || page >= im->image_size) return 0;
        size_t bo;
        if (!rva_file(d, n, im, rva + pos, block, &bo)) return 0;
        for (uint32_t x = 8; x < block; x += 2) {
            uint16_t entry = u16(d + bo + x), type = entry >> 12;
            uint64_t target = (uint64_t)page + (entry & 0xfff);
            if (type != 0 && type != 10) return 0;
            if (type == 10 && target + 8 > im->image_size) return 0;
        }
        pos += block;
    }
    return pos == size;
}

enum pe_status pe_parse(const uint8_t *d, size_t n, struct pe_image *out)
{
    if (!d || !out || n < 64 || d[0] != 'M' || d[1] != 'Z') return PE_INVALID;
    uint32_t pe = u32(d + 0x3c);
    if (!range(n, pe, 24) || memcmp(d + pe, "PE\0\0", 4)) return PE_INVALID;
    const uint8_t *coff = d + pe + 4;
    uint16_t count = u16(coff + 2), opt_size = u16(coff + 16);
    if (u16(coff) != 0x8664 || count == 0 || count > 96 || opt_size < 112 ||
        !range(n, (uint64_t)pe + 24, opt_size)) return PE_UNSUPPORTED;
    const uint8_t *o = coff + 20;
    if (u16(o) != 0x20b || opt_size < 112) return PE_UNSUPPORTED;
    struct pe_image im = {0};
    im.is_dll = (u16(coff + 18) & 0x2000u) != 0;
    im.entry_rva = u32(o + 16); im.image_base = u64(o + 24);
    uint32_t section_align = u32(o + 32), file_align = u32(o + 36);
    im.image_size = u32(o + 56); im.headers_size = u32(o + 60);
    uint32_t dirs = u32(o + 108);
    if (!im.image_size || im.image_size > MAX_IMAGE_SIZE || !im.headers_size ||
        im.headers_size > n || im.headers_size > im.image_size || !section_align || !file_align ||
        dirs > (opt_size - 112u) / 8u || dirs > DIR_COUNT) return PE_INVALID;
    uint64_t sht = (uint64_t)pe + 24 + opt_size;
    if (!range(im.headers_size, sht, (uint64_t)count * 40)) return PE_INVALID;
    im.section_count = count;
    uint64_t previous_end = im.headers_size;
    for (uint16_t i = 0; i < count; ++i) {
        const uint8_t *s = d + sht + (uint64_t)i * 40;
        struct pe_section *v = &im.sections[i];
        v->virtual_size = u32(s + 8); v->rva = u32(s + 12);
        v->raw_size = u32(s + 16); v->raw_offset = u32(s + 20);
        uint32_t span = v->virtual_size > v->raw_size ? v->virtual_size : v->raw_size;
        if ((v->rva % section_align) || v->rva < im.headers_size ||
            (uint64_t)v->rva + span > im.image_size ||
            (v->raw_size && (!range(n, v->raw_offset, v->raw_size) || v->raw_offset < im.headers_size))) return PE_INVALID;
        for (uint16_t j = 0; j < i; ++j) {
            const struct pe_section *q = &im.sections[j];
            uint32_t qspan = q->virtual_size > q->raw_size ? q->virtual_size : q->raw_size;
            if ((uint64_t)v->rva < (uint64_t)q->rva + qspan && (uint64_t)q->rva < (uint64_t)v->rva + span) return PE_INVALID;
            if (v->raw_size && q->raw_size && (uint64_t)v->raw_offset < (uint64_t)q->raw_offset + q->raw_size && (uint64_t)q->raw_offset < (uint64_t)v->raw_offset + v->raw_size) return PE_INVALID;
        }
        if (v->rva < previous_end) return PE_INVALID;
        previous_end = (uint64_t)v->rva + span;
    }
    if (im.entry_rva >= im.image_size) return PE_INVALID;
    const uint8_t *directory = o + 112;
    if (dirs > DIR_IMPORT && !validate_imports(d, n, &im, u32(directory + DIR_IMPORT * 8), u32(directory + DIR_IMPORT * 8 + 4))) return PE_INVALID;
    if (dirs > DIR_RELOC && !validate_relocs(d, n, &im, u32(directory + DIR_RELOC * 8), u32(directory + DIR_RELOC * 8 + 4))) return PE_INVALID;
    *out = im;
    return PE_OK;
}

enum pe_status pe_stage(const uint8_t *d, size_t n, const struct pe_image *im,
                       uint8_t **staging, size_t *staging_size)
{
    if (!d || !im || !staging || !staging_size || !range(n, 0, im->headers_size) ||
        im->headers_size > im->image_size || !im->image_size || im->image_size > MAX_IMAGE_SIZE ||
        im->section_count > sizeof(im->sections) / sizeof(im->sections[0])) return PE_INVALID;
    if (im->entry_rva >= im->image_size)
        return PE_INVALID;
    for (uint16_t i = 0; i < im->section_count; ++i) {
        const struct pe_section *s = &im->sections[i];
        uint32_t span = s->virtual_size > s->raw_size ? s->virtual_size : s->raw_size;
        if (s->rva < im->headers_size ||
            (uint64_t)s->rva + span > im->image_size ||
            !range(n, s->raw_offset, s->raw_size) ||
            !range(im->image_size, s->rva, s->raw_size)) return PE_INVALID;
        for (uint16_t j = 0; j < i; ++j) {
            const struct pe_section *p = &im->sections[j];
            uint32_t pspan = p->virtual_size > p->raw_size ? p->virtual_size : p->raw_size;
            if ((uint64_t)s->rva < (uint64_t)p->rva + pspan &&
                (uint64_t)p->rva < (uint64_t)s->rva + span) return PE_INVALID;
        }
    }
    uint8_t *mem = calloc(1, im->image_size);
    if (!mem) return PE_NOMEM;
    memcpy(mem, d, im->headers_size);
    for (uint16_t i = 0; i < im->section_count; ++i) {
        const struct pe_section *s = &im->sections[i];
        memcpy(mem + s->rva, d + s->raw_offset, s->raw_size);
    }
    *staging = mem; *staging_size = im->image_size;
    return PE_OK;
}

void pe_stage_free(uint8_t *staging, size_t size) { (void)size; free(staging); }
