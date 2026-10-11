#!/usr/bin/env python3
"""Harvest permissively licensed audio from archive.org into FUZZBOI sample slots.

The tool searches the archive.org advanced-search API for audio items whose licence URL is a
Creative Commons public-domain dedication or an attribution licence, downloads the audio files
of the items it picks (one per item unless --files-per-item says more, optionally only those
whose names match --name-filter), converts them to mono 16-bit WAV at the asked-for rate with
ffmpeg, trims leading silence, caps the length, normalises the peak to --peak-db so a quiet
field recording sits beside a close-miked bell, and writes a manifest recording the item, file,
title and licence of every sample so attribution travels with the audio.

    harvest_archive.py --query '"toy piano"' --query theremin --out assets/samples --max 16

Only these licence URLs are accepted (anything else is skipped and reported):
    https://creativecommons.org/publicdomain/zero/1.0/
    https://creativecommons.org/publicdomain/mark/1.0/
    https://creativecommons.org/licenses/by/{2.0,2.5,3.0,4.0}/
    https://creativecommons.org/licenses/by-sa/{2.0,2.5,3.0,4.0}/

The plug-in loads the output directory with FUZZBOI_SAMPLES=<dir>; a file named like
"jug_A3.wav" is tuned so A3 plays it at its native speed (see src/fuzzboi.cpp, root_hz_from_name).

Network access is required. Everything downloaded is untrusted data: it is written into the
output directory and only ever read back as audio.
"""
from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
import urllib.parse
import urllib.request
from pathlib import Path

SEARCH_URL = "https://archive.org/advancedsearch.php"
METADATA_URL = "https://archive.org/metadata/"
DOWNLOAD_URL = "https://archive.org/download/"

ACCEPTED_LICENCES = re.compile(
    r"^https?://creativecommons\.org/("
    r"publicdomain/(zero|mark)/1\.0|"
    r"licenses/by(-sa)?/(2\.0|2\.5|3\.0|4\.0)"
    r")/?$"
)
AUDIO_EXTENSIONS = (".wav", ".flac", ".mp3", ".ogg", ".aiff", ".aif")


def fetch_json(url: str) -> dict:
    with urllib.request.urlopen(url, timeout=60) as response:
        return json.load(response)


def search(query: str, rows: int) -> list[dict]:
    """Items of mediatype audio matching `query` and carrying any licence URL."""
    params = {
        "q": f"mediatype:audio AND licenseurl:* AND ({query})",
        "fl[]": ["identifier", "title", "licenseurl", "creator"],
        "rows": str(rows),
        "output": "json",
    }
    url = SEARCH_URL + "?" + urllib.parse.urlencode(params, doseq=True)
    return fetch_json(url).get("response", {}).get("docs", [])


def licence_accepted(url: str | None) -> bool:
    return bool(url) and ACCEPTED_LICENCES.match(url.strip()) is not None


def audio_files(identifier: str) -> list[dict]:
    files = fetch_json(METADATA_URL + identifier).get("files", [])
    chosen = [f for f in files if f.get("name", "").lower().endswith(AUDIO_EXTENSIONS)]
    # Prefer lossless originals over derivative MP3s when both exist.
    chosen.sort(key=lambda f: (f.get("source") != "original", not f["name"].lower().endswith((".wav", ".flac"))))
    return chosen


def download(identifier: str, name: str, target: Path) -> None:
    url = DOWNLOAD_URL + identifier + "/" + urllib.parse.quote(name)
    with urllib.request.urlopen(url, timeout=300) as response, open(target, "wb") as out:
        shutil.copyfileobj(response, out)


def peak_db(path: Path) -> float | None:
    """The file's peak level in dBFS as ffmpeg's volumedetect reports it, or None if it fails."""
    command = ["ffmpeg", "-nostdin", "-i", str(path), "-af", "volumedetect", "-f", "null", "-"]
    result = subprocess.run(command, check=False, capture_output=True, text=True)
    match = re.search(r"max_volume:\s*(-?[0-9.]+) dB", result.stderr)
    return float(match.group(1)) if match else None


def convert(source: Path, target: Path, rate: int, max_seconds: float, peak_target_db: float | None = -3.0) -> bool:
    """Mono 16-bit WAV, leading silence trimmed, length capped, peak normalised to
    `peak_target_db` (None leaves the level alone). Returns False if ffmpeg fails."""
    filters = f"silenceremove=start_periods=1:start_threshold=-50dB,atrim=end={max_seconds}"
    command = [
        "ffmpeg", "-nostdin", "-loglevel", "error", "-y", "-i", str(source),
        "-ac", "1", "-ar", str(rate), "-sample_fmt", "s16", "-af", filters, str(target),
    ]
    if subprocess.run(command, check=False).returncode != 0:
        return False
    if peak_target_db is None:
        return True
    # Second pass: measure the excerpt that was kept and lift or drop it to the target peak, so
    # a quiet field recording sits at the same level as a close-miked instrument.
    peak = peak_db(target)
    if peak is None:
        return True
    gain = peak_target_db - peak
    if abs(gain) < 0.5:
        return True
    tmp = target.with_suffix(".norm.wav")
    command = ["ffmpeg", "-nostdin", "-loglevel", "error", "-y", "-i", str(target), "-af", f"volume={gain:.2f}dB",
               "-sample_fmt", "s16", str(tmp)]
    if subprocess.run(command, check=False).returncode != 0:
        return True
    tmp.replace(target)
    return True


