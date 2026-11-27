#!/usr/bin/env python3
"""Download and SHA256-verify pinned third-party archives using the OS TLS trust store."""

from __future__ import annotations

import argparse
import hashlib
import pathlib
import sys
import urllib.request


def sha256_file(path: pathlib.Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        while chunk := f.read(1024 * 1024):
            h.update(chunk)
    return h.hexdigest().lower()


def main() -> None:
    parser = argparse.ArgumentParser(description="Fetch and SHA256-verify a pinned dependency")
    parser.add_argument("--url", required=True)
    parser.add_argument("--dest", type=pathlib.Path, required=True)
    parser.add_argument("--sha256", required=True)
    args = parser.parse_args()

    expected = args.sha256.strip().lower()
    args.dest.parent.mkdir(parents=True, exist_ok=True)

    if args.dest.exists():
        actual = sha256_file(args.dest)
        if actual == expected:
            return
        args.dest.unlink()

    tmp = args.dest.with_suffix(args.dest.suffix + ".part")
    print(f"[fetch_deps] Downloading {args.url} -> {args.dest.name}", file=sys.stderr)
    req = urllib.request.Request(args.url, headers={"User-Agent": "stem-engine-build/0.1"})
    with urllib.request.urlopen(req, timeout=120) as resp, tmp.open("wb") as out:
        while chunk := resp.read(256 * 1024):
            out.write(chunk)

    actual = sha256_file(tmp)
    if actual != expected:
        tmp.unlink(missing_ok=True)
        msg = f"SHA256 mismatch for {args.url}: expected {expected}, got {actual}"
        raise RuntimeError(msg)

    tmp.replace(args.dest)


if __name__ == "__main__":
    main()
