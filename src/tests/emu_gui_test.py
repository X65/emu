#!/usr/bin/env python3
"""Drive the native emulator window through X11 and verify observable state."""

from __future__ import annotations

import argparse
import base64
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import time
from typing import Callable, TypeVar


WINDOW_TITLE = "X65 emu"
JOYSTICK_ADDR = 0x0301
MARKER_ADDR = 0x0300

# Openbox publishes _NET_SUPPORTING_WM_CHECK before it can manage anything, so
# a slow start shows up here rather than in _wm_ready: the emulator maps its
# window, Openbox holds the redirected MapRequest, and nothing is visible yet.
WINDOW_TIMEOUT = 30.0


class TestFailure(RuntimeError):
    pass


T = TypeVar("T")


def wait_until(description: str, predicate: Callable[[], T | None], timeout: float = 8.0) -> T:
    deadline = time.monotonic() + timeout
    last_error: Exception | None = None
    while time.monotonic() < deadline:
        try:
            value = predicate()
            if value:
                return value
        except (OSError, subprocess.SubprocessError, TestFailure) as error:
            last_error = error
        time.sleep(0.05)
    detail = f": {last_error}" if last_error else ""
    raise TestFailure(f"timed out waiting for {description}{detail}")


class DapClient:
    def __init__(self, port: int):
        self.socket = socket.create_connection(("127.0.0.1", port), timeout=3)
        self.socket.settimeout(3)
        self.buffer = bytearray()
        self.sequence = 0

    def close(self) -> None:
        self.socket.close()

    def _receive(self) -> dict[str, object]:
        while b"\r\n\r\n" not in self.buffer:
            chunk = self.socket.recv(4096)
            if not chunk:
                raise TestFailure("DAP connection closed while reading headers")
            self.buffer.extend(chunk)
        header, _, remaining = self.buffer.partition(b"\r\n\r\n")
        self.buffer = remaining
        lengths = [line for line in header.split(b"\r\n") if line.lower().startswith(b"content-length:")]
        if len(lengths) != 1:
            raise TestFailure(f"invalid DAP header: {header!r}")
        length = int(lengths[0].split(b":", 1)[1].strip())
        while len(self.buffer) < length:
            chunk = self.socket.recv(4096)
            if not chunk:
                raise TestFailure("DAP connection closed while reading payload")
            self.buffer.extend(chunk)
        payload = bytes(self.buffer[:length])
        del self.buffer[:length]
        return json.loads(payload)

    def request(self, command: str, arguments: dict[str, object]) -> dict[str, object]:
        self.sequence += 1
        request = {
            "seq": self.sequence,
            "type": "request",
            "command": command,
            "arguments": arguments,
        }
        payload = json.dumps(request, separators=(",", ":")).encode()
        self.socket.sendall(f"Content-Length: {len(payload)}\r\n\r\n".encode() + payload)
        while True:
            message = self._receive()
            if message.get("type") == "response" and message.get("request_seq") == self.sequence:
                if not message.get("success"):
                    raise TestFailure(f"DAP {command} failed: {message}")
                return message

    def read_byte(self, address: int) -> int:
        response = self.request(
            "readMemory",
            {"memoryReference": f"0x{address:06X}", "count": 1},
        )
        body = response.get("body")
        if not isinstance(body, dict) or not isinstance(body.get("data"), str):
            raise TestFailure(f"DAP readMemory returned no data: {response}")
        data = base64.b64decode(body["data"], validate=True)
        if len(data) != 1:
            raise TestFailure(f"DAP readMemory returned {len(data)} bytes")
        return data[0]


