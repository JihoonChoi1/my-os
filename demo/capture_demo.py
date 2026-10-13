#!/usr/bin/env python3
"""Record a real MyOS session in QEMU for the website demo.

Builds disk.img, boots it headless with the serial port on stdio, types the tour
commands into the user shell over COM1, and saves every output line with the time
it arrived. The website replays this file; it never edits it.

    python3 demo/capture_demo.py             # -> demo/transcript.json
    python3 demo/capture_demo.py --out x.json
"""
import argparse
import datetime
import json
import os
import select
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
QEMU = "qemu-system-i386"
QEMU_ARGS = ["-display", "none", "-serial", "stdio", "-no-shutdown", "-snapshot",
             "-m", "128M", "-drive", "format=raw,file=disk.img"]

# Run in this order. help and ls print no PIDs, so the site may replay them at any
# time; the exec steps must be replayed in this order (PIDs and frames carry over).
SESSION = [
    "help",
    "ls",
    "exec hello.elf",
    "exec fork_cow.elf",
    "exec thread_test.elf",
    "exec producer_consumer.elf",
    "exit",
]
PROMPT = b"\r> "            # print("\n") goes out as "\n\r" on COM1, then the prompt
EXIT_DONE = b"exited with code 0\n\r"
KEY_DELAY = 0.03            # seconds between typed characters


def sh(*cmd):
    return subprocess.run(cmd, cwd=ROOT, check=True, capture_output=True, text=True).stdout.strip()


class Serial:
    def __init__(self, proc):
        self.proc = proc
        self.buf = b""
        self.stamps = []    # (byte offset, monotonic time) for every chunk read

    def read_until(self, token, start, timeout):
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            if token in self.buf[start:]:
                return True
            r, _, _ = select.select([self.proc.stdout], [], [], 0.01)
            if r:
                chunk = os.read(self.proc.stdout.fileno(), 4096)
                if not chunk:
                    return False
                self.stamps.append((len(self.buf), time.monotonic()))
                self.buf += chunk
        return token in self.buf[start:]

    def time_at(self, offset):
        t = None
        for off, ts in self.stamps:
            if off > offset:
                break
            t = ts
        return t

    def send(self, data):
        self.proc.stdin.write(data)
        self.proc.stdin.flush()


def to_lines(serial, start, end, t0):
    """Split buf[start:end] into lines, each stamped with the arrival time of its first byte."""
    out, pos = [], start
    raw = serial.buf[start:end]
    for part in raw.split(b"\n"):
        text = part.replace(b"\r", b"").decode("ascii", errors="replace")
        t = serial.time_at(pos)
        out.append({"t": round(t - t0, 3), "text": text})
        pos += len(part) + 1
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(ROOT, "demo", "transcript.json"))
    args = ap.parse_args()

    print("building disk.img ...", file=sys.stderr)
    subprocess.run(["make", "disk.img"], cwd=ROOT, check=True, stdout=subprocess.DEVNULL)

    proc = subprocess.Popen([QEMU, *QEMU_ARGS], cwd=ROOT,
                            stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    serial = Serial(proc)
    try:
        if not serial.read_until(PROMPT, 0, 30):
            sys.exit("boot: no shell prompt within 30 s")
        t0 = serial.stamps[0][1]
        boot_end = serial.buf.rindex(PROMPT) + 1        # keep the "\r", drop "> "
        boot = to_lines(serial, 0, boot_end, t0)
        while boot and boot[-1]["text"] == "":
            boot.pop()

        steps = []
        for cmd in SESSION:
            print(f"> {cmd}", file=sys.stderr)
            for ch in cmd:
                serial.send(ch.encode())
                time.sleep(KEY_DELAY)
            serial.read_until(cmd.encode(), len(serial.buf) - len(cmd) - 1, 5)   # wait for the echo
            start = len(serial.buf)
            serial.send(b"\r")
            sent = time.monotonic()
            token = EXIT_DONE if cmd == "exit" else PROMPT
            if not serial.read_until(token, start, 120):
                sys.exit(f"{cmd}: no prompt within 120 s")
            end = serial.buf.index(token, start) + (len(token) if cmd == "exit" else 1)
            lines = to_lines(serial, start, end, sent)
            if lines and lines[0]["text"] == "":           # the newline after the echoed command
                lines.pop(0)
            while lines and lines[-1]["text"] == "":
                lines.pop()
            steps.append({"input": cmd, "lines": lines})
    finally:
        proc.kill()

    commit = sh("git", "rev-parse", "--short", "HEAD")
    dirty = bool(sh("git", "status", "--porcelain", "--untracked-files=no"))
    qemu_version = subprocess.run([QEMU, "--version"], capture_output=True, text=True).stdout.splitlines()[0]
    doc = {
        "captured_at": datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds"),
        "commit": commit,
        "dirty": dirty,
        "emulator": qemu_version,
        "command": f"make disk.img && {QEMU} {' '.join(QEMU_ARGS)}",
        "boot": {"lines": boot},
        "steps": steps,
    }
    with open(args.out, "w") as f:
        json.dump(doc, f, indent=1, ensure_ascii=False)
        f.write("\n")
    print(f"wrote {args.out}: {len(boot)} boot lines, {len(steps)} steps, commit {commit}"
          f"{' (DIRTY)' if dirty else ''}", file=sys.stderr)


if __name__ == "__main__":
    main()
