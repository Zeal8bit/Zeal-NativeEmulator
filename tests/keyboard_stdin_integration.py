#!/usr/bin/env python3

"""Process-level tests for host stdin feeding the emulated PS/2 keyboard."""

import fcntl
import os
import pty
import signal
import subprocess
import sys
import termios
import time


def command(executable, rom, ticks):
    return [
        executable,
        "--config",
        "/dev/null",
        "--rom",
        rom,
        "--headless",
        str(ticks),
        "--stdin",
    ]


def wait_for_raw_mode(slave):
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        settings = termios.tcgetattr(slave)
        if not settings[3] & (termios.ICANON | termios.ECHO):
            return settings
        time.sleep(0.01)
    raise AssertionError("emulator did not configure its controlling terminal")


def controlling_terminal():
    os.setsid()
    fcntl.ioctl(0, termios.TIOCSCTTY, 0)


def test_pty(executable, rom):
    master, slave = pty.openpty()
    original_termios = termios.tcgetattr(slave)
    os.set_blocking(master, False)

    process = subprocess.Popen(
        command(executable, rom, 2_000_000_000),
        stdin=slave,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        preexec_fn=controlling_terminal,
    )
    try:
        active = wait_for_raw_mode(slave)
        assert not active[3] & termios.ICANON
        assert not active[3] & termios.ECHO
        assert active[3] & termios.ISIG == original_termios[3] & termios.ISIG

        # Printable input is available immediately. Split an ANSI key sequence
        # across writes to exercise nonblocking parser state.
        os.write(master, b"Z")
        os.write(master, b"\x1b")
        time.sleep(0.01)
        os.write(master, b"[")
        time.sleep(0.01)
        os.write(master, b"A")
        time.sleep(0.05)
        try:
            echoed = os.read(master, 1024)
        except BlockingIOError:
            echoed = b""
        assert b"Z" not in echoed
        assert b"[" not in echoed

        # ISIG must remain enabled: this becomes SIGINT, never guest byte 0x03.
        os.write(master, b"\x03")
        assert process.wait(timeout=5) == 0

        # macOS revokes the controlling slave when its session leader exits,
        # so post-exit tcgetattr is not portable here. The companion C PTY
        # test verifies exact termios/flags restoration before closing it.
    finally:
        if process.poll() is None:
            process.send_signal(signal.SIGKILL)
            process.wait()
        os.close(master)
        os.close(slave)


def test_pipe(executable, rom):
    process = subprocess.Popen(
        command(executable, rom, 150_000_000),
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    assert process.stdin is not None
    assert process.stdout is not None

    # Delayed chunks include a command, split ANSI input, and enough bytes to
    # force the PS/2 FIFO to drain while the producer remains nonblocking.
    for chunk in (b"l", b"s", b"\r", b"\x1b", b"[", b"A", b"x" * 2048):
        process.stdin.write(chunk)
        process.stdin.flush()
        time.sleep(0.005)
    process.stdin.close()

    # EOF must not terminate emulation. Existing tick limit remains owner of
    # process lifetime.
    time.sleep(0.02)
    assert process.poll() is None
    output = process.stdout.read()
    assert process.wait(timeout=5) == 0
    assert b"[ZEAL] Ran for" in output


def main():
    if len(sys.argv) != 4 or sys.argv[1] not in ("pty", "pipe"):
        raise SystemExit(f"usage: {sys.argv[0]} pty|pipe EMULATOR ROM")
    if sys.argv[1] == "pty":
        test_pty(sys.argv[2], sys.argv[3])
    else:
        test_pipe(sys.argv[2], sys.argv[3])


if __name__ == "__main__":
    main()
