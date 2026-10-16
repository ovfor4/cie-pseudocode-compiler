#!/usr/bin/env python3
"""Require identical generated input bytes across platforms and Perl versions."""

import hashlib
import json
from pathlib import Path
import subprocess
import sys


directory = Path(__file__).resolve().parent / "ii"
vectors = json.loads((directory / "generator-vectors.json").read_text(encoding="utf-8"))
for name, digest in vectors.items():
    command = [sys.argv[1], str(directory / name / "generate.pl")]
    outputs = []
    for seed in ["0" * 40, "0" * 40, "1" * 40]:
        result = subprocess.run([*command, seed, "100"], check=True, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, timeout=10)
        if result.stderr:
            raise RuntimeError(f"{name}: unexpected stderr {result.stderr!r}")
        outputs.append(result.stdout)
    if outputs[0] != outputs[1] or outputs[0] == outputs[2]:
        raise RuntimeError(f"{name}: seed reproducibility failed")
    if hashlib.sha256(outputs[0]).hexdigest() != digest:
        raise RuntimeError(f"{name}: generated input differs from the portable test vector")
    print(f"PASS {name}: repeatable, seed-sensitive, portable bytes")
