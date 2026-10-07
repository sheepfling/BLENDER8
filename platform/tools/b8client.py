"""Local JSON-lines transport for the native B8 process. Python 3.12+, standard library only."""

from __future__ import annotations

import json
import queue
import subprocess
import threading
from collections import deque
from collections.abc import Sequence
from pathlib import Path
from typing import Any, Self, cast


class EngineError(RuntimeError):
    """A rejected command or an explicitly reported simulator/firmware failure."""


class HostTimeout(EngineError):
    """The native process stopped responding; this is NOT an emulated WDT reset."""


class Engine:
    def __init__(
        self,
        executable: Path,
        *,
        bench: bool = False,
        legacy: bool = False,
        timeout: float = 10.0,
        bus_trace: Path | None = None,
        arguments: Sequence[str] = (),
    ) -> None:
        if not executable.is_file():
            raise FileNotFoundError(f"Build the emulator first: {executable}")
        if legacy and not bench:
            raise ValueError("legacy profile is bench-only")
        if timeout <= 0:
            raise ValueError("timeout must be positive")
        self.timeout = timeout
        self.transcript: list[dict[str, Any]] = []
        self.stderr: deque[str] = deque(maxlen=128)
        self._replies: queue.Queue[dict[str, Any] | BaseException] = queue.Queue()
        self._lock = threading.RLock()
        args = [str(executable.resolve()), *arguments]
        if bench:
            args.append("--bench")
        if legacy:
            args.append("--legacy02")
        if bus_trace:
            args += ["--bus-trace", str(bus_trace)]
        self.process = subprocess.Popen(
            args,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            bufsize=1,
        )
        self._reader = threading.Thread(target=self._read_stdout, daemon=True)
        self._error_reader = threading.Thread(target=self._read_stderr, daemon=True)
        self._reader.start()
        self._error_reader.start()
        try:
            self.hello = self._receive()
            if self.hello.get("protocol") != 1:
                raise EngineError("unsupported emulator protocol")
        except BaseException:
            self.close()
            raise

    def _read_stdout(self) -> None:
        assert self.process.stdout is not None
        try:
            while True:
                line = self.process.stdout.readline(32 * 1024 * 1024 + 1)
                if not line:
                    raise EngineError("emulator exited; inspect stderr")
                if len(line) > 32 * 1024 * 1024 or not line.endswith("\n"):
                    raise EngineError("oversized or unterminated emulator response")
                data = json.loads(line)
                if not isinstance(data, dict):
                    raise EngineError("emulator returned a non-object")
                self._replies.put(cast(dict[str, Any], data))
        except BaseException as exc:
            self._replies.put(exc)

    def _read_stderr(self) -> None:
        assert self.process.stderr is not None
        for line in self.process.stderr:
            self.stderr.append(line.rstrip())

    def _receive(self) -> dict[str, Any]:
        try:
            result = self._replies.get(timeout=self.timeout)
        except queue.Empty as exc:
            self.process.kill()
            self.process.wait(timeout=2)
            raise HostTimeout(
                "HOST_CALLBACK_TIMEOUT: no native response; process killed, "
                "not a simulated watchdog reset"
            ) from exc
        if isinstance(result, BaseException):
            raise EngineError(f"{result}\n" + "\n".join(self.stderr)) from result
        if not result.get("ok"):
            raise EngineError(str(result.get("error", "unknown engine error")))
        return result

    def command(self, command: str) -> dict[str, Any]:
        if not command or len(command) > 4096 or "\n" in command or "\r" in command:
            raise ValueError("one nonempty command line, at most 4096 characters")
        with self._lock:
            if self.process.poll() is not None:
                raise EngineError("emulator process is not running")
            assert self.process.stdin is not None
            self.process.stdin.write(command + "\n")
            self.process.stdin.flush()
            result = self._receive()
            self.transcript.append(
                {"command": command, "time_us": result.get("state", {}).get("time_us")}
            )
            return result

    def state(self) -> dict[str, Any]:
        return self.command("snapshot")["state"]

    def run(self, microseconds: int) -> dict[str, Any]:
        if microseconds < 0:
            raise ValueError("negative duration")
        return self.command(f"run {microseconds}")["state"]

    def trace(self, microseconds: int, sample_us: int = 1000) -> dict[str, Any]:
        return self.command(f"trace {microseconds} {sample_us}")

    def close(self) -> None:
        if getattr(self, "process", None) is None:
            return
        if self.process.poll() is None:
            try:
                assert self.process.stdin is not None
                self.process.stdin.write("quit\n")
                self.process.stdin.flush()
                self.process.wait(timeout=1)
            except (OSError, subprocess.TimeoutExpired):
                self.process.kill()
                self.process.wait(timeout=2)
        self._reader.join(timeout=1)
        self._error_reader.join(timeout=1)
        for stream in (self.process.stdin, self.process.stdout, self.process.stderr):
            if stream:
                stream.close()

    def __enter__(self) -> Self:
        return self

    def __exit__(self, *args: object) -> None:
        self.close()


def resolve_executable(build: Path, name: str = "b8_emulator") -> Path:
    for folder in (build, build / "Release", build / "Debug"):
        for suffix in ("", ".exe"):
            path = folder / (name + suffix)
            if path.is_file():
                return path
    raise FileNotFoundError(f"Cannot find {name} under {build}. Run the build command first.")
