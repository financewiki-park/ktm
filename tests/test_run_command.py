"""Receive/review/execute real shell commands over a terminal, including installers."""
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


class RunTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="ktm-run-")
        self.root = Path(self.tmp.name)
        self.inbox = self.root / "data" / "inbox"
        self.inbox.mkdir(parents=True)
        self.message = self.inbox / "current.txt"
        self.env = dict(os.environ, KTM_DATA_DIR=str(self.root / "data"),
                        KTM_COMMAND_CWD=str(self.root))
        self.master = None
        self.child = None
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
        self.child = subprocess.Popen(COMMAND + ["run"], stdin=slave, stdout=slave,
                                      stderr=slave, env=self.env, close_fds=True)
        os.close(slave)

    def receive(self, needle):
        deadline = time.monotonic() + 8
        while needle not in self.output and time.monotonic() < deadline:
            if select.select([self.master], [], [], .1)[0]:
                try:
                    data = os.read(self.master, 8192)
                except OSError as e:
                    if e.errno == errno.EIO:
                        break
                    raise
                if not data:
                    break
                self.output += data
        self.assertIn(needle, self.output, self.output)

    def confirm(self):
        self.receive(b"Enter alone cancels:")
        os.write(self.master, b"r\r")

    def assert_snapshot_cleaned(self):
        self.assertEqual(list((self.root / "data" / "state").glob("command.*")), [])

    def test_multiline_install_command_runs_only_after_confirmation(self):
        fake = self.root / "kpm"
        fake.write_text("#!/bin/sh\nprintf '%s\\n' \"$@\" > installed\nprintf 'INSTALL_COMPLETE\\n'\n")
        fake.chmod(0o700)
        text = f'KPM={shlex.quote(str(fake))}\n"$KPM" install ktm\n'.encode()
        self.start(text)
        self.receive(b"Enter alone cancels:")
        self.assertFalse((self.root / "installed").exists())
        os.write(self.master, b"r\r")
        self.receive(b"exit code 0")
        self.assertEqual(self.child.wait(timeout=5), 0)
        self.assertEqual((self.root / "installed").read_text(), "install\nktm\n")
        self.assertIn(b"INSTALL_COMPLETE", self.output)
        self.assertEqual(self.message.read_bytes(), text)
        self.assert_snapshot_cleaned()

    def test_enter_cancels_without_execution(self):
        self.start(b"touch should-not-exist")
        self.receive(b"Enter alone cancels:")
        os.write(self.master, b"\r")
        self.receive(b"Cancelled.")
        self.child.wait(timeout=5)
        self.assertFalse((self.root / "should-not-exist").exists())
        self.assert_snapshot_cleaned()

    def test_interactive_installer_gets_keyboard_input(self):
        self.start(b"printf 'Installer answer: '; read answer\nprintf '%s' \"$answer\" > answer")
        self.confirm()
        self.receive(b"--- Running in Kindle shell ---")
        os.write(self.master, b"yes\r")
        self.receive(b"exit code 0")
        self.child.wait(timeout=5)
        self.assertEqual((self.root / "answer").read_bytes(), b"yes")

    def test_heredoc_pipeline_unicode_and_more_than_512_bytes(self):
        payload = "한글 command " * 100
        text = f"cat <<'END' | tr a-z A-Z > result\n{payload}\nEND\n".encode()
        self.start(text)
        self.confirm()
        self.receive(b"exit code 0")
        self.child.wait(timeout=5)
        self.assertEqual((self.root / "result").read_text(), payload.upper() + "\n")

    def test_new_message_cannot_change_reviewed_command(self):
        self.start(b"printf reviewed > result")
        self.receive(b"Enter alone cancels:")
        self.message.write_bytes(b"printf replaced > result")
        os.write(self.master, b"r\r")
        self.receive(b"exit code 0")
        self.child.wait(timeout=5)
        self.assertEqual((self.root / "result").read_bytes(), b"reviewed")
        self.assertEqual(self.message.read_bytes(), b"printf replaced > result")

    def test_syntax_error_does_not_execute_earlier_lines(self):
        self.start(b"touch should-not-exist\nif then")
        self.receive(b"syntax check failed")
        self.assertEqual(self.child.wait(timeout=5), 2)
        self.assertFalse((self.root / "should-not-exist").exists())
        self.assert_snapshot_cleaned()

    def test_exit_code_and_crlf(self):
        text = b"printf ok > result\r\nexit 7\r\n"
        self.start(text)
        self.confirm()
        self.receive(b"exit code 7")
        self.assertEqual(self.child.wait(timeout=5), 7)
        self.assertEqual((self.root / "result").read_bytes(), b"ok")
        self.assertEqual(self.message.read_bytes(), text)
        self.assert_snapshot_cleaned()

    def test_no_tty_rejected(self):
        self.message.write_bytes(b"touch should-not-exist")
        p = subprocess.run(COMMAND + ["run"], input=b"r\n", env=self.env, capture_output=True)
        self.assertNotEqual(p.returncode, 0)
        self.assertFalse((self.root / "should-not-exist").exists())


if __name__ == "__main__":
    unittest.main()
