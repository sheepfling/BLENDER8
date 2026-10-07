from __future__ import annotations

import inspect
import json
import shutil
import unicodedata
from pathlib import Path
from typing import Any, cast

import pytest
from pydantic import ValidationError
from pypdf import PdfReader

from b8docs.boundaries import audit_boundaries
from b8docs.correspondence import Archive, generate_correspondence, load_archive, tex_escape

ROOT = Path.cwd().resolve()


def source() -> dict[str, Any]:
    return cast(
        dict[str, Any],
        json.loads((ROOT / "internal/authoring/correspondence/archive.json").read_text()),
    )


def test_seven_complete_five_message_exchanges_and_a_handoff() -> None:
    a = load_archive(ROOT)
    assert len(a.threads) == 7
    assert sum(len(t.messages) for t in a.threads) == 35
    assert a.forward.author == "rick" and a.forward.to == ["team"]
    assert all(t.messages[-1].author == "mara" for t in a.threads)
    assert all(t.messages[-1].in_reply_to == t.messages[-2].id for t in a.threads)


@pytest.mark.parametrize(
    "kind",
    [
        "wrong_name",
        "wrong_role",
        "wrong_recipient",
        "bad_order",
        "bad_reply",
        "missing_sketch",
        "duplicate_id",
        "early_handoff",
        "naive_date",
        "unsafe_path",
        "missing_subject",
        "missing_reply_prefix",
        "missing_external_marker",
        "missing_signature",
        "learner_in_mail",
    ],
)
def test_invalid_archives_fail_loudly(kind: str) -> None:
    data = source()
    if kind == "wrong_name":
        data["people"]["team"]["name"] = "Jordan"
    elif kind == "wrong_role":
        data["people"]["rick"]["organization"] = "Kestrel"
    elif kind == "wrong_recipient":
        data["threads"][0]["messages"][1]["to"] = ["team"]
    elif kind == "bad_order":
        data["threads"][0]["messages"][2]["author"] = "rick"
    elif kind == "bad_reply":
        data["threads"][0]["messages"][3]["in_reply_to"] = "panel-01"
    elif kind == "missing_sketch":
        del data["threads"][0]["messages"][1]["sketch"]
    elif kind == "duplicate_id":
        data["forward"]["id"] = "panel-01"
    elif kind == "early_handoff":
        data["forward"]["date"] = "2026-09-01T12:00:00-05:00"
    elif kind == "naive_date":
        data["forward"]["date"] = "2026-10-06T16:42:00"
    elif kind == "unsafe_path":
        data["attachments"][0]["path"] = "../secret.tex"
    elif kind == "missing_subject":
        data["threads"][0]["messages"][1]["subject"] = ""
    elif kind == "missing_reply_prefix":
        data["threads"][0]["messages"][1]["subject"] = data["threads"][0]["subject"]
    elif kind == "missing_external_marker":
        data["threads"][0]["messages"][0]["subject"] = data["threads"][0]["subject"]
    elif kind == "missing_signature":
        data["people"]["mara"]["signature"] = []
    elif kind == "learner_in_mail":
        data["threads"][0]["messages"][3]["body"].append("The New Employee will do this.")
    with pytest.raises(ValidationError):
        Archive.model_validate(data)


def test_roundtrip_source_preserves_every_email_body() -> None:
    a = load_archive(ROOT)
    b = Archive.model_validate_json(a.model_dump_json())
    assert a == b


def test_public_json_matches_canonical_mail_source() -> None:
    public = json.loads((ROOT / "platform/requirements/correspondence.json").read_text())
    assert public == source()
    assert public["people"]["team"]["name"] == "Firmware team"
    assert public["organization"] == "Half-A/Labs"
    rendered = " ".join((ROOT / "platform/requirements/correspondence.md").read_text().split())
    messages = [public["forward"]] + [
        message for thread in public["threads"] for message in thread["messages"]
    ]
    assert all(
        " ".join(paragraph.split()) in rendered
        for message in messages
        for paragraph in message["body"]
    )


def test_current_mail_is_not_the_old_hidden_numbered_spec() -> None:
    text = (ROOT / "internal/authoring/correspondence/archive.json").read_text()
    for forbidden in (
        "R-004",
        "rolling 200 ms",
        "below 62",
        "above 574",
        "scan every 8",
        "16 ms debounce",
    ):
        assert forbidden not in text
    assert "choose how to scan and debounce" in text
    assert "choose the tach estimator" in text
    assert "choose and justify the analog plausibility band" in text


