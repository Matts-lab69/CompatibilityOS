/* SPDX-License-Identifier: MIT */
#ifndef CPOS_PE_H
#define CPOS_PE_H

#include <stddef.h>
#include <stdint.h>

enum pe_status { PE_OK, PE_INVALID, PE_UNSUPPORTED, PE_NOMEM };
struct pe_section { uint32_t rva, virtual_size, raw_offset, raw_size; };
struct pe_image {
    uint64_t image_base;
    uint32_t image_size, headers_size, entry_rva;
    uint16_t section_count;
    struct pe_section sections[96];
};

enum pe_status pe_parse(const uint8_t *data, size_t size, struct pe_image *out);
enum pe_status pe_stage(const uint8_t *data, size_t size,
                        const struct pe_image *image, uint8_t **staging,
                        size_t *staging_size);
void pe_stage_free(uint8_t *staging, size_t size);

/* Experimental fixture-only execution API. Import names are exact, caller-owned
 * allowlist entries; addresses are never looked up in the host process. */
struct pe_import {
    const char *module;
    const char *symbol;
    void *address;
};
struct pe_exec_options {
    const struct pe_import *imports;
    size_t import_count;
    int invoke_entry;
};
enum pe_status pe_execute_fixture(const uint8_t *data, size_t size,
                                 const struct pe_exec_options *options);
#endif
