/* SPDX-License-Identifier: MIT */
#define _GNU_SOURCE
#include "pe.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/personality.h>
#include <sys/syscall.h>
#include <asm/prctl.h>
#include <unistd.h>

static uint16_t r16(const uint8_t *p) { return (uint16_t)p[0] | (uint16_t)p[1] << 8; }
static uint32_t r32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint64_t r64(const uint8_t *p) { return (uint64_t)r32(p) | (uint64_t)r32(p + 4) << 32; }
static void w64(uint8_t *p, uint64_t v) { for (unsigned i = 0; i < 8; ++i) p[i] = (uint8_t)(v >> (i * 8)); }

#define PE_TLS_DIRECTORY_SIZE 40u
#define PE_TLS_CALLBACK_LIMIT 64u
#define PE_DELAY_DESCRIPTOR_SIZE 32u

static int image_range(size_t image_size, uint32_t rva, size_t length)
{
    return rva <= image_size && length <= image_size - rva;
}

static int image_string(const uint8_t *image, size_t image_size, uint32_t rva,
                        const char *expected)
{
    if (!image_range(image_size, rva, 1)) return 0;
    size_t available = image_size - rva;
    size_t length = strlen(expected) + 1;
    return length <= available && memcmp(image + rva, expected, length) == 0;
}

static int delay_directory_validate(const uint8_t *image, size_t image_size,
                                    uint32_t rva, uint32_t size)
{
    if (!rva && !size) return 1;
    if (!rva || size != 2 * PE_DELAY_DESCRIPTOR_SIZE ||
        !image_range(image_size, rva, size)) return 0;
    const uint8_t *desc = image + rva;
    uint32_t attrs = r32(desc), name_rva = r32(desc + 4);
    uint32_t module_slot = r32(desc + 8), iat = r32(desc + 12);
    uint32_t lookup = r32(desc + 16);
    if (attrs != 1 || !name_rva || !module_slot || !iat || !lookup ||
        r32(desc + 20) || r32(desc + 24) || r32(desc + 28) ||
        !image_string(image, image_size, name_rva, "cposdep.dll") ||
        !image_range(image_size, module_slot, 8) ||
        !image_range(image_size, iat, 16) ||
        !image_range(image_size, lookup, 16) ||
        !image_range(image_size, rva + PE_DELAY_DESCRIPTOR_SIZE,
                     PE_DELAY_DESCRIPTOR_SIZE)) return 0;
    for (size_t i = 0; i < PE_DELAY_DESCRIPTOR_SIZE; ++i)
        if (image[rva + PE_DELAY_DESCRIPTOR_SIZE + i]) return 0;
    uint64_t lookup_name = r64(image + lookup);
    if (lookup_name != r32(image + iat) || r32(image + iat + 4) ||
        r64(image + iat + 8) || r32(image + lookup + 4) ||
        r64(image + lookup + 8) || lookup_name > UINT32_MAX - 2 ||
        !image_range(image_size, (uint32_t)lookup_name, 2) ||
        !image_string(image, image_size, (uint32_t)lookup_name + 2,
                      "CposDependencyValue")) return 0;
    return 1;
}

static int executable_rva(const uint8_t *opt, const struct pe_image *parsed,
                          uint32_t rva)
{
    const uint8_t *sections = opt + r16(opt - 4);
    for (uint16_t i = 0; i < parsed->section_count; ++i) {
        const uint8_t *header = sections + (size_t)i * 40;
        uint32_t start = parsed->sections[i].rva;
        uint32_t span = parsed->sections[i].virtual_size > parsed->sections[i].raw_size ?
                        parsed->sections[i].virtual_size : parsed->sections[i].raw_size;
        if (rva >= start && rva - start < span)
            return (r32(header + 36) & 0x20000000u) != 0;
    }
    return 0;
}

