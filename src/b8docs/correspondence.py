"""Build the customer mail file from a validated source; no mail service is contacted."""

from __future__ import annotations

import json
import re
from datetime import datetime
from pathlib import Path
from textwrap import fill
from typing import Literal

from pydantic import BaseModel, ConfigDict, Field, model_validator


class Record(BaseModel):
    model_config = ConfigDict(extra="forbid", frozen=True)


class Person(Record):
    name: str
    organization: str
    email: str
    signature: list[str] = Field(default_factory=list)


class Message(Record):
    id: str
    author: Literal["rick", "mara"]
    to: list[Literal["rick", "mara", "team"]]
    date: datetime
    subject: str
    body: list[str] = Field(min_length=1)
    in_reply_to: str | None = None
    sketch: (
        Literal["pulse", "stop", "motion", "thermal", "clock", "supervision", "interlock"] | None
    ) = None
    attachments: list[str] = Field(default_factory=list)

    @model_validator(mode="after")
    def aware_timestamp(self) -> Message:
        if self.date.tzinfo is None or self.date.utcoffset() is None:
            raise ValueError("A message needs an explicit timezone.")
        return self


class Thread(Record):
    id: str
    subject: str
    messages: list[Message] = Field(min_length=5, max_length=5)

    @model_validator(mode="after")
    def check_exchange(self) -> Thread:
        if [m.author for m in self.messages] != ["mara", "rick", "mara", "rick", "mara"]:
            raise ValueError("Exchange must be customer, Rick, customer, Rick, customer approval.")
        if self.messages[1].sketch is None:
            raise ValueError("Rick's clarification message must reference a sketch.")
        for i, msg in enumerate(self.messages):
            expected = ["rick"] if msg.author == "mara" else ["mara"]
            if msg.to != expected:
                raise ValueError("Customer correspondence is between Mara and Rick.")
            previous = self.messages[i - 1] if i else None
            if msg.in_reply_to != (previous.id if previous else None):
                raise ValueError("Reply chain must follow the immediately preceding message.")
            if previous and msg.date <= previous.date:
                raise ValueError("Reply timestamps must increase.")
            subject = ("RE: " if previous else "") + self.subject
            if msg.author == "mara":
                subject = "[EXTERNAL] " + subject
            if msg.subject != subject:
                raise ValueError(
                    "Subject must retain the thread, reply prefix and incoming marker."
                )
        return self


class Attachment(Record):
    id: str
    path: str
    title: str
    parent_message: str


class Archive(Record):
    edition: str
    organization: Literal["Half-A/Labs"]
    people: dict[str, Person]
    forward: Message
    threads: list[Thread]
    attachments: list[Attachment]

    @model_validator(mode="after")
    def check_identity(self) -> Archive:
        if self.people["team"].name != "Firmware team":
            raise ValueError("Rick hands the file to the firmware team.")
        if self.people["rick"].organization != "Half-A/Labs":
            raise ValueError("Rick represents Half-A/Labs.")
        for author in ("rick", "mara"):
            person = self.people[author]
            if person.organization not in "\n".join(person.signature):
                raise ValueError("Author signatures must identify their organization.")
        messages = [m for t in self.threads for m in t.messages]
        ids = [m.id for m in messages] + [self.forward.id]
        if len(ids) != len(set(ids)):
            raise ValueError("Duplicate message identifier.")
        if self.forward.author != "rick" or self.forward.to != ["team"]:
            raise ValueError("Final handoff must be Rick to the firmware team.")
        if any(m.date >= self.forward.date for m in messages):
            raise ValueError("Handoff must follow the archived correspondence.")
        for attachment in self.attachments:
            if attachment.parent_message not in ids:
                raise ValueError("Attachment has no owning message.")
            p = Path(attachment.path)
            if p.is_absolute() or ".." in p.parts:
                raise ValueError("Attachment paths must stay inside the source tree.")
        for msg in [*messages, self.forward]:
            if re.search(
                r"\b(?:Jordon|Jordan|New Employee|HalfALabs)\b",
                " ".join(msg.body) + msg.subject,
                re.IGNORECASE,
            ):
                raise ValueError("Retired name or wordmark in the mail text.")
        return self


def load_archive(root: Path) -> Archive:
    return Archive.model_validate_json(
        (root / "internal/authoring/correspondence/archive.json").read_text()
    )


