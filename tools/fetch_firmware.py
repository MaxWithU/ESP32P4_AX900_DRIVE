#!/usr/bin/env python3
"""Fetch the tested vendor firmware set and verify its pinned SHA-256 hashes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile
from urllib.error import URLError
from urllib.request import Request, urlopen


FIRMWARE_DIR = Path(__file__).resolve().parents[1] / "components/ax900/firmware"


def matches(data, entry):
    return (len(data) == entry["bytes"]
            and hashlib.sha256(data).hexdigest() == entry["sha256"])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--verify-only", action="store_true",
                        help="Check existing files without accessing the network")
    parser.add_argument("--output-dir", type=Path, default=FIRMWARE_DIR)
    args = parser.parse_args()
    manifest = json.loads((FIRMWARE_DIR / "SOURCE.json").read_text())
    repo = manifest["repo"].removeprefix("https://github.com/")
    if repo == manifest["repo"] or len(repo.split("/")) != 2:
        raise ValueError("Expected a GitHub repository in SOURCE.json")
    base = f"https://raw.githubusercontent.com/{repo}/{manifest['commit']}/{manifest['path']}"
    for entry in manifest["files"]:
        name = entry["path"]
        if Path(name).name != name or name in (".", ".."):
            raise ValueError("Invalid firmware filename")
        target = args.output_dir / name
        if (target.is_file() and target.stat().st_size == entry["bytes"]
                and matches(target.read_bytes(), entry)):
            print(f"Verified {name}")
            continue
        if args.verify_only:
            raise ValueError(f"Missing or mismatched firmware: {target}")
        request = Request(f"{base}/{name}", headers={"User-Agent": "ESP32P4-AX900-Driver"})
        with urlopen(request, timeout=60) as response:
            data = response.read(entry["bytes"] + 1)
        if not matches(data, entry):
            raise ValueError(f"Size or SHA-256 mismatch: {name}")
        args.output_dir.mkdir(parents=True, exist_ok=True)
        temporary = None
        try:
            with tempfile.NamedTemporaryFile(dir=args.output_dir, delete=False) as output:
                temporary = Path(output.name)
                output.write(data)
            os.replace(temporary, target)
        finally:
            if temporary is not None:
                temporary.unlink(missing_ok=True)
        print(f"Downloaded and verified {name}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, URLError) as error:
        print(f"Firmware preparation failed: {error}", file=sys.stderr)
        sys.exit(1)
