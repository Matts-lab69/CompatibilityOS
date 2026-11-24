#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Receive and verify one synthetic RAW printer job on loopback."""

import argparse
import socket
import sys


EXPECTED = (
    b"CPOS spool fixture\r\n"
    b"RAW printer transport test\r\n"
    b"0123456789abcdef\r\n"
)
MAX_JOB_SIZE = 1024 * 1024


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=0,
                        help="loopback TCP port (0 selects an available port)")
    args = parser.parse_args()
    if not 0 <= args.port <= 65535:
        parser.error("port must be between 0 and 65535")

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind(("127.0.0.1", args.port))
        server.listen(1)
        server.settimeout(120)
        print(f"READY 127.0.0.1 {server.getsockname()[1]}", flush=True)
        try:
            connection, address = server.accept()
        except TimeoutError:
            print("Timed out waiting for printer job", file=sys.stderr)
            return 1
        if address[0] != "127.0.0.1":
            print("Rejected non-loopback peer", file=sys.stderr)
            return 1

        received = bytearray()
        with connection:
            connection.settimeout(30)
            while True:
                chunk = connection.recv(4096)
                if not chunk:
                    break
                received.extend(chunk)
                if len(received) > MAX_JOB_SIZE:
                    print("Received job exceeds size limit", file=sys.stderr)
                    return 1

    if bytes(received) != EXPECTED:
        print(f"Job bytes mismatch: expected {len(EXPECTED)}, got {len(received)}",
              file=sys.stderr)
        return 1
    print(f"Verified one RAW job ({len(received)} bytes)", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
