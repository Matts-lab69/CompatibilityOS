/* SPDX-License-Identifier: MIT */
static volatile unsigned int marker;
static volatile unsigned int *volatile target = &marker;

void fixture_entry(void)
{
    *target = 42;
    if (marker != 42) __builtin_trap();
}
