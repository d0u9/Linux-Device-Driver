"""Run with: python3 tools/tests/test_check_builds.py."""

from pathlib import Path
import subprocess
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "check_builds.sh"
REPO = SCRIPT.parents[1]


class CheckBuildsTest(unittest.TestCase):
    def test_reports_failures_and_keeps_checkout_clean(self):
        status = lambda: subprocess.run(
            ["git", "status", "--porcelain", "--ignored", "examples"],
            cwd=REPO, capture_output=True, text=True).stdout
        before = status()
        with tempfile.TemporaryDirectory(prefix="ldd-kernel-") as tmp:
            kernel = Path(tmp)
            # Fake kbuild: fails only for 03_scull_basic.
            (kernel / "Makefile").write_text(
                "modules:\n\tcase '$(M)' in *03_scull_basic*) exit 1;; esac\n"
                "\ttouch '$(M)/Module.symvers'\n")
            run = lambda *a: subprocess.run(
                ["bash", str(SCRIPT), str(kernel), *a],
                capture_output=True, text=True)

            ok = run("01_hello_world", "22_lddbus")
            self.assertEqual(ok.returncode, 0, ok.stdout + ok.stderr)
            self.assertIn("PASS 22_lddbus/sculld", ok.stdout)

            bad = run("01_hello_world", "03_scull_basic", "nope")
            self.assertEqual(bad.returncode, 1)
            self.assertIn("FAIL 03_scull_basic", bad.stdout)
            self.assertIn("1 built, 2 failed", bad.stdout)

            everything = run()
            self.assertIn("01_hello_world", everything.stdout)
            self.assertNotIn("_NOT_COMPLETE", everything.stdout)

        self.assertEqual(status(), before)

    def test_rejects_non_kernel_dir(self):
        with tempfile.TemporaryDirectory() as tmp:
            result = subprocess.run(["bash", str(SCRIPT), tmp],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 2)


if __name__ == "__main__":
    unittest.main()
