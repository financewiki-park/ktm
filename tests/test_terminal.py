"""Exercise actual nested PTYs and a real shell, not mocked input-feed calls."""
import errno
import os
from pathlib import Path
import pty
import select
import shlex
import subprocess
import sys
import tempfile
import time
import unittest

COMMAND = shlex.split(os.environ.get("TEST_RUNNER", "")) + [str(Path(sys.argv.pop(1)).resolve())]


class TerminalTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="ktm-pty-")
        self.root = Path(self.tmp.name)
        self.message = self.root / "current.txt"
        self.child = None
        self.master = None
        self.output = b""

    def tearDown(self):
        if self.child and self.child.poll() is None:
            self.child.terminate()
            self.child.wait(timeout=5)
        if self.master is not None:
            os.close(self.master)
        self.tmp.cleanup()

    def start(self, text):
        self.message.write_bytes(text)
        self.master, slave = pty.openpty()
        self.child = subprocess.Popen(COMMAND + [str(self.message)], stdin=slave,
                                      stdout=slave, stderr=slave, close_fds=True)
        os.close(slave)

    def read_until(self, needle, timeout=8):
        deadline = time.monotonic() + timeout
        while needle not in self.output and time.monotonic() < deadline:
            if select.select([self.master], [], [], .1)[0]:
                try:
                    b = os.read(self.master, 8192)
                except OSError as e:
                    if e.errno == errno.EIO:
                        break
                    raise
                if not b:
                    break
                self.output += b
        self.assertIn(needle, self.output, self.output)

    def test_input_not_executed_until_enter(self):
        marker = self.root / "result"
        text = f"printf '한글' > {shlex.quote(str(marker))}".encode()
        self.start(text)
        self.read_until(marker.name.encode())
        time.sleep(.2)
        self.assertFalse(marker.exists(), "Receiving input must never execute it")
        self.assertEqual(self.message.read_bytes(), text)
        os.write(self.master, b"\r")
        deadline = time.monotonic() + 5
        while not marker.exists() and time.monotonic() < deadline:
            time.sleep(.02)
        self.assertEqual(marker.read_bytes(), "한글".encode())
        os.write(self.master, b"exit\r")
        self.assertEqual(self.child.wait(timeout=5), 0)

    def test_input_can_be_cleared_and_replaced(self):
        unwanted = self.root / "must-not-exist"
        wanted = self.root / "edited"
        text = f"touch {shlex.quote(str(unwanted))}".encode()
        self.start(text)
        self.read_until(unwanted.name.encode())
        os.write(self.master, b"\x15" + f"touch {shlex.quote(str(wanted))}\r".encode())
        deadline = time.monotonic() + 5
        while not wanted.exists() and time.monotonic() < deadline:
            time.sleep(.02)
        self.assertTrue(wanted.exists(), self.output)
        self.assertFalse(unwanted.exists())
        os.write(self.master, b"exit\r")
        self.child.wait(timeout=5)

    def test_ctrl_c_cancels_input(self):
        marker = self.root / "must-not-exist"
        self.start(f"touch {shlex.quote(str(marker))}".encode())
        self.read_until(marker.name.encode())
        os.write(self.master, b"\x03")
        time.sleep(.1)
        os.write(self.master, b"exit\r")
        self.child.wait(timeout=5)
        self.assertFalse(marker.exists())

    def test_rejected_messages_remain_byte_exact(self):
        for data in (b"", b"a\nb", b"a\rb", b"a\tb", b"\x1b[31m", b"a\0b",
                     b"a\x7fb", b"a" * 513):
            with self.subTest(data=data[:10]):
                self.message.write_bytes(data)
                p = subprocess.run(COMMAND + ["--check", str(self.message)], capture_output=True)
                self.assertNotEqual(p.returncode, 0)
                self.assertEqual(self.message.read_bytes(), data)

    def test_no_tty_is_visible_error(self):
        self.message.write_bytes(b"echo test")
        p = subprocess.run(COMMAND + [str(self.message)], capture_output=True)
        self.assertNotEqual(p.returncode, 0)
        self.assertIn(b"inside KTerm", p.stderr)

    def test_512_byte_boundary(self):
        self.message.write_bytes(b"a" * 512)
        p = subprocess.run(COMMAND + ["--check", str(self.message)], capture_output=True)
        self.assertEqual(p.returncode, 0, p.stderr)


if __name__ == "__main__":
    unittest.main()
