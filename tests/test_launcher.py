"""Check the launcher contract and menu error recovery with installed package layout."""
import os
from pathlib import Path
import pty
import select
import shlex
import shutil
import subprocess
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]


class LauncherTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="ktm-launch-")
        self.root = Path(self.tmp.name)
        self.package = self.root / "package with spaces"
        shutil.copytree(ROOT / "kpm", self.package)
        (self.package / "bin").mkdir()
        for binary in ("ktm", "ktm-input"):
            shutil.copy2(ROOT / "build" / binary, self.package / "bin" / binary)
        self.env = dict(os.environ, KTM_DATA_DIR=str(self.root / "data"),
                        TG_CURL=str(ROOT / "tests/mock_curl.sh"), TERM="dumb")

    def tearDown(self):
        self.tmp.cleanup()

    def test_official_kterm_launcher_and_quoted_session(self):
        fake = self.root / "kpm"
        fake.write_text("#!/bin/sh\nprintf '%s\\n' \"$@\"\n")
        fake.chmod(0o700)
        self.env["KTM_KPM"] = str(fake)
        p = subprocess.run(["sh", str(self.package / "launch.sh")], env=self.env, capture_output=True)
        self.assertEqual(p.returncode, 0, p.stderr)
        args = p.stdout.decode().splitlines()
        self.assertEqual(args[:3], ["launch", "kterm", "-e"])
        self.assertEqual(shlex.split(args[3]), ["/bin/sh", str(self.package / "session.sh")])

    def test_menu_is_readable_even_without_execute_bit(self):
        (self.package / "ui.sh").chmod(0o600)
        p = subprocess.run(["sh", str(self.package / "session.sh")], input=b"7\n",
                           env=self.env, capture_output=True, timeout=5)
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertIn(b"ktm 0.1.11", p.stdout)

    def test_sync_error_returns_to_menu(self):
        p = subprocess.run(["sh", str(self.package / "session.sh")], input=b"5\n\n7\n",
                           env=self.env, capture_output=True, timeout=5)
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertEqual(p.stdout.count(b"ktm 0.1.11"), 2)

    def test_cli_status_does_not_start_gui(self):
        p = subprocess.run(["sh", str(self.package / "launch.sh"), "status"],
                           env=self.env, capture_output=True)
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertIn(b"version=0.1.11", p.stdout)

    def test_full_menu_5_shell_and_return_to_menu(self):
        core = str(self.package / "bin" / "ktm")
        subprocess.run([core, "setup"], input=b"not-a-real-token\n", env=self.env,
                       capture_output=True, check=True)
        pairing = subprocess.run([core, "pair"], env=self.env, capture_output=True, check=True)
        code = next(line.split()[-1] for line in pairing.stdout.decode().splitlines()
                    if line.startswith("Pairing code"))
        subprocess.run([core, "sync"], env=dict(self.env, MOCK_MODE="pair", MOCK_PAIR_CODE=code),
                       capture_output=True, check=True)
        self.env["MOCK_MODE"] = "empty"
        marker = self.root / "user-confirmed"
        message = Path(self.env["KTM_DATA_DIR"]) / "inbox" / "current.txt"
        original = f"touch {shlex.quote(str(marker))}".encode()
        message.write_bytes(original)
        master, slave = pty.openpty()
        child = subprocess.Popen(["sh", str(self.package / "session.sh")], env=self.env,
                                 stdin=slave, stdout=slave, stderr=slave, close_fds=True)
        os.close(slave)
        def receive(needle):
            output = b""
            deadline = time.monotonic() + 8
            while needle not in output and time.monotonic() < deadline:
                if select.select([master], [], [], .1)[0]:
                    output += os.read(master, 8192)
            self.assertIn(needle, output, output)
        try:
            receive(b"Choose:")
            os.write(master, b"5\r")
            receive(b"user-confirmed")
            self.assertFalse(marker.exists())
            os.write(master, b"\r")
            deadline = time.monotonic() + 5
            while not marker.exists() and time.monotonic() < deadline:
                time.sleep(.02)
            self.assertTrue(marker.exists())
            os.write(master, b"exit\r")
            receive(b"Choose:")
            os.write(master, b"7\r")
            self.assertEqual(child.wait(timeout=5), 0)
            self.assertEqual(message.read_bytes(), original)
        finally:
            if child.poll() is None:
                child.terminate()
                child.wait(timeout=5)
            os.close(master)

    def test_startup_failure_log_is_retained(self):
        fake = self.root / "kpm"
        fake.write_text("#!/bin/sh\nprintf 'loader-failed\\n' >&2\nexit 127\n")
        fake.chmod(0o700)
        self.env["KTM_KPM"] = str(fake)
        p = subprocess.run(["sh", str(self.package / "launch.sh")], env=self.env, capture_output=True)
        self.assertEqual(p.returncode, 127)
        log = Path(self.env["KTM_DATA_DIR"]) / "logs" / "startup.log"
        self.assertEqual(log.read_text(), "loader-failed\n")


if __name__ == "__main__":
    unittest.main()