class GuiTest:
    def __init__(self, args: argparse.Namespace):
        self.args = args
        self.test_dir = Path(args.test_dir)
        self.test_dir.mkdir(parents=True, exist_ok=True)
        config_dir = Path(os.environ["XDG_CONFIG_HOME"])
        if config_dir.exists():
            shutil.rmtree(config_dir)
        config_dir.mkdir(parents=True)
        self.emu_log_path = self.test_dir / "emu.log"
        self.wm_log_path = self.test_dir / "window-manager.log"
        self.emu_log = self.emu_log_path.open("w", encoding="utf-8")
        self.wm_log = self.wm_log_path.open("w", encoding="utf-8")
        self.wm: subprocess.Popen[bytes] | None = None
        self.emu: subprocess.Popen[bytes] | None = None
        self.window: str | None = None

    def run_tool(self, tool: str, *arguments: str, check: bool = True) -> str | None:
        # Returns None instead of raising when a probing call exits non-zero.
        result = subprocess.run(
            [tool, *arguments],
            check=check,
            capture_output=True,
            text=True,
            timeout=3,
        )
        return result.stdout.strip() if result.returncode == 0 else None

    def xdotool(self, *arguments: str) -> str:
        output = self.run_tool(self.args.xdotool, *arguments)
        assert output is not None
        return output

    def start(self, extra_arguments: list[str]) -> None:
        if self.args.openbox:
            if not self.args.xprop:
                raise TestFailure("xprop is required when a window manager is used")
            self.wm = subprocess.Popen(
                [self.args.openbox, "--sm-disable"],
                stdout=self.wm_log,
                stderr=subprocess.STDOUT,
                env=self._wm_environment(),
            )
            wait_until("Openbox to publish its supporting-window property", self._wm_ready)
        self.emu = subprocess.Popen(
            [
                self.args.emu,
                "--zero-mem",
                "--seed",
                "1",
                *extra_arguments,
                self.args.fixture,
            ],
            stdout=self.emu_log,
            stderr=subprocess.STDOUT,
        )
        self.window = wait_until("the emulator window", self._find_window, WINDOW_TIMEOUT)
        name = self.xdotool("getwindowname", self.window)
        if name != WINDOW_TITLE:
            raise TestFailure(f"window title is {name!r}, expected {WINDOW_TITLE!r}")
        self.focus()

    def _wm_environment(self) -> dict[str, str]:
        # Openbox loads its theme -- and through it Pango and fontconfig -- only
        # after it has taken over the screen, so building a cold fontconfig cache
        # (a fresh runner, or a font package installed by the CI job itself) can
        # keep it out of its event loop for tens of seconds while the emulator
        # window waits to be mapped. Nothing here reads a titlebar, so hand it a
        # font set with no directories in it: nothing to scan, nothing to cache.
        cache_dir = self.test_dir / "fontconfig"
        cache_dir.mkdir(parents=True, exist_ok=True)
        config = self.test_dir / "fonts.conf"
        config.write_text(
            '<?xml version="1.0"?>\n'
            '<!DOCTYPE fontconfig SYSTEM "fonts.dtd">\n'
            f"<fontconfig><cachedir>{cache_dir}</cachedir></fontconfig>\n",
            encoding="utf-8",
        )
        return dict(os.environ, FONTCONFIG_FILE=str(config))

    def _wm_ready(self) -> bool:
        assert self.wm
        if self.wm.poll() is not None:
            raise TestFailure(f"Openbox exited with status {self.wm.returncode}")
        check = self.run_tool(self.args.xprop, "-root", "_NET_SUPPORTING_WM_CHECK", check=False)
        return check is not None and "window id" in check

    def _find_window(self) -> str | None:
        assert self.emu
        if self.emu.poll() is not None:
            raise TestFailure(f"emulator exited with status {self.emu.returncode}")
        found = self.run_tool(
            self.args.xdotool, "search", "--onlyvisible",
            "--pid", str(self.emu.pid),
            "--name", f"^{WINDOW_TITLE}$",
            check=False,
        )
        windows = found.split() if found else []
        return windows[0] if windows else None

    def geometry(self) -> tuple[int, int, int, int]:
        assert self.window
        values: dict[str, int] = {}
        for line in self.xdotool("getwindowgeometry", "--shell", self.window).splitlines():
            key, separator, value = line.partition("=")
            if separator and key in {"X", "Y", "WIDTH", "HEIGHT"}:
                values[key] = int(value)
        try:
            return values["X"], values["Y"], values["WIDTH"], values["HEIGHT"]
        except KeyError as error:
            raise TestFailure(f"incomplete window geometry: {values}") from error

    def wait_for_geometry(self, description: str, size: tuple[int, int]) -> tuple[int, int, int, int]:
        def matches() -> tuple[int, int, int, int] | None:
            geometry = self.geometry()
            return geometry if geometry[2:] == size else None

        return wait_until(description, matches)

    def focus(self) -> None:
        assert self.window
        if self.wm:
            self.xdotool("windowactivate", "--sync", self.window)
        else:
            self.xdotool("windowfocus", "--sync", self.window)

    def key(self, keys: str) -> None:
        self.focus()
        self.xdotool("key", "--clearmodifiers", keys)

    def quit(self) -> None:
        # Keep Control held until Q's key-up event: app_input intentionally
        # checks the modifier on key-up before requesting a clean shutdown.
        self.focus()
        self.xdotool("keydown", "ctrl", "key", "q", "keyup", "ctrl")

    def expect_clean_exit(self) -> None:
        assert self.emu
        try:
            status = self.emu.wait(timeout=10)
        except subprocess.TimeoutExpired as error:
            raise TestFailure("emulator did not exit after Ctrl+Q") from error
        if status != 0:
            raise TestFailure(f"emulator exited with status {status}")

    def stop(self) -> None:
        for process in (self.emu, self.wm):
            if process and process.poll() is None:
                process.terminate()
        for process in (self.emu, self.wm):
            if not process:
                continue
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=3)
        self.emu_log.close()
        self.wm_log.close()

    def diagnostics(self) -> str:
        self.emu_log.flush()
        self.wm_log.flush()
        emu = self.emu_log_path.read_text(encoding="utf-8", errors="replace")[-6000:]
        wm = self.wm_log_path.read_text(encoding="utf-8", errors="replace")[-3000:]
        return f"\n--- emulator log ---\n{emu}\n--- window-manager log ---\n{wm}"


