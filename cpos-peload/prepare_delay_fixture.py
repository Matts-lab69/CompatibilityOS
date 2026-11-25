#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Validate a linked fixture and register its RVA-form delay directory.

The script edits only the supplied project-owned PE fixture and reports the
dedicated IAT RVA and initial value. It does not implement lazy resolution;
the execution harness must observe that slot before and after entry invocation.
"""
import argparse
import struct
from pathlib import Path


def u16(data, offset):
    return struct.unpack_from("<H", data, offset)[0]


def u32(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


def put32(data, offset, value):
    struct.pack_into("<I", data, offset, value)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    args = parser.parse_args()
    data = bytearray(args.image.read_bytes())
    if len(data) < 64 or data[:2] != b"MZ":
        raise SystemExit("not a PE image")
    pe = u32(data, 0x3c)
    if pe + 24 > len(data) or data[pe:pe + 4] != b"PE\0\0" or u16(data, pe + 4) != 0x8664:
        raise SystemExit("expected an AMD64 PE image")
    section_count = u16(data, pe + 6)
    optional_size = u16(data, pe + 20)
    optional = pe + 24
    if (optional_size < 112 + 14 * 8 or optional + optional_size > len(data)
            or u16(data, optional) != 0x20b or u32(data, optional + 108) <= 13):
        raise SystemExit("PE32+ image has no data-directory slot 13")
    section_table = optional + optional_size
    if section_table + 40 * section_count > len(data):
        raise SystemExit("truncated section table")
    sections = []
    for index in range(section_count):
        off = section_table + 40 * index
        name = bytes(data[off:off + 8]).split(b"\0", 1)[0]
        virtual_size, va, raw_size, raw = struct.unpack_from("<IIII", data, off + 8)
        if raw_size and raw + raw_size > len(data):
            raise SystemExit("truncated section data")
        sections.append((va, max(virtual_size, raw_size), raw, raw_size, name))

    def rva_offset(rva, size=1):
        for va, extent, raw, raw_size, _ in sections:
            if va <= rva and rva + size <= va + extent and rva - va + size <= raw_size:
                return raw + rva - va
        raise ValueError(f"RVA 0x{rva:x} is not backed by section data")

    # Infer the descriptor from the assembler's rdata$didata payload, rather
    # than relying on a linker-emitted directory. Validate all RVAs before
    # publishing data-directory entry 13.
    matches = []
    for va, _, raw, raw_size, name in sections:
        if name != b".rdata":
            continue
        for pos in range(raw, raw + raw_size - 64 + 1, 4):
            if u32(data, pos) != 1 or any(data[pos + 32:pos + 64]):
                continue
            descriptor = struct.unpack_from("<8I", data, pos)
            _, name_rva, module_rva, iat_rva, int_rva, bound, unload, timestamp = descriptor
            try:
                expected_module = b"cposdep.dll\0"
                module_name_off = rva_offset(name_rva, len(expected_module))
                module_name = data[module_name_off:module_name_off + len(expected_module)]
                module_slot_off = rva_offset(module_rva, 8)
                name_rva_offset = rva_offset(int_rva, 16)
                thunk_name_rva = u32(data, name_rva_offset)
                expected_hint_name = b"\0\0CposDependencyValue\0"
                hint_name_offset = rva_offset(thunk_name_rva, len(expected_hint_name))
                hint_name = data[hint_name_offset:hint_name_offset + len(expected_hint_name)]
                iat_off = rva_offset(iat_rva, 16)
                initial_iat = u32(data, iat_off)
                valid_tail = (not any(data[name_rva_offset + 4:name_rva_offset + 16])
                              and not any(data[iat_off + 4:iat_off + 16]))
                if (module_name == expected_module and hint_name == expected_hint_name
                        and thunk_name_rva == initial_iat and valid_tail
                        and not any(data[module_slot_off:module_slot_off + 8])
                        and not bound and not unload and not timestamp):
                    matches.append((va + pos - raw, iat_rva, initial_iat))
            except (ValueError, struct.error):
                continue
    if len(matches) != 1:
        raise SystemExit(f"expected one valid delay descriptor; found {len(matches)}")
    descriptor_rva, iat_rva, initial_iat = matches[0]
    directory = optional + 112 + 13 * 8
    # Refuse to overwrite a conflicting directory; linker zero is normal.
    existing_rva, existing_size = struct.unpack_from("<II", data, directory)
    if (existing_rva, existing_size) not in ((0, 0), (descriptor_rva, 64)):
        raise SystemExit("linker published a conflicting delay-import directory")
    put32(data, directory, descriptor_rva)
    put32(data, directory + 4, 64)  # descriptor plus all-zero terminator
    args.image.write_bytes(data)
    print(f"delay_directory_rva=0x{descriptor_rva:x} size=64")
    print(f"delay_iat_rva=0x{iat_rva:x} initial_value=0x{initial_iat:x}")


if __name__ == "__main__":
    main()
