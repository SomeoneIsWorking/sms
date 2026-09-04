#!/usr/bin/env python3
"""Extract and demangle symbols for one source file from mario.MAP."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from tools.symbol_demangle import (
    demangle,
    matching_map_lines,
    resolve_dtk,
    symbol_candidates,
    useful_demangle,
)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cpp_filename")
    parser.add_argument("output_file", nargs="?", type=Path)
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    map_path = Path("mario.MAP")
    if not map_path.is_file():
        print(f"extract-symbols: REFUSING: {map_path} not found", file=sys.stderr)
        return 2
    try:
        dtk = resolve_dtk(Path.cwd())
    except FileNotFoundError as error:
        print(f"extract-symbols: REFUSING: {error}", file=sys.stderr)
        return 2

    lines = matching_map_lines(map_path.read_text(errors="replace").splitlines(), arguments.cpp_filename)
    print(f"Found {len(lines)} matching lines in {map_path}")
    demangled: list[str] = []
    failures: list[str] = []
    for symbol in symbol_candidates(lines):
        result = demangle(dtk, symbol)
        if result.error:
            failures.append(f"{symbol}: {result.error}")
        elif useful_demangle(result):
            demangled.append(result.demangled)
            if arguments.output_file is None:
                print(f"SUCCESS - Original: {symbol}")
                print(f"Demangled: {result.demangled}")

    if failures:
        for failure in failures:
            print(f"extract-symbols: {failure}", file=sys.stderr)
        print("extract-symbols: REFUSING to write partial results", file=sys.stderr)
        return 1
    if arguments.output_file is not None:
        arguments.output_file.write_text("".join(f"{symbol}\n" for symbol in demangled))
        print(f"Saved {len(demangled)} symbols to {arguments.output_file}")
    else:
        print(f"Demangled {len(demangled)} symbols")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