static int invoke_tls_callbacks(uint8_t *image, const uint8_t *metadata, const struct pe_image *parsed,
                                const uint8_t *opt, uint32_t tls_rva,
                                uint32_t tls_size)
{
    if (!tls_rva) return tls_size == 0;
    if (tls_size < PE_TLS_DIRECTORY_SIZE ||
        !image_range(parsed->image_size, tls_rva, tls_size)) return 0;

    const uint8_t *tls = metadata + tls_rva;
    if (r32(tls + 32)) return 0;
    uint64_t callbacks_va = r64(tls + 24);
    if (!callbacks_va) return 1;
    if (callbacks_va < parsed->image_base ||
        callbacks_va - parsed->image_base > UINT32_MAX) return 0;
    uint32_t callbacks_rva = (uint32_t)(callbacks_va - parsed->image_base);

    uint32_t callback_count = 0;
    for (uint32_t i = 0; i <= PE_TLS_CALLBACK_LIMIT; ++i) {
        uint64_t slot_rva = (uint64_t)callbacks_rva + (uint64_t)i * 8;
        if (slot_rva > UINT32_MAX || !image_range(parsed->image_size, (uint32_t)slot_rva, 8)) return 0;
        uint64_t callback_va = r64(metadata + (uint32_t)slot_rva);
        if (!callback_va) break;
        if (i == PE_TLS_CALLBACK_LIMIT || callback_va < parsed->image_base ||
            callback_va - parsed->image_base > UINT32_MAX) return 0;
        uint32_t callback_rva = (uint32_t)(callback_va - parsed->image_base);
        if (!image_range(parsed->image_size, callback_rva, 1) ||
            !executable_rva(opt, parsed, callback_rva)) return 0;
        ++callback_count;
    }
    for (uint32_t i = 0; i < callback_count; ++i) {
        uint32_t callback_rva = (uint32_t)(r64(metadata + callbacks_rva + (uint64_t)i * 8) - parsed->image_base);
        ((void (__attribute__((ms_abi)) *)(void *, uint32_t, void *))
         (image + callback_rva))(image, 1, NULL);
    }
    return 1;
}

struct fixture_tls {
    uint8_t teb[0x80];
    uint8_t peb[0x20];
    uintptr_t slots[1];
    uint8_t *block;
};

static int setup_tls(uint8_t *image, const uint8_t *metadata,
                     const struct pe_image *parsed, uint32_t tls_rva,
                     uint32_t tls_size, struct fixture_tls *tls)
{
    memset(tls, 0, sizeof(*tls));
    const uintptr_t peb_address = (uintptr_t)tls->peb;
    const uintptr_t image_base = (uintptr_t)image;
    memcpy(tls->teb + 0x60, &peb_address, sizeof(peb_address));
    memcpy(tls->peb + 0x10, &image_base, sizeof(image_base));
    if (!tls_rva) return tls_size == 0;
    if (tls_size < PE_TLS_DIRECTORY_SIZE ||
        !image_range(parsed->image_size, tls_rva, PE_TLS_DIRECTORY_SIZE)) return 0;
    const uint8_t *directory = metadata + tls_rva;
    uint64_t start = r64(directory), end = r64(directory + 8);
    uint64_t index_va = r64(directory + 16);
    uint32_t zero_fill = r32(directory + 32);
    int empty_template = start == 0 && end == 0;
    if (end < start || end - start > parsed->image_size ||
        zero_fill > parsed->image_size - (size_t)(end - start) ||
        (!empty_template && (start < parsed->image_base ||
         end - parsed->image_base > parsed->image_size)) ||
        (index_va && (index_va < parsed->image_base ||
         index_va - parsed->image_base > parsed->image_size - sizeof(uint32_t)))) return 0;
    size_t template_size = (size_t)(end - start);
    size_t block_size = template_size + zero_fill;
    tls->block = calloc(block_size ? block_size : 1, 1);
    if (!tls->block) return 0;
    if (template_size) memcpy(tls->block, metadata + (uint32_t)(start - parsed->image_base), template_size);
    tls->slots[0] = (uintptr_t)tls->block;
    uintptr_t slots_address = (uintptr_t)tls->slots;
    memcpy(tls->teb + 0x58, &slots_address, sizeof(slots_address));
    if (index_va) {
        uint32_t slot = 0;
        memcpy(image + (uint32_t)(index_va - parsed->image_base), &slot, sizeof(slot));
    }
    return 1;
}

