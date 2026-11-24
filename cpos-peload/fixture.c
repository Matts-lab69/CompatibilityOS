/* SPDX-License-Identifier: MIT */
#include <stdint.h>
static volatile unsigned int marker;
static volatile unsigned int *volatile target = &marker;
__attribute__((section(".tls$AAA"), used)) static const unsigned int fixture_tls_start = 41;
__attribute__((section(".tls$ZZZ"), used)) static const unsigned int fixture_tls_end = 0;
static unsigned int fixture_tls_index;

static unsigned int *fixture_tls_slot(void)
{
    unsigned long *array;
    __asm__ volatile ("movq %%gs:0x58, %0" : "=r" (array));
    return (unsigned int *)(uintptr_t)array[0];
}

typedef void (*tls_callback)(void *, unsigned long, void *);

static void fixture_tls_callback(void *image, unsigned long reason, void *reserved)
{
    (void)image;
    (void)reserved;
    if (reason == 1) *fixture_tls_slot() = fixture_tls_start;
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
    if (*fixture_tls_slot() != 41) __builtin_trap();
    *target = 42;
    if (marker != 42) __builtin_trap();
}
