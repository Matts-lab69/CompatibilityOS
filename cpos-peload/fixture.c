/* SPDX-License-Identifier: MIT */
static volatile unsigned int marker;
static volatile unsigned int *volatile target = &marker;

typedef void (*tls_callback)(void *, unsigned long, void *);

static void fixture_tls_callback(void *image, unsigned long reason, void *reserved)
{
    (void)image;
    (void)reserved;
    if (reason == 1) marker = 41;
}

static tls_callback const fixture_tls_callbacks[] = {fixture_tls_callback, 0};
/* GNU PE linker uses _tls_used for the TLS data-directory entry. */
__attribute__((section(".rdata$T"), used))
const struct {
    const void *start, *end;
    unsigned int *index;
    tls_callback const *callbacks;
    unsigned int zero_fill, characteristics;
} _tls_used = {0, 0, 0, fixture_tls_callbacks, 0, 0};

void fixture_entry(void)
{
    if (marker != 41) __builtin_trap();
    *target = 42;
    if (marker != 42) __builtin_trap();
}