static int set_gs(uintptr_t base)
{
    return syscall(SYS_arch_prctl, ARCH_SET_GS, base) == 0;
}

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

static int allowlist_has_module(const struct pe_exec_options *options,
                                const char *module)
{
    for (size_t i = 0; i < options->import_count; ++i) {
        const struct pe_import *entry = &options->imports[i];
        if (entry->module && strcmp(entry->module, module) == 0) return 1;
    }
    return 0;
}

struct dll_set;
static int resolve_fixture_import(struct dll_set *set, const char *module,
                                  const char *symbol, void **address);

static int bind_imports(uint8_t *image, const uint8_t *metadata, uint32_t rva,
                        struct dll_set *set)
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
            if (!resolve_fixture_import(set, module, symbol, &address)) return 0;
            w64(image + iat + (uint64_t)i * 8, (uintptr_t)address);
            if (i == 65535) return 0;
        }
    }
    return 0;
}

struct fixture_image {
    const uint8_t *metadata;
    const uint8_t *optional_header;
    struct pe_image parsed;
    uint8_t *staged;
    size_t staged_size;
    uint8_t *destination;
    long page_size;
    uint32_t reloc_rva, reloc_size;
    uint32_t import_rva;
    uint32_t tls_rva, tls_size;
    uint32_t delay_rva, delay_size;
    struct fixture_tls *tls;
};

