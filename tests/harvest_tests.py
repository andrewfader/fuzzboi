#!/usr/bin/env python3
"""Executable scenario for features/samples.feature: the harvest tool accepts only permissive
licences and records attribution. Runs against a fake archive (no network) and a fake ffmpeg.

    python3 -I tests/harvest_tests.py
"""
from __future__ import annotations

import importlib.util
import json
import os
import shutil
import stat
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("harvest", ROOT / "tools" / "harvest_archive.py")
harvest = importlib.util.module_from_spec(spec)
spec.loader.exec_module(harvest)  # type: ignore[union-attr]

failures = 0


def check(condition: bool, what: str) -> None:
    global failures
    if condition:
        return
    failures += 1
    print(f"    FAILED: {what}")


FAKE_ARCHIVE = {
    "zero_toy": {"title": "Toy piano, CC0", "licenseurl": "https://creativecommons.org/publicdomain/zero/1.0/",
                 "files": [{"name": "toy.mp3", "source": "derivative"}, {"name": "toy.wav", "source": "original"}]},
    "by_theremin": {"title": "Theremin, CC BY", "licenseurl": "http://creativecommons.org/licenses/by/4.0/",
                    "files": [{"name": "theremin.flac", "source": "original"}]},
    "nc_box": {"title": "Music box, CC BY-NC", "licenseurl": "https://creativecommons.org/licenses/by-nc/4.0/",
               "files": [{"name": "box.wav", "source": "original"}]},
    "no_licence": {"title": "Unlicensed", "licenseurl": None, "files": [{"name": "x.wav"}]},
    "no_audio": {"title": "Text only, CC0", "licenseurl": "https://creativecommons.org/publicdomain/zero/1.0/",
                 "files": [{"name": "notes.txt"}]},
}


def fake_search(query: str, rows: int) -> list[dict]:
    return [{"identifier": k, "title": v["title"], "licenseurl": v["licenseurl"]} for k, v in FAKE_ARCHIVE.items()]


def fake_audio_files(identifier: str) -> list[dict]:
    files = FAKE_ARCHIVE[identifier]["files"]
    chosen = [f for f in files if f.get("name", "").lower().endswith(harvest.AUDIO_EXTENSIONS)]
    chosen.sort(key=lambda f: (f.get("source") != "original", not f["name"].lower().endswith((".wav", ".flac"))))
    return chosen


downloaded: list[tuple[str, str]] = []


def fake_download(identifier: str, name: str, target: Path) -> None:
    downloaded.append((identifier, name))
    target.write_bytes(b"RIFF-not-really")


peaks: list[float | None] = []


def fake_convert(source: Path, target: Path, rate: int, max_seconds: float, peak_target_db: float | None = None) -> bool:
    peaks.append(peak_target_db)
    target.write_bytes(b"converted:" + source.name.encode())
    return True


def main() -> int:
    print("Scenario: harvest_tool_filters_licences_and_writes_a_manifest")
    print("  Given  search results under CC0, CC BY, CC BY-NC and no licence")
    harvest.search = fake_search
    harvest.audio_files = fake_audio_files
    harvest.download = fake_download
    harvest.convert = fake_convert
    out = Path(tempfile.mkdtemp(prefix="fuzzboi-harvest-"))
    try:
        print("  When   the tool runs against a fake archive")
        code = harvest.main(["--query", "toy", "--out", str(out), "--max", "16"])
        print("  Then   only the CC0 and CC BY items are downloaded, converted and listed in the manifest with their licences")
        check(code == 0, "exit code 0 when something was harvested")
        manifest = json.loads((out / "manifest.json").read_text())
        ids = [m["identifier"] for m in manifest]
        check(ids == ["zero_toy", "by_theremin"], f"accepted exactly the permissive items, got {ids}")
        check([d[0] for d in downloaded] == ["zero_toy", "by_theremin"], "downloaded only accepted items")
        check(downloaded[0][1] == "toy.wav", "prefers the lossless original over the derivative mp3")
        check(all(m["license"] and m["url"].startswith("https://archive.org/details/") for m in manifest), "licence and URL recorded")
        check((out / manifest[0]["file"]).read_bytes().startswith(b"converted:"), "converted WAV written under the manifest's file name")
        check(manifest[0]["slot"] == 0 and manifest[1]["slot"] == 1, "slots numbered in order")
        check(peaks and all(p == -3.0 for p in peaks), "every WAV is asked to be peak-normalised to -3 dBFS by default")
        print("  And    the licence matcher accepts exactly the documented licence URLs")
        for url, expected in [
            ("https://creativecommons.org/publicdomain/zero/1.0/", True),
            ("https://creativecommons.org/publicdomain/mark/1.0/", True),
            ("http://creativecommons.org/licenses/by/3.0/", True),
            ("https://creativecommons.org/licenses/by-sa/4.0/", True),
            ("https://creativecommons.org/licenses/by-nc/4.0/", False),
            ("https://creativecommons.org/licenses/by-nd/4.0/", False),
            ("https://example.com/licenses/by/4.0/", False),
            ("", False), (None, False),
        ]:
            check(harvest.licence_accepted(url) == expected, f"licence {url!r} accepted={expected}")
        print("  And    a dry run lists candidates without downloading")
        downloaded.clear()
        dry = Path(tempfile.mkdtemp(prefix="fuzzboi-harvest-dry-"))
        try:
            harvest.main(["--query", "toy", "--out", str(dry), "--dry-run"])
            check(not downloaded, "nothing downloaded in a dry run")
            check(len(json.loads((dry / "manifest.json").read_text())) == 2, "dry-run manifest lists the two accepted items")
        finally:
            shutil.rmtree(dry, ignore_errors=True)
    finally:
        shutil.rmtree(out, ignore_errors=True)
    print("  " + ("ok" if failures == 0 else "FAILED"))
    print(f"1 scenario(s), {failures} failure(s)")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
