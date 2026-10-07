from __future__ import annotations

import json
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path.cwd().resolve()
CLANG = shutil.which("clang++")


def check(tmp_path: Path, code: str, profile: str = "B8") -> dict[str, object]:
    if CLANG is None:
        pytest.skip("LLVM clang++ is required for semantic numeric-policy tests")
    (tmp_path / "firmware.cpp").write_text(code)
    result = subprocess.run(
        [
            sys.executable,
            str(ROOT / "platform/tools/check_numeric.py"),
            "--root",
            str(ROOT / "platform"),
            "--firmware",
            str(tmp_path),
            "--profile",
            profile,
            "--clang",
            CLANG,
            "--json",
        ],
        capture_output=True,
        text=True,
        timeout=90,
        check=False,
    )
    assert result.returncode in (0, 1), result.stderr
    return json.loads(result.stdout)


@pytest.mark.parametrize("profile", ["B8", "B16"])
@pytest.mark.parametrize(
    "code",
    [
        "#include <cstdint>\nstd::uint32_t counter;",
        "#include <cstdint>\nstd::uint64_t counter;",
        "using Hidden = double; Hidden sample;",
        "long double sample;",
        "auto sample = 1.0;",
        "#include <array>\nstd::array<double, 2> samples;",
        "#include <cstdint>\nstd::uint8_t a, b; auto f(){return a+b;}",
        "#include <cstdint>\nstd::uint8_t a, b; void f(){a+=b;}",
        "#include <cstdint>\nstd::uint8_t a, b; auto f(){return static_cast<std::uint8_t>(a*b);}",
    ],
)
def test_rejects_unsupported_types_and_promoted_intermediates(
    tmp_path: Path,
    profile: str,
    code: str,
) -> None:
    result = check(tmp_path, code, profile)
    assert result["passed"] is False
    assert result["issues"]


def test_local_headers_and_macro_expansions_are_checked(tmp_path: Path) -> None:
    (tmp_path / "local.hpp").write_text("#define VALUE 1.0f\nusing Hidden = double;\n")
    result = check(tmp_path, '#include "local.hpp"\nauto sample = VALUE;\n')
    assert result["passed"] is False
    assert "local.hpp" in json.dumps(result) and "firmware.cpp" in json.dumps(result)


def test_b8_rejects_words_and_float(tmp_path: Path) -> None:
    result = check(tmp_path, "#include <cstdint>\nstd::uint16_t word; float sample=1.0f;")
    assert result["passed"] is False


def test_b16_accepts_words_binary32_and_width_preserving_math(tmp_path: Path) -> None:
    code = """
#include "blender8/numeric.hpp"
std::uint16_t a=60000, b=3;
float sample=1.25f;
void step(){ a=b8::numeric::add(a,b); sample=sample*2.0f; }
"""
    assert check(tmp_path, code, "B16")["passed"] is True


def test_b8_accepts_byte_helpers_and_byte_arrays(tmp_path: Path) -> None:
    code = """
#include "blender8/numeric.hpp"
std::uint8_t a=250, b=10, storage[512]{};
void step(){ a=b8::numeric::add(a,b); storage[a]=b; }
"""
    assert check(tmp_path, code)["passed"] is True


def test_missing_compiler_fails_closed(tmp_path: Path) -> None:
    result = subprocess.run(
        [
            sys.executable,
            str(ROOT / "platform/tools/check_numeric.py"),
            "--root",
            str(ROOT / "platform"),
            "--clang",
            str(tmp_path / "missing-compiler"),
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 2


def test_cmake_enforces_and_selects_the_numeric_profile(tmp_path: Path) -> None:
    cmake = shutil.which("cmake")
    if CLANG is None or cmake is None:
        pytest.skip("Clang and CMake are required for the build-gate test")
    firmware = tmp_path / "firmware"
    firmware.mkdir()
    (firmware / "firmware.cpp").write_text(
        "#include <cstdint>\nstd::uint16_t counter=60000; float coefficient=1.25f;\n"
    )
    for profile, accepted in (("B8", False), ("B16", True)):
        configure = subprocess.run(
            [
                cmake,
                "-S",
                str(ROOT / "platform"),
                "-B",
                str(tmp_path / "build"),
                f"-DB8_FIRMWARE_DIR={firmware}",
                f"-DB8_POLICY_CLANG={CLANG}",
                f"-DB8_NUMERIC_PROFILE={profile}",
                f"-DPython3_EXECUTABLE={sys.executable}",
            ],
            capture_output=True,
            text=True,
            check=False,
            timeout=90,
        )
        assert configure.returncode == 0, configure.stderr
        build = subprocess.run(
            [
                cmake,
                "--build",
                str(tmp_path / "build"),
                "--target",
                "student_firmware",
                "--config",
                "Release",
            ],
            capture_output=True,
            text=True,
            check=False,
            timeout=90,
        )
        assert (build.returncode == 0) is accepted, build.stdout + build.stderr


def test_numeric_helpers_have_defined_boundary_behavior(tmp_path: Path) -> None:
    if CLANG is None:
        pytest.skip("LLVM clang++ is required")
    source = tmp_path / "helpers.cpp"
    source.write_text("""
#include "blender8/numeric.hpp"
using namespace b8::numeric;
static_assert(add(std::uint8_t{255},std::uint8_t{1})==0);
static_assert(subtract(std::uint8_t{0},std::uint8_t{1})==255);
static_assert(multiply(std::uint16_t{65535},std::uint16_t{65535})==1);
static_assert(add(std::int8_t{127},std::int8_t{1})==-128);
static_assert(divide(std::int16_t{-32768},std::int16_t{-1}).overflow);
static_assert(divide(std::int8_t{-7},std::int8_t{3}).quotient==-2);
static_assert(divide(std::int8_t{-7},std::int8_t{3}).remainder==-1);
static_assert(divide(std::uint8_t{1},std::uint8_t{0}).divide_by_zero);
static_assert(shift_left(std::uint8_t{1},8)==0);
static_assert(shift_right(std::int8_t{-1},1)==127);
int main(){}
""")
    result = subprocess.run(
        [
            CLANG,
            "-std=c++20",
            "-fsyntax-only",
            "-DB8_NUMERIC_PROFILE=16",
            "-I",
            str(ROOT / "platform/sdk/include"),
            str(source),
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, result.stderr