static enum pe_status fixture_image_map(struct fixture_image *image,
                                         const uint8_t *data, size_t size, int is_dll)
{
    memset(image, 0, sizeof(*image));
    image->metadata = data;
    if (pe_parse(data, size, &image->parsed) != PE_OK) return PE_INVALID;
    if (image->parsed.is_dll != is_dll) return PE_INVALID;
    if (pe_stage(data, size, &image->parsed, &image->staged,
                 &image->staged_size) != PE_OK) return PE_INVALID;

    image->page_size = sysconf(_SC_PAGESIZE);
    if (image->page_size <= 0) return PE_UNSUPPORTED;
    const uint8_t *optional = data + r32(data + 0x3c) + 24;
    image->optional_header = optional;
    uint32_t directory_count = r32(optional + 108);
    image->reloc_rva = directory_count > 5 ? r32(optional + 112 + 40) : 0;
    image->reloc_size = directory_count > 5 ? r32(optional + 112 + 44) : 0;
    image->import_rva = directory_count > 1 ? r32(optional + 112 + 8) : 0;
    image->tls_rva = directory_count > 9 ? r32(optional + 112 + 72) : 0;
    image->tls_size = directory_count > 9 ? r32(optional + 112 + 76) : 0;
    image->delay_rva = directory_count > 13 ? r32(optional + 112 + 13 * 8) : 0;
    image->delay_size = directory_count > 13 ? r32(optional + 112 + 13 * 8 + 4) : 0;
    if ((!image->tls_rva) != (!image->tls_size)) return PE_INVALID;
    if ((!image->delay_rva) != (!image->delay_size) ||
        (image->delay_rva && !delay_directory_validate(image->staged,
            image->parsed.image_size, image->delay_rva, image->delay_size)))
        return PE_INVALID;

    void *preferred = (void *)(uintptr_t)image->parsed.image_base;
    void *mapped = MAP_FAILED;
    if (preferred && !(image->parsed.image_base % (unsigned long)image->page_size)) {
        mapped = mmap(preferred, image->staged_size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        if (mapped != MAP_FAILED && mapped != preferred) {
            munmap(mapped, image->staged_size);
            mapped = MAP_FAILED;
        }
    }
    if (mapped == MAP_FAILED)
        mapped = mmap(NULL, image->staged_size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapped == MAP_FAILED) return PE_NOMEM;
    image->destination = mapped;
    memcpy(image->destination, image->staged, image->staged_size);
    image->tls = calloc(1, sizeof(*image->tls));
    if (!image->tls) return PE_NOMEM;
    return PE_OK;
}

static int fixture_image_relocate(struct fixture_image *image)
{
    intptr_t delta = (intptr_t)((uintptr_t)image->destination -
                                (uintptr_t)image->parsed.image_base);
    if (!delta) return 1;
    if (!image->reloc_rva || !image->reloc_size) return 0;
    for (uint32_t pos = 0; pos < image->reloc_size;) {
        const uint8_t *block = image->staged + image->reloc_rva + pos;
        uint32_t base = r32(block), block_size = r32(block + 4);
        if (block_size < 8 || block_size > image->reloc_size - pos) return 0;
        for (uint32_t offset = 8; offset < block_size; offset += 2) {
            uint16_t entry = r16(block + offset);
            if ((entry >> 12) == 10) {
                uint8_t *target = image->destination + base + (entry & 0xfff);
                w64(target, r64(target) + (uint64_t)delta);
            }
        }
        pos += block_size;
    }
    return 1;
}

static int fixture_image_protect(struct fixture_image *image, int *executable_entry)
{
    if (mprotect(image->destination, image->staged_size, PROT_READ) != 0) return 0;
    *executable_entry = 0;
    for (uint16_t i = 0; i < image->parsed.section_count; ++i) {
        const uint8_t *section = image->optional_header +
            r16(image->optional_header - 4) + (size_t)i * 40;
        uint32_t flags = r32(section + 36);
        uint32_t start = image->parsed.sections[i].rva;
        uint64_t span = image->parsed.sections[i].virtual_size >
                        image->parsed.sections[i].raw_size ?
                        image->parsed.sections[i].virtual_size :
                        image->parsed.sections[i].raw_size;
        if (!span) continue;
        if (start % (unsigned long)image->page_size) return 0;
        uintptr_t lo = (uintptr_t)image->destination + start;
        uintptr_t hi = ((uintptr_t)image->destination + start + span +
                        (uintptr_t)image->page_size - 1) &
                       ~((uintptr_t)image->page_size - 1);
        int protection = PROT_READ;
        if (flags & 0x80000000u) protection |= PROT_WRITE;
        if (flags & 0x20000000u) protection |= PROT_EXEC;
        if ((protection & (PROT_WRITE | PROT_EXEC)) == (PROT_WRITE | PROT_EXEC) ||
            mprotect((void *)lo, hi - lo, protection) != 0) return 0;
        if ((protection & PROT_EXEC) && image->parsed.entry_rva >= start &&
            image->parsed.entry_rva - start < span) *executable_entry = 1;
    }
    return 1;
}

static void fixture_image_cleanup(struct fixture_image *image)
{
    if (image->tls) {
        free(image->tls->block);
        free(image->tls);
    }
    if (image->destination) munmap(image->destination, image->staged_size);
    pe_stage_free(image->staged, image->staged_size);
    memset(image, 0, sizeof(*image));
}

#define PE_DLL_LIMIT 32u
#define PE_FORWARD_LIMIT 16u

struct loaded_dll {
    const char *name;
    struct fixture_image image;
    unsigned state;
    int attached;
};

struct dll_set {
    struct loaded_dll entries[PE_DLL_LIMIT];
    size_t count;
    size_t order[PE_DLL_LIMIT];
    size_t order_count;
    const struct pe_exec_options *options;
};

static int name_equal(const char *a, const char *b)
{
    if (!a || !b) return 0;
    while (*a && *b) {
        char ca = *a++, cb = *b++;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca + ('a' - 'A'));
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb + ('a' - 'A'));
        if (ca != cb) return 0;
    }
    return *a == *b;
}

static int find_dll(const struct dll_set *set, const char *name)
{
    for (size_t i = 0; i < set->count; ++i)
        if (name_equal(set->entries[i].name, name)) return (int)i;
    return -1;
}

