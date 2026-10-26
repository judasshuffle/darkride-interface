"""Expose dependency diagnostics before a failed Docker layer is discarded."""
from pathlib import Path
import re
import sys

# Bound Actions output: ten newest logs, first error context and the final 60 lines.
logs = []
for root in map(Path, sys.argv[1:]):
    logs.extend(p for p in root.rglob("*.log") if p.is_file())
for path in sorted(logs, key=lambda p: (p.stat().st_mtime_ns, str(p)), reverse=True)[:10]:
    lines = path.read_text(errors="replace").splitlines()
    if not lines:
        continue
    print(f"=== BUILD DIAGNOSTIC: {path} ===")
    selected = set(range(max(0, len(lines) - 60), len(lines)))
    for i, line in enumerate(lines):
        if re.search(r"error:|fatal error|FAILED:|CMake Error", line, re.I):
            selected.update(range(max(0, i - 5), min(len(lines), i + 25)))
            break
    previous = -1
    for i in sorted(selected):
        if i > previous + 1:
            print("... omitted ...")
        print(f"{i + 1}: {lines[i][:2000]}")
        previous = i
