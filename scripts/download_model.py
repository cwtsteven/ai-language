#!/usr/bin/env python3
"""Download a pinned GGUF model, resuming interrupted downloads."""

import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess
import sys

MODELS = {
    "qwen3.5-9b": (
        "unsloth/Qwen3.5-9B-GGUF", "99a1b2185534379e6e8b5ec869da25d3e7b3f73c",
        "Qwen3.5-9B-Q4_K_M.gguf",
        "03b74727a860a56338e042c4420bb3f04b2fec5734175f4cb9fa853daf52b7e8", "5.68",
    ),
    "gpt-oss-20b": (
        "unsloth/gpt-oss-20b-GGUF", "d449b42d93e1c2c7bda5312f5c25c8fb91dfa9b4",
        "gpt-oss-20b-Q4_K_M.gguf",
        "c27536640e410032865dc68781d80a08b98f8db5e93575919af8ccc0568aeb4f", "11.62",
    ),
}


def verify(path, expected_sha256):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(8 * 1024 * 1024), b""):
            digest.update(chunk)
    if digest.hexdigest() != expected_sha256:
        raise ValueError(f"SHA-256 mismatch: {path}. Move or delete this file and retry.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", choices=MODELS, default="qwen3.5-9b")
    parser.add_argument("--output-dir", type=Path,
                        default=Path(__file__).resolve().parents[1] / "models",
                        help="Model directory; may be on an external SSD")
    args = parser.parse_args()
    repository, revision, filename, sha256, size_gb = MODELS[args.model]
    url = f"https://huggingface.co/{repository}/resolve/{revision}/{filename}"
    directory = args.output_dir.expanduser().resolve()
    directory.mkdir(parents=True, exist_ok=True)
    target = directory / filename
    if target.exists():
        print(f"Verifying existing model: {target}", flush=True)
        verify(target, sha256)
    else:
        if not shutil.which("curl"):
            raise RuntimeError("curl is required; see the README installation instructions.")
        partial = target.with_suffix(target.suffix + ".part")
        print(f"Downloading approximately {size_gb} GB to {partial}", flush=True)
        subprocess.run(["curl", "--fail", "--location", "--retry", "3",
                        "--continue-at", "-", "--output", str(partial), url], check=True)
        print("Verifying SHA-256...", flush=True)
        verify(partial, sha256)
        partial.replace(target)
    print(f"Verified model: {target}")
    print("Use this path with --model if it is outside the project's models directory.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"error: {error}", file=sys.stderr)
        sys.exit(1)