static int resolve_dll_export(struct dll_set *set, int index, const char *symbol,
                              unsigned depth, void **address)
{
    if (depth >= PE_FORWARD_LIMIT || index < 0 || !symbol) return 0;
    struct fixture_image *im = &set->entries[index].image;
    uint32_t directory_count = r32(im->optional_header + 108);
    if (directory_count == 0 || !r32(im->optional_header + 112) ||
        !r32(im->optional_header + 116)) return 0;
    uint32_t exp = r32(im->optional_header + 112);
    uint32_t exp_size = r32(im->optional_header + 116);
    if (!image_range(im->parsed.image_size, exp, 40) || exp_size < 40 ||
        !image_range(im->parsed.image_size, exp, exp_size)) return 0;
    const uint8_t *e = im->staged + exp;
    uint32_t funcs = r32(e + 28), names = r32(e + 32), ordinals = r32(e + 36);
    uint32_t function_count = r32(e + 20), name_count = r32(e + 24);
    if (function_count > 65536 || name_count > function_count ||
        !image_range(im->parsed.image_size, funcs, (size_t)function_count * 4) ||
        !image_range(im->parsed.image_size, names, (size_t)name_count * 4) ||
        !image_range(im->parsed.image_size, ordinals, (size_t)name_count * 2)) return 0;
    uint32_t function_index = UINT32_MAX;
    for (uint32_t i = 0; i < name_count; ++i) {
        uint32_t name_rva = r32(im->staged + names + (size_t)i * 4);
        if (!image_range(im->parsed.image_size, name_rva, 1)) return 0;
        const char *export_name = (const char *)(im->staged + name_rva);
        if (!memchr(export_name, '\0', im->parsed.image_size - name_rva)) return 0;
        if (strcmp(export_name, symbol) == 0) {
            function_index = r16(im->staged + ordinals + (size_t)i * 2);
            break;
        }
    }
    if (function_index >= function_count) return 0;
    uint32_t target = r32(im->staged + funcs + (size_t)function_index * 4);
    if (target >= exp && (uint64_t)target < (uint64_t)exp + exp_size) {
        if (!image_range(im->parsed.image_size, target, 1)) return 0;
        const char *forward = (const char *)(im->staged + target);
        size_t available = im->parsed.image_size - target;
        const char *dot = memchr(forward, '.', available);
        if (!dot || dot == forward || (size_t)(dot - forward) + 1 >= available ||
            !dot[1] ||
            !memchr(dot + 1, '\0', available - (size_t)(dot + 1 - forward))) return 0;
        char module[128];
        size_t module_size = (size_t)(dot - forward);
        if (module_size + 5 > sizeof(module)) return 0;
        memcpy(module, forward, module_size);
        memcpy(module + module_size, ".dll", 5);
        return resolve_dll_export(set, find_dll(set, module), dot + 1,
                                  depth + 1, address);
    }
    if (!image_range(im->parsed.image_size, target, 1) ||
        !executable_rva(im->optional_header, &im->parsed, target)) return 0;
    *address = im->destination + target;
    return 1;
}

static int resolve_fixture_import(struct dll_set *set, const char *module,
                                  const char *symbol, void **address)
{
    int index = find_dll(set, module);
    if (index >= 0) return resolve_dll_export(set, index, symbol, 0, address);
    return resolve_import(set->options, module, symbol, address);
}

/* Private ABI for the repository's single no-argument delay fixture. The
 * module is already attached; only the symbol binding happens on first use. */
struct fixture_delay {
    struct dll_set *dlls;
    uint8_t *slot;
    void *target;
    unsigned calls;
};

static _Thread_local struct fixture_delay *active_delay;

static unsigned __attribute__((ms_abi)) fixture_delay_first_call(void)
{
    struct fixture_delay *delay = active_delay;
    if (!delay || ++delay->calls != 1 ||
        !resolve_dll_export(delay->dlls, find_dll(delay->dlls, "cposdep.dll"),
                            "CposDependencyValue", 0, &delay->target)) return 0;
    w64(delay->slot, (uintptr_t)delay->target);
    return ((unsigned (__attribute__((ms_abi)) *)(void))delay->target)();
}

static int fixture_delay_prepare(struct fixture_image *image, struct dll_set *dlls,
                                 struct fixture_delay *delay)
{
    if (image->import_rva || image->tls_rva || find_dll(dlls, "cposdep.dll") < 0)
        return 0;
    uint32_t slot = r32(image->staged + image->delay_rva + 12);
    const uint8_t *headers = image->optional_header + r16(image->optional_header - 4);
    for (uint16_t i = 0; i < image->parsed.section_count; ++i) {
        const struct pe_section *section = &image->parsed.sections[i];
        uint32_t flags = r32(headers + (size_t)i * 40 + 36);
        if (slot >= section->rva &&
            image_range(section->raw_size, slot - section->rva, 16) &&
            (flags & 0xa0000000u) == 0x80000000u &&
            r64(image->destination + slot) == r64(image->staged + slot)) {
            delay->dlls = dlls;
            delay->slot = image->destination + slot;
            w64(delay->slot, (uintptr_t)fixture_delay_first_call);
            return 1;
        }
    }
    return 0;
}

