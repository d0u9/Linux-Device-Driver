"""Run with: python3 tools/tests/test_install.py."""

import os
from pathlib import Path
import subprocess
import tempfile
import unittest


TOOLS = Path(__file__).resolve().parents[1]


class InstallerTest(unittest.TestCase):
    def test_install_and_update(self):
        with tempfile.TemporaryDirectory(prefix="ldd install ") as tmp:
            runtime = Path(tmp) / "runtime"
            (runtime / "initramfs").mkdir(parents=True)
            existing_file = runtime / "initramfs/keep"
            existing_file.write_text("existing guest file")
            for _ in range(2):
                subprocess.run([str(TOOLS / "install.sh"), str(runtime)],
                               cwd="/", check=True, capture_output=True)
                self.assertEqual((runtime / "activate").read_bytes(),
                                 (TOOLS / "activate").read_bytes())
                for name in ("qemu", "boot.sh", "rebuild_initramfs.sh"):
                    self.assertEqual((runtime / "bin" / name).read_bytes(),
                                     (TOOLS / "bin" / name).read_bytes())
                    self.assertTrue(os.access(runtime / "bin" / name, os.X_OK))
                self.assertEqual(sorted(p.name for p in runtime.iterdir()),
                                 ["activate", "bin", "initramfs", "kernels", "shared", "src"])
                for name in ("bin", "src", "kernels", "initramfs", "shared"):
                    self.assertTrue((runtime / name).is_dir())
                self.assertEqual(existing_file.read_text(), "existing guest file")
                self.assertEqual(list((runtime / "src").iterdir()), [])
                self.assertEqual(sorted(p.name for p in (runtime / "bin").iterdir()),
                                 ["boot.sh", "qemu", "rebuild_initramfs.sh"])

    def test_reject_invalid_destination(self):
        for args in ([], [""], [str(TOOLS)], [str(TOOLS.parent)]):
            result = subprocess.run([str(TOOLS / "install.sh"), *args],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 2, result.stderr)

    def test_replace_old_symlink_without_overwriting_target(self):
        with tempfile.TemporaryDirectory() as tmp:
            runtime = Path(tmp) / "runtime"
            (runtime / "bin").mkdir(parents=True)
            old_binary = Path(tmp) / "old-qemu"
            old_binary.write_text("keep old binary")
            (runtime / "bin/qemu").symlink_to(old_binary)
            subprocess.run([str(TOOLS / "install.sh"), str(runtime)],
                           check=True, capture_output=True)
            self.assertEqual(old_binary.read_text(), "keep old binary")
            self.assertFalse((runtime / "bin/qemu").is_symlink())


if __name__ == "__main__":
    unittest.main()