def test_source_generation_is_repeatable(tmp_path: Path) -> None:
    (tmp_path / "internal/authoring").mkdir(parents=True)
    shutil.copytree(
        ROOT / "internal/authoring/correspondence", tmp_path / "internal/authoring/correspondence"
    )
    shutil.copytree(ROOT / "latex", tmp_path / "latex")
    report = generate_correspondence(tmp_path)
    a = (tmp_path / "generated/correspondence-body.tex").read_bytes()
    assert report == {"threads": 7, "messages": 36, "attachments": 2}
    generate_correspondence(tmp_path)
    assert (tmp_path / "generated/correspondence-body.tex").read_bytes() == a
    assert a.count(b"\\begin{Mail}") == 36
    assert a.count(b"\\MailSketch{") == 7
    assert a.count(b"\\MailSignature{") == 36
    assert a.count(b"\\MailSignature{mara}") == 21
    assert a.count(b"\\MailSignature{rick}") == 15
    text = a.decode()
    archive = load_archive(tmp_path)
    markdown = (tmp_path / "generated/customer-correspondence.md").read_text()
    assert markdown.count("![Kestrel K logo](assets/kestrel-mark.svg)") == 21
    for message in [archive.forward, *[m for t in archive.threads for m in t.messages]]:
        assert "}{" + tex_escape(message.subject) + "}\n" in text
        assert "**Subject:** " + message.subject in markdown
        for line in archive.people[message.author].signature:
            assert tex_escape(line) in text
            assert line in markdown.replace("<", "").replace(">", "")
    assert "Rick's inline sketch" not in text
    assert "PULSE boost and return to the selected speed" in text


def test_printed_mail_retains_subjects_and_sender_footers() -> None:
    path = ROOT / "build/docs/clean/HAL-Customer-Correspondence.pdf"
    if not path.exists():
        pytest.skip("Build the correspondence first.")
    text = unicodedata.normalize("NFKC", "\n".join(p.extract_text() for p in PdfReader(path).pages))
    assert text.count("Subject:") == 36
    assert text.count("Subject: [EXTERNAL]") == 21
    assert text.count("Subject: RE:") == 14
    assert text.count("mara.ellis@kestrel.example") == 21
    assert text.count("Rick\nHalf-A/Labs") == 15
    assert "New Employee" not in text
    assert "Rick's inline sketch" not in text
    for thread in load_archive(ROOT).threads:
        for message in thread.messages:
            assert message.subject in text


def test_tex_escaping_cannot_inject_commands() -> None:
    assert tex_escape("A&B 10% _ $") == r"A\&B 10\% \_ \$"
    assert tex_escape(r"\input{x}") == r"\textbackslash{}input\{x\}"


def test_mail_bodies_are_bounded_not_an_unabridged_matrix() -> None:
    a = load_archive(ROOT)
    words = sum(len(" ".join(m.body).split()) for t in a.threads for m in t.messages)
    assert words < 3100
    assert all(len(" ".join(t.messages[0].body).split()) < 100 for t in a.threads)


def test_boundary_guard_reads_generated_source_inputs(tmp_path: Path) -> None:
    for folder in (
        "latex",
        "internal/authoring/config",
        "internal/authoring/spec",
        "internal/authoring/correspondence",
    ):
        (tmp_path / folder).parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(ROOT / folder, tmp_path / folder)
    p = tmp_path / "internal/authoring/correspondence/archive.json"
    p.write_text(p.read_text().replace("Firmware team", "Jordan"))
    result = audit_boundaries(tmp_path)
    assert result["passed"] is False
    violations = cast(list[dict[str, str]], result["violations"])
    assert any(row["source"] == "requirements" for row in violations)


def test_no_customer_or_contractor_is_sending_real_email() -> None:
    a = load_archive(ROOT)
    assert all(person.email.endswith(".example") for person in a.people.values())
    # The generator is filesystem-only, with no mail connector dependency.
    import b8docs.correspondence as module

    assert "smtplib" not in inspect.getsource(module)


def test_retained_drawings_do_not_point_to_retired_attachment_labels() -> None:
    text = (ROOT / "latex/correspondence/rear-switch.tex").read_text()
    assert "permissions in B1" not in text
    assert "bench-harness-r4.pdf" in text


def test_jar_change_has_an_approved_later_revision_and_no_lid_claim() -> None:
    a = load_archive(ROOT)
    t = a.threads[-1]
    assert t.id == "interlock"
    assert all(x.parent_message == "interlock-02" for x in a.attachments)
    assert all("r4" in x.title for x in a.attachments)
    assert "not adding a lid sensor" in " ".join(t.messages[2].body)
    assert "new STOP" in " ".join(t.messages[3].body)
    assert "within 10 ms" in " ".join(t.messages[3].body)


def test_interlock_drawing_inhibits_and_latches_not_only_reports_status() -> None:
    text = (ROOT / "latex/correspondence/bench-harness.tex").read_text()
    for term in (
        "AND JAR\\_OK AND JAR\\_PERMIT",
        "PB3 / PB4",
        "opening dominates",
        "20 ms",
        "within 1 ms",
    ):
        assert term in text