static int dll_dependencies_first(struct dll_set *set, size_t index)
{
    struct loaded_dll *entry = &set->entries[index];
    if (entry->state == 1) return 0;
    if (entry->state == 2) return 1;
    entry->state = 1;
    uint32_t rva = entry->image.import_rva;
    if (rva) {
        for (uint32_t pos = 0; pos < 65536; pos += 20) {
            if (!image_range(entry->image.parsed.image_size, rva + pos, 20)) return 0;
            const uint8_t *desc = entry->image.staged + rva + pos;
            uint32_t name_rva = r32(desc + 12);
            if (!r32(desc) && !name_rva && !r32(desc + 16)) break;
            if (!image_range(entry->image.parsed.image_size, name_rva, 1)) return 0;
            const char *module = (const char *)(entry->image.staged + name_rva);
            int dependency = find_dll(set, module);
            if (dependency < 0) {
                if (!allowlist_has_module(set->options, module)) return 0;
                continue;
            }
            if (!dll_dependencies_first(set, (size_t)dependency)) return 0;
        }
    }
    entry->state = 2;
    set->order[set->order_count++] = index;
    return 1;
}

static void dll_set_cleanup(struct dll_set *set, int invoke_detach)
{
    if (invoke_detach) {
        for (size_t n = set->order_count; n > 0; --n) {
            struct loaded_dll *entry = &set->entries[set->order[n - 1]];
            if (entry->attached) {
                ((int (__attribute__((ms_abi)) *)(void *, uint32_t, void *))
                 (entry->image.destination + entry->image.parsed.entry_rva))(
                    entry->image.destination, 0, NULL);
                entry->attached = 0;
            }
        }
    }
    for (size_t i = 0; i < set->count; ++i)
        fixture_image_cleanup(&set->entries[i].image);
    memset(set, 0, sizeof(*set));
}

static enum pe_status dll_set_load(struct dll_set *set,
                                   const struct pe_exec_options *options)
{
    memset(set, 0, sizeof(*set));
    set->options = options;
    if (options->dll_count > PE_DLL_LIMIT ||
        (!options->dlls && options->dll_count)) return PE_INVALID;
    set->count = options->dll_count;
    for (size_t i = 0; i < set->count; ++i) {
        const struct pe_fixture_dll *input = &options->dlls[i];
        if (!input->name || !input->data || !input->size) return PE_INVALID;
        set->entries[i].name = input->name;
        if (find_dll(set, input->name) != (int)i) return PE_INVALID;
        enum pe_status status = fixture_image_map(&set->entries[i].image,
                                                  input->data, input->size, 1);
        if (status != PE_OK) return status;
        /* Static TLS and delay binding are implemented only for the EXE fixture. */
        if (set->entries[i].image.delay_rva || set->entries[i].image.tls_rva)
            return PE_UNSUPPORTED;
        if (!fixture_image_relocate(&set->entries[i].image)) return PE_UNSUPPORTED;
    }
    for (size_t i = 0; i < set->count; ++i)
        if (!dll_dependencies_first(set, i)) return PE_UNSUPPORTED;
    for (size_t i = 0; i < set->count; ++i) {
        struct fixture_image *image = &set->entries[i].image;
        if (!bind_imports(image->destination, image->staged, image->import_rva, set))
            return PE_UNSUPPORTED;
    }
    for (size_t i = 0; i < set->count; ++i) {
        int entry_executable = 0;
        if (!fixture_image_protect(&set->entries[i].image, &entry_executable) ||
            (set->entries[i].image.parsed.entry_rva && !entry_executable))
            return PE_UNSUPPORTED;
    }
    if (!options->invoke_entry) return PE_OK;
    for (size_t n = 0; n < set->order_count; ++n) {
        struct loaded_dll *entry = &set->entries[set->order[n]];
        if (!entry->image.parsed.entry_rva) continue;
        int attached = ((int (__attribute__((ms_abi)) *)(void *, uint32_t, void *))
            (entry->image.destination + entry->image.parsed.entry_rva))(
                entry->image.destination, 1, NULL);
        if (!attached) return PE_UNSUPPORTED;
        entry->attached = 1;
    }
    return PE_OK;
}

