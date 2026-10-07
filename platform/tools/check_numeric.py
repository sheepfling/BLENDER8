"""Check firmware scalar types and expression widths using Clang's semantic AST.

This is a source-policy gate, not an instruction emulator or a security sandbox.
The SDK and system headers are trusted implementations. Firmware headers are checked.
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any, cast

Node = dict[str, Any]
BUILTINS = re.compile(
    r"\b(?:long double|unsigned long long(?: int)?|long long(?: int)?|"
    r"unsigned long(?: int)?|long(?: int)?|unsigned short(?: int)?|short(?: int)?|"
    r"unsigned char|signed char|unsigned(?: int)?|signed(?: int)?|"
    r"__int128|__float128|__ibm128|_Float128|_Float64|_Float32|_Float16|__fp16|__bf16|"
    r"float|double|int|char32_t|char16_t|wchar_t|char8_t|char)\b"
)
TYPED_KINDS = {
    "VarDecl",
    "FieldDecl",
    "ParmVarDecl",
    "FunctionDecl",
    "CXXMethodDecl",
    "TypedefDecl",
    "TypeAliasDecl",
    "BinaryOperator",
    "CompoundAssignOperator",
    "UnaryOperator",
    "FloatingLiteral",
    "CharacterLiteral",
}


@dataclass(frozen=True)
class Issue:
    file: str
    line: int
    kind: str
    reason: str


def children(node: Node) -> list[Node]:
    return cast(list[Node], node.get("inner", []))


def location(node: Node) -> Node:
    value = cast(Node, node.get("loc") or node.get("range", {}).get("begin", {}))
    return cast(Node, value.get("expansionLoc", value))


def scalar_type(value: Node) -> str:
    return str(value.get("desugaredQualType", value.get("qualType", "")))


def compiler_macros(compiler: str) -> dict[str, int]:
    result = subprocess.run(
        [compiler, "-dM", "-E", "-x", "c++", "-"],
        input="",
        text=True,
        capture_output=True,
        check=True,
        timeout=30,
    )
    return {
        key: int(value) for key, value in re.findall(r"^#define (\w+) (\d+)$", result.stdout, re.M)
    }


def aliases_in(node: Node, aliases: dict[str, set[str]]) -> None:
    if node.get("kind") in {"TypedefDecl", "TypeAliasDecl"}:
        name, value = str(node.get("name", "")), scalar_type(node.get("type", {}))
        if name and value and name != value:
            aliases.setdefault(name, set()).add(value)
    for child in children(node):
        aliases_in(child, aliases)


def type_problem(
    value: str,
    profile: str,
    macros: dict[str, int],
    aliases: dict[str, set[str]],
    seen: frozenset[str] = frozenset(),
) -> str | None:
    maximum = 8 if profile == "B8" else 16
    if any(token in value for token in ("_BitInt", "_ExtInt", "__vector", "_Complex")):
        return f"extended arithmetic type is outside {profile}: {value}"
    for match in BUILTINS.finditer(value):
        name = match.group()
        if name in {
            "double",
            "long double",
            "__float128",
            "__ibm128",
            "_Float128",
            "_Float64",
            "_Float32",
            "_Float16",
            "__fp16",
            "__bf16",
        }:
            return f"{name} is forbidden in {profile}"
        if name == "float":
            if profile == "B8":
                return "floating-point data/operations are forbidden in B8"
            continue
        width = 128 if name == "__int128" else 0
        if "char" in name and name not in {"wchar_t", "char16_t", "char32_t"}:
            width = macros["__CHAR_BIT__"]
        elif name in {"char16_t", "char32_t"}:
            width = int(name[4:6])
        elif name == "wchar_t":
            width = macros["__SIZEOF_WCHAR_T__"] * macros["__CHAR_BIT__"]
        elif width == 0:
            size = (
                "LONG_LONG"
                if "long long" in name
                else ("LONG" if "long" in name else "SHORT" if "short" in name else "INT")
            )
            width = macros[f"__SIZEOF_{size}__"] * macros["__CHAR_BIT__"]
        if width > maximum:
            return f"{name} is {width}-bit; {profile} permits at most {maximum}-bit integers"
    for token in re.findall(r"\b[A-Za-z_]\w*\b", value):
        if token not in seen and len(aliases.get(token, set())) == 1:
            canonical = next(iter(aliases[token]))
            if problem := type_problem(canonical, profile, macros, aliases, seen | {token}):
                return problem
    return None


def literal_expression(node: Node) -> bool:
    if node.get("kind") == "IntegerLiteral":
        return True
    return (
        node.get("kind") in {"ImplicitCastExpr", "ParenExpr", "UnaryOperator"}
        and node.get("opcode", "+") in {"+", "-"}
        and len(children(node)) == 1
        and literal_expression(children(node)[0])
    )


def inspect_ast(ast: Node, firmware: Path, profile: str, macros: dict[str, int]) -> list[Issue]:
    aliases: dict[str, set[str]] = {}
    aliases_in(ast, aliases)
    issues: set[Issue] = set()
    last_file: Path | None = None
    source_lines: dict[Path, str] = {}

    def visit(node: Node, inherited_file: Path | None = None) -> None:
        nonlocal last_file
        loc = location(node)
        if "file" in loc:
            last_file = Path(str(loc["file"])).resolve()
        file = last_file or inherited_file
        kind = str(node.get("kind", ""))
        owned = file is not None and file.is_relative_to(firmware)
        if owned:
            assert file is not None
            if file not in source_lines:
                source_lines[file] = file.read_text(encoding="utf-8")
            text = source_lines[file]
            line = int(loc.get("line", text.count("\n", 0, int(loc.get("offset", 0))) + 1))

            def report(reason: str) -> None:
                issues.add(Issue(str(file), line, kind, reason))

            if kind in {"GCCAsmStmt", "MSAsmStmt"}:
                report("inline assembly is outside the checked firmware subset")
            if kind == "IntegerLiteral":
                limit = 255 if profile == "B8" else 65535
                if int(node.get("value", 0)) > limit:
                    report(f"integer literal exceeds {profile}'s {limit} maximum")
            elif not literal_expression(node) and (kind in TYPED_KINDS or kind.endswith("Expr")):
                for key in ("type", "computeLHSType", "computeResultType"):
                    value = scalar_type(node.get(key, {}))
                    if problem := type_problem(value, profile, macros, aliases):
                        report(f"{problem} ({value}); use width-preserving numeric helpers")
            if kind == "EnumDecl":
                value = scalar_type(node.get("fixedUnderlyingType", {"qualType": "int"}))
                if problem := type_problem(value, profile, macros, aliases):
                    report(problem)
        # Template extents and static assertions are compile-time infrastructure.
        if kind not in {"StaticAssertDecl", "TemplateArgument"}:
            for child in children(node):
                visit(child, file)

    visit(ast)
    return sorted(issues, key=lambda item: (item.file, item.line, item.kind, item.reason))


def check_sources(
    root: Path,
    firmware: Path,
    profile: str,
    compiler: str,
    sources: list[Path] | None = None,
) -> list[Issue]:
    macros = compiler_macros(compiler)
    if not (
        macros.get("__SIZEOF_FLOAT__") == 4
        and macros.get("__FLT_MANT_DIG__") == 24
        and macros.get("__FLT_MAX_EXP__") == 128
        and macros.get("__CHAR_BIT__") == 8
    ):
        raise ValueError("The checker needs eight-bit bytes and IEEE binary32 float")
    files = sources if sources is not None else sorted(firmware.glob("*.cpp"))
    if not files:
        raise ValueError(f"No firmware translation units in {firmware}")
    issues: set[Issue] = set()
    for source in files:
        result = subprocess.run(
            [
                compiler,
                "-std=c++20",
                "-fsyntax-only",
                "-Wno-everything",
                "-Xclang",
                "-ast-dump=json",
                f"-DB8_NUMERIC_PROFILE={8 if profile == 'B8' else 16}",
                *(["-DB8_TARGET_B16=1"] if profile == "B16" else []),
                "-I",
                str(root / "sdk/include"),
                "-I",
                str(firmware),
                str(source),
            ],
            capture_output=True,
            text=True,
            check=False,
            timeout=90,
        )
        if result.returncode:
            raise ValueError(f"Clang could not check {source}:\n{result.stderr}")
        ast = cast(Node, json.loads(result.stdout))
        issues.update(inspect_ast(ast, firmware, profile, macros))
    return sorted(issues, key=lambda item: (item.file, item.line, item.kind, item.reason))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd(), help="platform root")
    parser.add_argument("--firmware", type=Path, help="firmware directory, relative to root")
    parser.add_argument("--profile", choices=("B8", "B16"), default="B8")
    parser.add_argument("--clang", default=shutil.which("clang++"), help="Clang C++ executable")
    parser.add_argument("--json", action="store_true", help="machine-readable report")
    args = parser.parse_args()
    root = args.root.resolve()
    firmware = (root / (args.firmware or Path("firmware"))).resolve()
    if not (root / "CMakeLists.txt").is_file():
        parser.error(f"not a B8 platform root: {root}")
    if not args.clang:
        parser.error("LLVM clang++ is required for numeric-policy enforcement")
    try:
        issues = check_sources(root, firmware, args.profile, args.clang)
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(str(error), file=sys.stderr)
        return 2
    if args.json:
        print(
            json.dumps(
                {
                    "profile": args.profile,
                    "passed": not issues,
                    "issues": [asdict(issue) for issue in issues],
                },
                indent=2,
            )
        )
    elif issues:
        for issue in issues[:40]:
            print(f"{issue.file}:{issue.line}: {issue.reason}", file=sys.stderr)
    else:
        print(f"{args.profile} firmware numeric policy passed.")
    return int(bool(issues))


if __name__ == "__main__":
    raise SystemExit(main())
