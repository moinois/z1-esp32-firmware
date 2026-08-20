"""Guards concise requirement statuses and their one-to-one evidence ledgers."""

from __future__ import annotations

from pathlib import Path
import re


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


def test_requirement_physical_status_is_concise_linked_and_has_evidence() -> None:
    """Every physical status links directly to one matching evidence row."""

    requirements = _table_rows(
        _ROOT / "docs" / "requirements.md",
        "| Area | Implementation | Automated verification | "
        "Target integration and physical evidence |",
    )
    evidence = _table_rows(
        _ROOT / "docs" / "physical-verification-evidence.md",
        "| Requirement area | Status | "
        "Detailed target integration and physical evidence |",
    )

    assert len(requirements) == len(evidence)
    for index, (requirement, detail) in enumerate(zip(requirements, evidence), 1):
        anchor = f"phys-{index:03d}"
        status_match = re.fullmatch(
            rf"\[(Yes|Partial|Pending fixture|Not required)\]"
            rf"\(physical-verification-evidence\.md#{anchor}\)",
            requirement[3],
        )
        assert status_match
        assert detail[0] == f'<a id="{anchor}"></a>{requirement[0]}'
        assert detail[1] == status_match.group(1)
        assert detail[2]

    counts = {
        status: sum(detail[1] == status for detail in evidence)
        for status in ("Yes", "Partial", "Pending fixture", "Not required")
    }
    requirements_text = (_ROOT / "docs" / "requirements.md").read_text(
        encoding="utf-8"
    )
    summary = re.search(
        r"current matrix contains (\d+) `Yes`,\n"
        r"(\d+) `Partial`, (\d+) `Pending fixture`, and "
        r"(\d+) `Not required` rows",
        requirements_text,
    )
    assert summary
    assert tuple(map(int, summary.groups())) == tuple(counts.values())


def test_physical_backlog_classifies_every_pending_fixture_once() -> None:
    """The actionable backlog neither loses nor duplicates pending rows."""

    evidence = _table_rows(
        _ROOT / "docs" / "physical-verification-evidence.md",
        "| Requirement area | Status | "
        "Detailed target integration and physical evidence |",
    )
    pending_anchors = {
        re.match(r'<a id="(phys-\d{3})"></a>', row[0]).group(1)
        for row in evidence
        if row[1] == "Pending fixture"
    }
    backlog = (_ROOT / "docs" / "physical-verification-backlog.md").read_text(
        encoding="utf-8"
    )
    backlog_anchors = re.findall(
        r"\(physical-verification-evidence\.md#(phys-\d{3})\)", backlog
    )

    assert len(backlog_anchors) == len(set(backlog_anchors))
    assert set(backlog_anchors) == pending_anchors
