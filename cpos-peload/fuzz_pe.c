/* SPDX-License-Identifier: MIT */
#include "pe.h"

#include <stddef.h>
#include <stdlib.h>

int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size)
{
    struct pe_image image;
    if (pe_parse(data, size, &image) == PE_OK) {
        unsigned char *staging = NULL;
        size_t staging_size = 0;
        if (pe_stage(data, size, &image, &staging, &staging_size) == PE_OK)
            pe_stage_free(staging, staging_size);
    }
    return 0;
}
