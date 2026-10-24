"""Run with: python3 tools/tests/test_qemu_link.py."""

import os
from pathlib import Path
import subprocess
import tempfile
import unittest


TOOLS = Path(__file__).resolve().parents[1]
LINK_SH = TOOLS / "qemu/link.sh"
DEVICES = (TOOLS / "qemu/hw/ldd").resolve()
MESON = "subdir('misc')\n"
KCONFIG = "source misc/Kconfig\n"


def run(*args, env=None):
    return subprocess.run(
        ["bash", str(LINK_SH), *args],
        env={**os.environ, **(env or {})}, capture_output=True, text=True,
    )


class QemuLinkTest(unittest.TestCase):
    def make_tree(self, tmp, git=True):
        qemu = Path(tmp) / "qemu src"
        (qemu / "hw").mkdir(parents=True)
        (qemu / "hw/meson.build").write_text(MESON)
        (qemu / "hw/Kconfig").write_text(KCONFIG)
        if git:
            subprocess.run(["git", "init", "-q", str(qemu)], check=True)
        return qemu

    def test_link_is_idempotent_and_removable(self):
        with tempfile.TemporaryDirectory() as tmp:
            qemu = self.make_tree(tmp)
            exclude = qemu / ".git/info/exclude"
            for _ in range(2):
                result = run(str(qemu))
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(os.readlink(qemu / "hw/ldd"), str(DEVICES))
                self.assertEqual((qemu / "hw/meson.build").read_text(),
                                 MESON + "subdir('ldd')\n")
                self.assertEqual((qemu / "hw/Kconfig").read_text(),
                                 KCONFIG + "source ldd/Kconfig\n")
                self.assertEqual(exclude.read_text().count("/hw/ldd\n"), 1)
            status = subprocess.run(
                ["git", "-C", str(qemu), "status", "--porcelain"],
                capture_output=True, text=True, check=True,
            ).stdout
            self.assertNotIn("ldd", status)

            result = run("--remove", str(qemu))
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertFalse(os.path.lexists(qemu / "hw/ldd"))
            self.assertEqual((qemu / "hw/meson.build").read_text(), MESON)
            self.assertEqual((qemu / "hw/Kconfig").read_text(), KCONFIG)
            self.assertNotIn("/hw/ldd", exclude.read_text())

    def test_default_path_and_missing_newline(self):
        with tempfile.TemporaryDirectory() as tmp:
            qemu = Path(tmp) / "src/qemu"
            (qemu / "hw").mkdir(parents=True)
            (qemu / "hw/meson.build").write_text(MESON.rstrip("\n"))
            (qemu / "hw/Kconfig").write_text(KCONFIG)
            result = run(env={"LDD_ROOT": tmp})
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual((qemu / "hw/meson.build").read_text(),
                             MESON + "subdir('ldd')\n")

    def test_refuses_foreign_path_and_non_qemu_tree(self):
        with tempfile.TemporaryDirectory() as tmp:
            qemu = self.make_tree(tmp, git=False)
            (qemu / "hw/ldd").mkdir()
            for args in ((str(qemu),), ("--remove", str(qemu))):
                result = run(*args)
                self.assertEqual(result.returncode, 1)
                self.assertTrue((qemu / "hw/ldd").is_dir())
            self.assertEqual((qemu / "hw/meson.build").read_text(), MESON)

            result = run(tmp)
            self.assertEqual(result.returncode, 1)
            self.assertIn("Not a QEMU source tree", result.stderr)


if __name__ == "__main__":
    unittest.main()