def tex_escape(text: str) -> str:
    replacements = {
        "\\": r"\textbackslash{}",
        "&": r"\&",
        "%": r"\%",
        "$": r"\$",
        "#": r"\#",
        "_": r"\_",
        "{": r"\{",
        "}": r"\}",
        "~": r"\textasciitilde{}",
        "^": r"\textasciicircum{}",
        "\n": r"\\ ",
    }
    return "".join(replacements.get(c, c) for c in text)


def party(archive: Archive, key: str) -> str:
    p = archive.people[key]
    return f"{p.name} / {p.organization}"


SKETCH_TITLES = {
    "pulse": "PULSE boost and return to the selected speed",
    "stop": "STOP release and a fresh command",
    "motion": "Run-up and missing-motion checks",
    "thermal": "Cooling and deliberate recovery",
    "clock": "Clock qualification before drive",
    "supervision": "Useful progress before timer renewal",
    "interlock": "Jug permission and deliberate restart",
}


def message_tex(archive: Archive, msg: Message) -> str:
    when = msg.date.strftime("%d %b %Y, %I:%M %p").replace(", 0", ", ")
    result = (
        r"\begin{Mail}{"
        + tex_escape(party(archive, msg.author))
        + "}{"
        + tex_escape("; ".join(party(archive, key) for key in msg.to))
        + "}{"
        + when
        + "}{"
        + tex_escape(msg.subject)
        + "}\n"
    )
    result += "\n\n".join(tex_escape(p) for p in msg.body) + "\n"
    if msg.sketch:
        result += r"\MailSketch{" + msg.sketch + "}{" + SKETCH_TITLES[msg.sketch] + "}\n"
    result += (
        r"\MailSignature{"
        + msg.author
        + "}{"
        + r"\\ ".join(tex_escape(line) for line in archive.people[msg.author].signature)
        + "}\n"
    )
    result += "\\end{Mail}\n"
    return result


def generate_correspondence(root: Path) -> dict[str, int]:
    archive = load_archive(root)
    out = root / "generated"
    out.mkdir(exist_ok=True)
    content = r"\pdfbookmark[1]{Rick's handoff to the firmware team}{handoff}" + "\n"
    content += r"{\Large\bfseries " + tex_escape(archive.forward.subject) + "\\par}\n"
    content += message_tex(archive, archive.forward)
    content += (
        r"\vspace{5mm}{\small\textbf{Enclosed:} Seven mail threads, "
        r"the two revised chassis drawings, "
        r"and the component papers.}\par"
        "\n"
    )
    for thread in archive.threads:
        content += r"\ThreadTitle{" + tex_escape(thread.subject) + "}{" + thread.id + "}\n"
        for msg in thread.messages:
            content += message_tex(archive, msg)
    for a in archive.attachments:
        if not (root / a.path).is_file():
            raise ValueError(f"Missing retained attachment: {a.path}")
        content += r"\input{" + a.path + "}\n"
    (out / "correspondence-body.tex").write_text(content, encoding="utf-8")

    lines = ["# " + archive.forward.subject, ""]
    for thread in [None, *archive.threads]:
        if thread:
            lines.extend(["## " + thread.subject, ""])
        for msg in thread.messages if thread else [archive.forward]:
            lines.extend(
                [
                    "**From:** " + party(archive, msg.author) + "  ",
                    "**To:** " + "; ".join(party(archive, key) for key in msg.to) + "  ",
                    "**Date:** " + msg.date.isoformat() + "  ",
                    "**Subject:** " + msg.subject,
                    "",
                    *[fill(p, width=96) + "\n" for p in msg.body],
                ]
            )
            if msg.sketch:
                lines.append(
                    f"[Sketch: {SKETCH_TITLES[msg.sketch]}; retained in the PDF edition.]\n"
                )
            person = archive.people[msg.author]
            signature = [
                line.replace(person.email, f"<{person.email}>") for line in person.signature
            ]
            if msg.author == "mara":
                signature[0] = "![Kestrel K logo](assets/kestrel-mark.svg) " + signature[0]
            lines.extend(["  \n".join(signature), ""])
            lines.extend(["---", ""])
    (out / "customer-correspondence.md").write_text("\n".join(lines), encoding="utf-8")
    count = sum(len(t.messages) for t in archive.threads) + 1
    report = {
        "threads": len(archive.threads),
        "messages": count,
        "attachments": len(archive.attachments),
    }
    (out / "correspondence-check.json").write_text(json.dumps(report, indent=2) + "\n")
    return report
