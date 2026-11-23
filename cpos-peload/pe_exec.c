/* SPDX-License-Identifier: MIT */
#define _GNU_SOURCE
#include "pe.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/personality.h>
#include <unistd.h>

static uint16_t r16(const uint8_t *p) { return (uint16_t)p[0] | (uint16_t)p[1] << 8; }
static uint32_t r32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint64_t r64(const uint8_t *p) { return (uint64_t)r32(p) | (uint64_t)r32(p + 4) << 32; }
static void w64(uint8_t *p, uint64_t v) { for (unsigned i = 0; i < 8; ++i) p[i] = (uint8_t)(v >> (i * 8)); }

static int resolve_import(const struct pe_exec_options *o, const char *module,
                          const char *symbol, void **address)
{
    for (size_t i = 0; i < o->import_count; ++i) {
        const struct pe_import *entry = &o->imports[i];
        if (entry->module && entry->symbol && entry->address &&
            strcmp(entry->module, module) == 0 && strcmp(entry->symbol, symbol) == 0) {
            *address = entry->address;
            return 1;
        }
    }
    return 0;
}

static int bind_imports(uint8_t *image, const uint8_t *metadata, uint32_t rva,
                        const struct pe_exec_options *o)
{
    if (!rva) return 1;
    for (uint32_t pos = 0; pos < 65536; pos += 20) {
        const uint8_t *desc = metadata + rva + pos;
        uint32_t names = r32(desc), name_rva = r32(desc + 12), iat = r32(desc + 16);
        if (!names && !name_rva && !iat) return 1;
        if (!name_rva || !iat) return 0;
        const char *module = (const char *)(metadata + name_rva);
        uint32_t lookup = names ? names : iat;
        for (uint32_t i = 0; i < 65536; ++i) {
            uint64_t value = r64(metadata + lookup + (uint64_t)i * 8);
            if (!value) break;
            if (value >> 63) return 0;
            const char *symbol = (const char *)(metadata + (uint32_t)value + 2);
            void *address = NULL;
            if (!resolve_import(o, module, symbol, &address)) return 0;
            w64(image + iat + (uint64_t)i * 8, (uintptr_t)address);
            if (i == 65535) return 0;
        }
    }
    return 0;
}

enum pe_status pe_execute_fixture(const uint8_t *data, size_t size,
                                  const struct pe_exec_options *options)
{
#if !defined(__x86_64__) || !defined(__linux__)
    (void)data; (void)size; (void)options;
    return PE_UNSUPPORTED;
#else
    struct pe_image parsed;
    int current_personality = personality(0xffffffffUL);
    if (current_personality < 0 || (current_personality & READ_IMPLIES_EXEC)) return PE_UNSUPPORTED;
    if (!options || (!options->imports && options->import_count) ||
        pe_parse(data, size, &parsed) != PE_OK) return PE_INVALID;
    uint8_t *file_image = NULL;
    size_t image_size = 0;
    if (pe_stage(data, size, &parsed, &file_image, &image_size) != PE_OK) return PE_INVALID;
    long page = sysconf(_SC_PAGESIZE);
    if (page <= 0) { pe_stage_free(file_image, image_size); return PE_UNSUPPORTED; }
    void *preferred = (void *)(uintptr_t)parsed.image_base;
    uint8_t *image = MAP_FAILED;
    if (preferred && !(parsed.image_base % (unsigned long)page)) {
        image = mmap(preferred, image_size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        if (image != MAP_FAILED && image != preferred) {
            munmap(image, image_size);
            image = MAP_FAILED;
        }
    }
    if (image == MAP_FAILED)
        image = mmap(NULL, image_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (image == MAP_FAILED) { pe_stage_free(file_image, image_size); return PE_NOMEM; }
    memcpy(image, file_image, image_size);

    const uint8_t *opt = data + r32(data + 0x3c) + 24;
    uint32_t dir_count = r32(opt + 108);
    uint32_t reloc_rva = dir_count > 5 ? r32(opt + 112 + 40) : 0;
    uint32_t reloc_size = dir_count > 5 ? r32(opt + 112 + 44) : 0;
    uint32_t import_rva = dir_count > 1 ? r32(opt + 112 + 8) : 0;
    uint32_t tls_rva = dir_count > 9 ? r32(opt + 112 + 72) : 0;
    if (tls_rva) goto invalid;
    intptr_t delta = (intptr_t)((uintptr_t)image - (uintptr_t)parsed.image_base);
    if (delta) {
        if (!reloc_rva || !reloc_size) goto invalid;
        for (uint32_t pos = 0; pos < reloc_size;) {
            const uint8_t *block = file_image + reloc_rva + pos;
            uint32_t base = r32(block), block_size = r32(block + 4);
            if (block_size < 8 || block_size > reloc_size - pos) goto invalid;
            for (uint32_t off = 8; off < block_size; off += 2) {
                uint16_t entry = r16(block + off);
                if ((entry >> 12) == 10) {
                    uint8_t *target = image + base + (entry & 0xfff);
                    w64(target, r64(target) + (uint64_t)delta);
                }
            }
            pos += block_size;
        }
    }
    if (!bind_imports(image, file_image, import_rva, options)) goto invalid;
    if (mprotect(image, image_size, PROT_READ) != 0) goto unsupported;
    int executable_entry = 0;
    for (uint16_t i = 0; i < parsed.section_count; ++i) {
        const uint8_t *section = opt + r16(opt - 4) + (size_t)i * 40;
        uint32_t flags = r32(section + 36), start = parsed.sections[i].rva;
        uint64_t span = parsed.sections[i].virtual_size > parsed.sections[i].raw_size ? parsed.sections[i].virtual_size : parsed.sections[i].raw_size;
        uintptr_t lo = (uintptr_t)image + start;
        uintptr_t hi = ((uintptr_t)image + start + span + (uintptr_t)page - 1) & ~((uintptr_t)page - 1);
        int prot = PROT_READ;
        if (flags & 0x80000000u) prot |= PROT_WRITE;
        if (flags & 0x20000000u) prot |= PROT_EXEC;
        if (!span) continue;
        if (start % (unsigned long)page) goto unsupported;
        if ((prot & PROT_EXEC) && parsed.entry_rva >= start && parsed.entry_rva - start < span)
            executable_entry = 1;
        if ((prot & (PROT_WRITE | PROT_EXEC)) == (PROT_WRITE | PROT_EXEC) ||
            mprotect((void *)lo, hi - lo, prot) != 0) goto unsupported;
    }
    if (options->invoke_entry) {
        if (!executable_entry) goto unsupported;
        ((void (__attribute__((ms_abi)) *)(void))(image + parsed.entry_rva))();
    }
    munmap(image, image_size);
    pe_stage_free(file_image, image_size);
    return PE_OK;
invalid:
unsupported:
    munmap(image, image_size);
    pe_stage_free(file_image, image_size);
    return PE_UNSUPPORTED;
#endif
}