def slug(text: str) -> str:
    text = re.sub(r"[^A-Za-z0-9]+", "_", text).strip("_").lower()
    return text[:48] or "sample"


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--query", action="append", required=True, help="search term; repeatable")
    parser.add_argument("--out", type=Path, required=True, help="output directory for WAVs and manifest.json")
    parser.add_argument("--max", type=int, default=16, help="maximum number of samples to keep")
    parser.add_argument("--rate", type=int, default=32000, help="sample rate of the converted WAVs")
    parser.add_argument("--max-seconds", type=float, default=6.0, help="cap on each sample's length")
    parser.add_argument("--rows", type=int, default=50, help="search results to examine per query")
    parser.add_argument("--files-per-item", type=int, default=1, help="how many audio files to take from one item")
    parser.add_argument("--name-filter", default=None, help="regular expression a file name must match to be taken")
    parser.add_argument("--peak-db", type=float, default=-3.0, help="peak level the WAVs are normalised to (dBFS)")
    parser.add_argument("--no-normalize", action="store_true", help="keep the source level instead of normalising the peak")
    parser.add_argument("--dry-run", action="store_true", help="search, read metadata and report; download nothing")
    args = parser.parse_args(argv)

    if not args.dry_run and shutil.which("ffmpeg") is None:
        print("ffmpeg is required for conversion", file=sys.stderr)
        return 2

    args.out.mkdir(parents=True, exist_ok=True)
    raw = args.out / "raw"
    raw.mkdir(exist_ok=True)
    manifest: list[dict] = []
    skipped: list[str] = []
    seen: set[str] = set()

    for query in args.query:
        if len(manifest) >= args.max:
            break
        try:
            docs = search(query, args.rows)
        except Exception as error:  # noqa: BLE001 - report and keep going with the next query
            print(f"search failed for {query!r}: {error}", file=sys.stderr)
            continue
        for doc in docs:
            if len(manifest) >= args.max:
                break
            identifier = doc.get("identifier")
            if not identifier or identifier in seen:
                continue
            seen.add(identifier)
            licence = doc.get("licenseurl")
            if isinstance(licence, list):
                licence = licence[0] if licence else None
            if not licence_accepted(licence):
                skipped.append(f"{identifier}: licence {licence!r} not accepted")
                continue
            print(f"{identifier}  [{licence}]  {doc.get('title')}")
            try:
                files = audio_files(identifier)
            except Exception as error:  # noqa: BLE001
                skipped.append(f"{identifier}: metadata failed ({error})")
                continue
            if args.name_filter:
                files = [f for f in files if re.search(args.name_filter, f["name"])]
            if not files:
                skipped.append(f"{identifier}: no audio files")
                continue
            if args.dry_run:
                manifest.append({"identifier": identifier, "title": doc.get("title"), "license": licence, "source_file": files[0]["name"]})
                continue
            for file in files[:max(1, args.files_per_item)]:
                if len(manifest) >= args.max:
                    break
                stem = slug(identifier) if args.files_per_item <= 1 else slug(identifier + "_" + Path(file["name"]).stem)
                raw_path = raw / (stem + Path(file["name"]).suffix.lower())
                wav_path = args.out / f"{len(manifest):02d}_{stem}.wav"
                try:
                    download(identifier, file["name"], raw_path)
                except Exception as error:  # noqa: BLE001
                    skipped.append(f"{identifier}/{file['name']}: download failed ({error})")
                    continue
                if not convert(raw_path, wav_path, args.rate, args.max_seconds, None if args.no_normalize else args.peak_db):
                    skipped.append(f"{identifier}/{file['name']}: conversion failed")
                    continue
                manifest.append({
                    "slot": len(manifest),
                    "file": wav_path.name,
                    "identifier": identifier,
                    "source_file": file["name"],
                    "title": doc.get("title"),
                    "creator": doc.get("creator"),
                    "license": licence,
                    "url": f"https://archive.org/details/{identifier}",
                })

    (args.out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"\n{len(manifest)} sample(s) written to {args.out}; manifest.json records their licences")
    if skipped:
        print(f"{len(skipped)} item(s) skipped:")
        for line in skipped:
            print("  " + line)
    return 0 if manifest else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
