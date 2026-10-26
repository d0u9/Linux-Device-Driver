"""Run with: python3 tools/tests/test_boot.py."""

import os
from pathlib import Path
import subprocess
import tempfile
import unittest


BOOT = Path(__file__).resolve().parents[1] / "bin/boot.sh"


class BootTest(unittest.TestCase):
    def test_launch_and_missing_inputs(self):
        with tempfile.TemporaryDirectory(prefix="ldd boot ") as tmp:
            root = Path(tmp)
            (root / "bin").mkdir()
            qemu = root / "bin/qemu"
            qemu.write_text('#!/bin/sh\nprintf "%s\\n" "$@"\nexit 42\n')
            qemu.chmod(0o755)
            image = root / "src/linux-stable/arch/x86/boot/bzImage"
            image.parent.mkdir(parents=True)
            image.touch()
            archive = root / "initramfs.cpio.gz"
            archive.touch()
            shared = root / "shared"
            shared.mkdir()
            env = {**os.environ, "LDD_ROOT": str(root)}

            result = subprocess.run(["bash", str(BOOT), "-device", "extra"],
                                    env=env, capture_output=True, text=True)
            self.assertEqual(result.returncode, 42, result.stderr)
            args = result.stdout.splitlines()
            self.assertEqual(args[-2:], ["-device", "extra"])
            self.assertEqual(args[args.index("-kernel") + 1], str(image))
            self.assertEqual(args[args.index("-initrd") + 1], str(archive))
            self.assertEqual(args[args.index("-nic") + 1],
                             "user,model=e1000,hostfwd=tcp:127.0.0.1:7023-:23")
            self.assertEqual(args[args.index("-virtfs") + 1],
                             f"local,path={shared},mount_tag=ldd,security_model=mapped-xattr")

            for missing in (archive, image, shared):
                if missing.is_dir():
                    missing.rmdir()
                else:
                    missing.unlink()
                result = subprocess.run(["bash", str(BOOT)], env=env,
                                        capture_output=True, text=True)
                self.assertEqual(result.returncode, 1)
                self.assertIn("Could not locate", result.stdout)
                if missing == shared:
                    missing.mkdir()
                else:
                    missing.touch()


if __name__ == "__main__":
    unittest.main()
