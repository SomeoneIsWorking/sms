#!/usr/bin/env python3
"""Focused tests for the shared MAP symbol selection policy."""

from __future__ import annotations

import unittest

from symbol_demangle import (
    DemangleResult,
    matching_map_lines,
    symbol_candidates,
    useful_demangle,
)


class SymbolDemangleTest(unittest.TestCase):
    def test_matching_and_candidate_extraction(self) -> None:
        lines = ["MarioMain.cpp __ct__6TMarioFv file.o 1234", "Other.cpp __ct__6TOtherFv"]
        matching = matching_map_lines(lines, "mariomain.CPP")
        self.assertEqual(symbol_candidates(matching), ["__ct__6TMarioFv"])

    def test_failed_or_identity_demangle_is_not_useful(self) -> None:
        self.assertFalse(useful_demangle(DemangleResult("symbol", "symbol", None)))
        self.assertFalse(useful_demangle(DemangleResult("symbol", "Name", "failure")))
        self.assertTrue(useful_demangle(DemangleResult("symbol", "Name()", None)))


if __name__ == "__main__":
    unittest.main()
