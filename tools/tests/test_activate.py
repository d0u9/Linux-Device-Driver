"""Run with: python3 tools/tests/test_activate.py."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ACTIVATE = Path(__file__).resolve().parents[1] / "activate"


class ActivationTest(unittest.TestCase):
    def test_directory_hooks(self):
        for shell, array_hook in (("bash", False), ("bash", True), ("zsh", False)):
            if not shutil.which(shell):
                continue
            for old_root in (None, "", "/external/tool workspace"):
                with self.subTest(shell=shell, old_root=old_root, array_hook=array_hook):
                    self.check_shell(shell, old_root, array_hook)

    def check_shell(self, shell, old_root, array_hook):
        with tempfile.TemporaryDirectory(prefix="ldd activation ") as tmp:
            workspace = Path(tmp) / "workspace"
            (workspace / "bin").mkdir(parents=True)
            (workspace / "shared/tutorial").mkdir(parents=True)
            sibling = Path(tmp) / "workspace-other"
            sibling.mkdir()
            shutil.copy2(ACTIVATE, workspace / "activate")
            qemu = workspace / "bin/qemu"
            qemu.write_text("#!/bin/sh\nexit 0\n")
            qemu.chmod(0o755)
            env = dict(os.environ)
            env.pop("LDD_ROOT", None)
            if old_root is not None:
                env["LDD_ROOT"] = old_root
            setup = '''
hook_count=0
existing_hook() { hook_count=$((hook_count + 1)); }
'''
            if shell == "bash":
                setup += '''
PROMPT_COMMAND=existing_hook
refresh() { eval "$PROMPT_COMMAND"; }
'''
                if array_hook:
                    setup = setup.replace(
                        'PROMPT_COMMAND=existing_hook',
                        'PROMPT_COMMAND=(existing_hook)',
                    ).replace(
                        'refresh() { eval "$PROMPT_COMMAND"; }',
                        'refresh() { for hook in "${PROMPT_COMMAND[@]}"; do eval "$hook"; done; }',
                    )
                flags = ["--noprofile", "--norc"]
            else:
                setup += '''
autoload -Uz add-zsh-hook
add-zsh-hook chpwd existing_hook
refresh() { :; }
'''
                flags = ["-f"]
            script = setup + '''
snapshot() { printf '%s\\n' "$PATH" "${LDD_ROOT-unset}" "${_LDD_ACTIVE-0}"; }
source "$1/activate"
snapshot
cd "$1"
refresh
snapshot
command -v qemu
source "$1/activate"
cd shared/tutorial
refresh
snapshot
cd "$2"
refresh
snapshot
cd "$1"
refresh
snapshot
printf '%s\\n' "$hook_count"
false
_ldd_refresh
printf '%s\\n' "$?"
'''
            result = subprocess.run(
                [shell, *flags, "-c", script, "test", str(workspace), str(sibling)],
                cwd=tmp, env=env, capture_output=True, text=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            lines = result.stdout.splitlines()
            outside = [env["PATH"], old_root if old_root is not None else "unset", "0"]
            active = [f"{workspace}/bin:{env['PATH']}", str(workspace.resolve()), "1"]
            # Resolve macOS /var -> /private/var for the script's physical path.
            active[0] = f"{workspace.resolve()}/bin:{env['PATH']}"
            self.assertEqual(lines[:3], outside)
            self.assertEqual(lines[3:6], active)
            self.assertEqual(Path(lines[6]).resolve(), qemu.resolve())
            self.assertEqual(lines[7:10], active)
            self.assertEqual(lines[10:13], outside)
            self.assertEqual(lines[13:16], active)
            self.assertGreater(int(lines[16]), 0)
            self.assertEqual(lines[17], "1")


if __name__ == "__main__":
    unittest.main()
