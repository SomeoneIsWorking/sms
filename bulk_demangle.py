#!/usr/bin/env python3
"""Demangle newline-delimited MWCC symbols from standard input."""

from __future__ import annotations

import sys
from pathlib import Path

from tools.symbol_demangle import demangle, resolve_dtk


def main() -> int:
    try:
        dtk = resolve_dtk(Path.cwd())
    except FileNotFoundError as error:
        print(f"bulk-demangle: REFUSING: {error}", file=sys.stderr)
        return 2

    failed = False
    print()
    for symbol in sys.stdin.read().splitlines():
        result = demangle(dtk, symbol)
        if result.demangled:
            print(result.demangled)
        if result.error:
            print(f"bulk-demangle: {symbol}: {result.error}", file=sys.stderr)
            failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
