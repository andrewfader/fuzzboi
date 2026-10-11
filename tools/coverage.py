#!/usr/bin/env python3
"""Line coverage of src/ from a build configured with -DFUZZBOI_COVERAGE=ON.

Runs gcov in JSON mode over every .gcda the test run left behind, unions the executable and
executed lines of each source file across all the objects that included it (the DSP is
header-only, so most files are compiled into several test binaries), and prints a per-file
and total percentage. No gcovr or lcov needed.

    cmake -S . -B build-cov -G Ninja -DCMAKE_BUILD_TYPE=Debug -DFUZZBOI_COVERAGE=ON
    cmake --build build-cov && ctest --test-dir build-cov
    python3 tools/coverage.py build-cov [--min 80]
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from collections import defaultdict
from pathlib import Path


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("build", type=Path)
    parser.add_argument("--min", type=float, default=0.0, help="fail if the total is below this percentage")
    parser.add_argument("--gcov", default=os.environ.get("GCOV", "gcov"))
    parser.add_argument("--exclude", action="append", default=["src/ui/canvas_", "src/ui/backends.hpp"],
                        help="path fragments to leave out (platform window code needs a display)")
    args = parser.parse_args(argv)

    root = Path(__file__).resolve().parent.parent
    executable: dict[str, set[int]] = defaultdict(set)
    executed: dict[str, set[int]] = defaultdict(set)
    gcdas = list(args.build.rglob("*.gcda"))
    if not gcdas:
        print("no .gcda files: build with -DFUZZBOI_COVERAGE=ON and run the tests first", file=sys.stderr)
        return 2
    for gcda in gcdas:
        result = subprocess.run([args.gcov, "--json-format", "--stdout", str(gcda)], capture_output=True, text=True, check=False)
        if result.returncode != 0 or not result.stdout.strip():
            continue
        for record in result.stdout.split("\n"):
            if not record.strip():
                continue
            try:
                data = json.loads(record)
            except json.JSONDecodeError:
                continue
            for file in data.get("files", []):
                path = Path(file["file"])
                try:
                    rel = str(path.resolve().relative_to(root))
                except ValueError:
                    continue
                if not rel.startswith("src/") or any(x in rel for x in args.exclude):
                    continue
                for line in file.get("lines", []):
                    number = line["line_number"]
                    executable[rel].add(number)
                    if line.get("count", 0) > 0:
                        executed[rel].add(number)
    total_lines = total_hit = 0
    print(f"{'file':44} {'lines':>7} {'hit':>7} {'cover':>7}")
    for rel in sorted(executable):
        lines, hit = len(executable[rel]), len(executed[rel])
        total_lines += lines
        total_hit += hit
        print(f"{rel:44} {lines:7d} {hit:7d} {100.0 * hit / lines:6.1f}%")
    if not total_lines:
        print("no source lines found", file=sys.stderr)
        return 2
    total = 100.0 * total_hit / total_lines
    print(f"{'TOTAL':44} {total_lines:7d} {total_hit:7d} {total:6.1f}%")
    return 0 if total >= args.min else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
