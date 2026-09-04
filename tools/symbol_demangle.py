"""Shared symbol extraction and dtk demangling operations."""

from __future__ import annotations

import re
import subprocess
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class DemangleResult:
    source: str
    demangled: str
    error: str | None


def resolve_dtk(root: Path) -> Path:
    candidates = (root / "build/tools/dtk", root / "build/tools/dtk.exe")
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    tried = ", ".join(str(candidate) for candidate in candidates)
    raise FileNotFoundError(f"dtk demangler not found; tried: {tried}")


def demangle(dtk: Path, symbol: str) -> DemangleResult:
    result = subprocess.run(
        [str(dtk), "demangle", symbol],
        check=False,
        capture_output=True,
        text=True,
    )
    output = result.stdout.strip()
    if result.returncode:
        detail = result.stderr.strip() or f"dtk exited {result.returncode}"
        return DemangleResult(symbol, output, detail)
    return DemangleResult(symbol, output, None)


def matching_map_lines(lines: list[str], source_name: str) -> list[str]:
    needle = source_name.casefold()
    return [line for line in lines if needle in line.casefold()]


def symbol_candidates(lines: list[str]) -> list[str]:
    candidates: list[str] = []
    for line in lines:
        for word in line.split():
            if _is_symbol_candidate(word):
                candidates.append(word)
    return candidates


def useful_demangle(result: DemangleResult) -> bool:
    lowered = result.demangled.casefold()
    return (
        result.error is None
        and bool(result.demangled)
        and result.demangled != result.source
        and "error" not in lowered
        and "failed" not in lowered
    )


def _is_symbol_candidate(word: str) -> bool:
    if len(word) < 5 or word.isdecimal() or word.startswith("0x"):
        return False
    if any(suffix in word for suffix in (".cpp", ".o", ".h")):
        return False
    return re.search(r"_[0-9]*", word) is not None