def free_tcp_port() -> int:
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        return listener.getsockname()[1]


def connect_dap(port: int) -> DapClient | None:
    try:
        return DapClient(port)
    except OSError:
        return None


def test_window(gui: GuiTest) -> None:
    if not gui.args.openbox:
        raise TestFailure("the window lifecycle test requires Openbox")
    gui.start([])
    display_width, display_height = map(int, gui.xdotool("getdisplaygeometry").split())
    initial = gui.geometry()
    if not (0 < initial[2] < display_width and 0 < initial[3] < display_height):
        raise TestFailure(f"unexpected initial geometry {initial} on {display_width}x{display_height}")

    gui.key("alt+Return")
    fullscreen = gui.wait_for_geometry("fullscreen geometry", (display_width, display_height))
    if fullscreen[:2] != (0, 0):
        raise TestFailure(f"fullscreen window is not at the display origin: {fullscreen}")

    gui.key("alt+Return")
    gui.wait_for_geometry("restored window geometry", initial[2:])

    gui.key("ctrl+shift+h")
    gui.key("ctrl+shift+h")
    gui.quit()
    gui.expect_clean_exit()
    gui.emu_log.flush()
    log = gui.emu_log_path.read_text(encoding="utf-8", errors="replace")
    if log.count("Debug UI hidden") != 1 or log.count("Debug UI shown") != 1:
        raise TestFailure("Ctrl+Shift+H did not hide and restore the debug UI exactly once")


def test_joystick(gui: GuiTest) -> None:
    port = free_tcp_port()
    gui.start(["--disable-gui", "--joystick=digital_1", "--dap-port", str(port)])
    client = wait_until("the DAP listener", lambda: connect_dap(port))
    try:
        def reads(address: int, expected: int) -> Callable[[], bool]:
            return lambda: client.read_byte(address) == expected

        wait_until("the fixture marker", reads(MARKER_ADDR, 0xA5))
        wait_until("released joystick state", reads(JOYSTICK_ADDR, 0xFF))
        gui.xdotool("keydown", "w", "a", "z")
        wait_until("up+left+A joystick state", reads(JOYSTICK_ADDR, 0xDA))
        gui.xdotool("keyup", "z", "a", "w")
        wait_until("released joystick state", reads(JOYSTICK_ADDR, 0xFF))
        client.request("disconnect", {})
    finally:
        client.close()
    gui.quit()
    gui.expect_clean_exit()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--kind", choices=("window", "joystick"), required=True)
    parser.add_argument("--emu", required=True)
    parser.add_argument("--fixture", required=True)
    parser.add_argument("--xdotool", required=True)
    parser.add_argument("--xprop")
    parser.add_argument("--openbox")
    parser.add_argument("--test-dir", required=True)
    args = parser.parse_args()
    gui = GuiTest(args)
    try:
        if args.kind == "window":
            test_window(gui)
        else:
            test_joystick(gui)
        return 0
    except Exception as error:
        print(f"GUI test failed: {error}{gui.diagnostics()}", file=sys.stderr)
        return 1
    finally:
        gui.stop()


if __name__ == "__main__":
    raise SystemExit(main())