enum pe_status pe_execute_fixture(const uint8_t *data, size_t size,
                                  const struct pe_exec_options *options)
{
#if !defined(__x86_64__) || !defined(__linux__)
    (void)data; (void)size; (void)options;
    return PE_UNSUPPORTED;
#else
    int current_personality = personality(0xffffffffUL);
    if (current_personality < 0 || (current_personality & READ_IMPLIES_EXEC)) return PE_UNSUPPORTED;
    if (!options || (!options->imports && options->import_count) ||
        (!options->dlls && options->dll_count)) return PE_INVALID;
    struct fixture_image image;
    enum pe_status map_status = fixture_image_map(&image, data, size, 0);
    if (map_status != PE_OK) { fixture_image_cleanup(&image); return map_status; }
    struct dll_set dlls;
    int invoke_dll_detach = 1;
    enum pe_status dll_status = dll_set_load(&dlls, options);
    if (dll_status != PE_OK) {
        dll_set_cleanup(&dlls, invoke_dll_detach);
        fixture_image_cleanup(&image);
        return dll_status;
    }
    int executable_entry = 0;
    struct fixture_delay delay = {0};
    if (!fixture_image_relocate(&image)) goto invalid;
    if (image.delay_rva && (!options->invoke_entry ||
        !fixture_delay_prepare(&image, &dlls, &delay))) goto unsupported;
    if (!bind_imports(image.destination, image.staged, image.import_rva, &dlls)) goto invalid;
    if (options->invoke_entry && !setup_tls(image.destination, image.staged,
            &image.parsed, image.tls_rva, image.tls_size, image.tls)) goto unsupported;
    if (!fixture_image_protect(&image, &executable_entry)) goto unsupported;
    if (options->invoke_entry) {
        if (!executable_entry) goto unsupported;
        uintptr_t previous_gs = 0;
        if (syscall(SYS_arch_prctl, ARCH_GET_GS, &previous_gs) != 0) goto unsupported;
        if (!set_gs((uintptr_t)image.tls->teb)) goto unsupported;
        int callbacks_ok = invoke_tls_callbacks(image.destination, image.staged,
            &image.parsed, image.optional_header, image.tls_rva, image.tls_size);
        if (callbacks_ok && delay.slot) {
            struct fixture_delay *previous_delay = active_delay;
            void (__attribute__((ms_abi)) *entry)(void) =
                (void *)(image.destination + image.parsed.entry_rva);
            active_delay = &delay;
            callbacks_ok = r64(delay.slot) == (uintptr_t)fixture_delay_first_call &&
                           delay.calls == 0 && !delay.target;
            if (callbacks_ok) {
                entry();
                callbacks_ok = delay.calls == 1 && delay.target &&
                               r64(delay.slot) == (uintptr_t)delay.target;
                if (callbacks_ok) {
                    entry();
                    callbacks_ok = delay.calls == 1 &&
                                   r64(delay.slot) == (uintptr_t)delay.target;
                }
            }
            active_delay = previous_delay;
        } else if (callbacks_ok)
            ((void (__attribute__((ms_abi)) *)(void))
             (image.destination + image.parsed.entry_rva))();
        int restored = set_gs(previous_gs);
        if (!restored) {
            image.tls = NULL;
            invoke_dll_detach = 0;
            goto unsupported;
        }
        free(image.tls->block);
        image.tls->block = NULL;
        if (!callbacks_ok) goto unsupported;
    }
    fixture_image_cleanup(&image);
    dll_set_cleanup(&dlls, invoke_dll_detach);
    return PE_OK;
invalid:
unsupported:
    fixture_image_cleanup(&image);
    dll_set_cleanup(&dlls, invoke_dll_detach);
    return PE_UNSUPPORTED;
#endif
}
