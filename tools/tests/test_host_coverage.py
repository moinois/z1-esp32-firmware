"""Tests deterministic coverage summary and badge generation."""

from __future__ import annotations

import unittest

from tools.host_coverage import coverage_badge_svg, coverage_color, line_coverage


class HostCoverageTests(unittest.TestCase):
    """Verifies coverage parsing, badge thresholds, and SVG generation."""

    def test_extracts_line_percentage_from_llvm_export(self) -> None:
        exported = {
            "data": [{"totals": {"lines": {"percent": 92.50987166831194}}}]
        }

        self.assertEqual(line_coverage(exported), 92.50987166831194)

    def test_badge_rounds_to_one_decimal_and_escapes_percent(self) -> None:
        badge = coverage_badge_svg(92.50987166831194)

        self.assertIn("coverage", badge)
        self.assertIn("92.5%", badge)
        self.assertIn("#4c1", badge)
        self.assertTrue(badge.endswith("\n"))

    def test_badge_colors_describe_coverage_quality(self) -> None:
        self.assertEqual(coverage_color(95.0), "#4c1")
        self.assertEqual(coverage_color(85.0), "#97ca00")
        self.assertEqual(coverage_color(75.0), "#a4a61d")
        self.assertEqual(coverage_color(65.0), "#dfb317")
        self.assertEqual(coverage_color(55.0), "#fe7d37")
        self.assertEqual(coverage_color(45.0), "#e05d44")


if __name__ == "__main__":
    unittest.main()
