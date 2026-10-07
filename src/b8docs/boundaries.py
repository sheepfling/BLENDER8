"""Regression guards for product-policy leakage into supplier manuals."""

from __future__ import annotations

import json
from pathlib import Path

from .models import load_catalog


def audit_boundaries(root: Path) -> dict[str, object]:
    from .manuals import HANDOUTS, OPTIONAL_ORDER, ORDER

    rules = json.loads((root / "internal/authoring/spec/document-roles.json").read_text())
    catalog = {d.id: d for d in load_catalog(root).documents}
    violations: list[dict[str, str]] = []
    members = [source for handout in HANDOUTS for source in handout.sources]
    if len(HANDOUTS) != rules["required_recipient_count"] or sorted(members) != sorted(ORDER):
        violations.append({"source": "packet", "term": "membership or count mismatch"})
    for key in (*ORDER, *OPTIONAL_ORDER):
        if key not in rules["sources"]:
            violations.append({"source": key, "term": "missing ownership declaration"})
        text = (root / catalog[key].source).read_text().replace(r"\_", "_").casefold()
        if key == "requirements":
            text += (
                "\n"
                + (root / "internal/authoring/correspondence/archive.json").read_text().casefold()
            )
            for extra in sorted((root / "latex/correspondence").glob("*.tex")):
                text += "\n" + extra.read_text().replace(r"\_", "_").casefold()
        forbidden = rules["prohibited_in_all_published_sources"] + rules["prohibited_terms"].get(
            key, []
        )
        for term in forbidden:
            if term.casefold() in text:
                violations.append({"source": key, "term": term})
    return {
        "passed": not violations,
        "violations": violations,
        "recipient_documents": len(HANDOUTS),
        "source_units": len(ORDER) + len(OPTIONAL_ORDER),
        "scope": rules["limitations"],
    }
