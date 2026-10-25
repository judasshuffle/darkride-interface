"""Check the container-only bootstrap patch without downloading/building dependencies."""
import ast
import os
from pathlib import Path
import subprocess
import tempfile
from types import SimpleNamespace
from unittest.mock import Mock

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
PATCH = HERE / "vcpkg-bootstrap-cstdint.patch"

with tempfile.TemporaryDirectory() as directory:
    source = Path(directory) / "hifi_vcpkg.py"
    source.write_bytes(subprocess.check_output(["git", "show", "HEAD:hifi_vcpkg.py"], cwd=ROOT))
    subprocess.run(["git", "apply", "--check", str(PATCH)], cwd=directory, check=True)
    subprocess.run(["git", "apply", str(PATCH)], cwd=directory, check=True)
    tree = ast.parse(source.read_text())
    compile(tree, str(source), "exec")

# Execute the actual patched bootstrap method with its filesystem/download/build
# operations mocked. This checks the environment passed at the real call site.
repo = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == "VcpkgRepo")
method = next(n for n in repo.body if isinstance(n, ast.FunctionDef) and n.name == "bootstrap")
code = compile(ast.Module(body=[method], type_ignores=[]), "hifi_vcpkg.py", "exec")
for system, version, flags in [
    ("Linux", "2021.05.12", None),
    ("Linux", "2021.05.12", "-O2 -DKEEP_EXISTING=1"),
    ("Linux", "different-version", "-O2"),
    ("Darwin", "2021.05.12", "-O2"),
    ("Windows", "2021.05.12", "-O2"),
]:
    original = {"PRESERVE": "yes"}
    if flags is not None:
        original["CXXFLAGS"] = flags
    bootstrap_env = original.copy()
    build_env = {"CXXFLAGS": "-DPORT_FLAGS=1"}
    state = SimpleNamespace(
        prebuiltArchive=None, upToDate=lambda: False, clean=Mock(),
        checkWindowsTools=Mock(), checkWindowsCompiler=Mock(), buildWindowsTool=Mock(),
        args=SimpleNamespace(force_bootstrap=True), path="isolated-vcpkg",
        exe="isolated-vcpkg/vcpkg", vcpkgVersion=version,
        bootstrapCmds=["bootstrap-vcpkg.sh", "-disableMetrics"],
        bootstrapEnv=bootstrap_env, buildEnv=build_env,
        sourcePortsPath="source-ports",
    )
    utils = Mock()
    fake_os = SimpleNamespace(
        path=SimpleNamespace(join=os.path.join, isfile=lambda p: False,
                             isdir=lambda p: False, islink=lambda p: False),
        listdir=lambda p: [], rmdir=Mock(), unlink=Mock(),
    )
    scope = {"os": fake_os, "shutil": Mock(), "hifi_utils": utils,
             "platform": SimpleNamespace(system=lambda: system), "print": lambda *a: None}
    exec(code, scope)
    scope["bootstrap"](state)
    calls = [c for c in utils.executeSubprocess.call_args_list
             if c.args[0] == state.bootstrapCmds]
    if system == "Windows":
        assert not calls
        state.buildWindowsTool.assert_called_once()
    else:
        assert len(calls) == 1
        expected = original.copy()
        if system == "Linux" and version == "2021.05.12":
            expected["CXXFLAGS"] = ((flags or "") + " -include cstdint").strip()
        assert calls[0].kwargs["env"] == expected
        assert calls[0].kwargs["env"] is not bootstrap_env
    assert bootstrap_env == original
    assert build_env == {"CXXFLAGS": "-DPORT_FLAGS=1"}
print("PASS: patch application, Python syntax, pinned Linux bootstrap flags, existing flags preserved")
print("PASS: other versions/platforms and port/server environments unchanged")
