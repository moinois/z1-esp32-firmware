"""Guards the concise requirement-status contract and its evidence ledger."""

from __future__ import annotations

from pathlib import Path


_ROOT = Path(__file__).resolve().parents[2]


def _table_rows(path: Path, header: str) -> list[list[str]]:
    """Returns cells from the first Markdown table following an exact header."""

    lines = path.read_text(encoding="utf-8").splitlines()
    start = lines.index(header) + 2
    rows: list[list[str]] = []
    for line in lines[start:]:
        if not line.startswith("|"):
            break
        rows.append([cell.strip() for cell in line.strip("|").split("|")])
    return rows


def test_requirement_automation_status_is_concise_and_has_evidence() -> None:
    """Every status is scannable and retains one matching evidence entry."""

    requirements = _table_rows(
        _ROOT / "docs" / "requirements.md",
        "| Area | Implementation | Automated verification | "
        "Target integration and physical evidence |",
    )
    evidence = _table_rows(
        _ROOT / "docs" / "automated-verification-evidence.md",
        "| Requirement area | Automated evidence |",
    )

    assert requirements
    assert all(len(row) == 4 for row in requirements)
    assert all(
        row[2] in {"Yes", "No"} or row[2].startswith("Partial:")
        for row in requirements
    )
    assert [row[0] for row in requirements] == [row[0] for row in evidence]
    assert all(len(row) == 2 and row[1] for row in evidence)
