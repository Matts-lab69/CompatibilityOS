/* SPDX-License-Identifier: MIT */
#include <stdint.h>
static volatile unsigned int marker;
static volatile unsigned int *volatile target = &marker;
static volatile uintptr_t fixture_image_base;
__attribute__((section(".tls$AAA"), used)) static const unsigned int fixture_tls_start = 41;
__attribute__((section(".tls$ZZZ"), used)) static const unsigned int fixture_tls_end = 0;
static unsigned int fixture_tls_index;

static unsigned int *fixture_tls_slot(void)
{
    uintptr_t *array;
    __asm__ volatile ("movq %%gs:0x58, %0" : "=r" (array));
    return (unsigned int *)array[0];
}

static uintptr_t fixture_peb_image_base(void)
{
    uintptr_t peb;
    uintptr_t image_base;
    __asm__ volatile ("movq %%gs:0x60, %0" : "=r" (peb));
    if (!peb) __builtin_trap();
    __builtin_memcpy(&image_base, (const void *)(peb + 0x10), sizeof(image_base));
    return image_base;
}

typedef void (*tls_callback)(void *, unsigned long, void *);

static void fixture_tls_callback(void *image, unsigned long reason, void *reserved)
{
    (void)reserved;
    if (reason == 1) {
        if (fixture_peb_image_base() != (uintptr_t)image) __builtin_trap();
        fixture_image_base = (uintptr_t)image;
        *fixture_tls_slot() = fixture_tls_start;
    }
}

static tls_callback const fixture_tls_callbacks[] = {fixture_tls_callback, 0};
/* GNU PE linker uses _tls_used for the TLS data-directory entry. */
__attribute__((section(".rdata$T"), used))
const struct {
    const void *start, *end;
    unsigned int *index;
    tls_callback const *callbacks;
    unsigned int zero_fill, characteristics;
} _tls_used = {
    &fixture_tls_start, &fixture_tls_end, &fixture_tls_index,
    fixture_tls_callbacks, 0, 0
};

void fixture_entry(void)
{
    if (!fixture_image_base || fixture_peb_image_base() != fixture_image_base) __builtin_trap();
    if (*fixture_tls_slot() != 41) __builtin_trap();
    *target = 42;
    if (marker != 42) __builtin_trap();
}
