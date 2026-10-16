"""Run with: python3 tools/tests/test_qemu_shim.py."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


SHIM = Path(__file__).resolve().parents[1] / "bin/qemu"


class QemuShimTest(unittest.TestCase):
    def test_root_arguments_and_exit_status(self):
        with tempfile.TemporaryDirectory(prefix="ldd workspace ") as tmp:
            root = Path(tmp)
            (root / "bin").mkdir()
            shim = root / "bin/qemu"
            shutil.copy2(SHIM, shim)
            qemu = root / "src/qemu/build/qemu-system-x86_64"
            qemu.parent.mkdir(parents=True)
            qemu.write_text('#!/bin/sh\nprintf "<%s>\\n" "$@"\nexit 42\n')
            qemu.chmod(0o755)
            for override in (False, True):
                env = dict(os.environ)
                env.pop("LDD_ROOT", None)
                if override:
                    env["LDD_ROOT"] = "/unrelated/workspace"
                result = subprocess.run(
                    [str(shim), "a b", "", "*"],
                    cwd="/", env=env, capture_output=True, text=True,
                )
                self.assertEqual(result.returncode, 42, result.stderr)
                self.assertEqual(result.stdout, "<a b>\n<>\n<*>\n")

    def test_missing_or_nonexecutable_qemu(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "bin").mkdir()
            shim = root / "bin/qemu"
            shutil.copy2(SHIM, shim)
            qemu = root / "src/qemu/build/qemu-system-x86_64"
            for exists in (False, True):
                if exists:
                    qemu.parent.mkdir(parents=True)
                    qemu.touch(mode=0o644)
                result = subprocess.run(
                    [str(shim)], env={**os.environ, "LDD_ROOT": tmp},
                    capture_output=True, text=True,
                )
                self.assertEqual(result.returncode, 127)
                self.assertIn(str(qemu), result.stderr)


if __name__ == "__main__":
    unittest.main()
