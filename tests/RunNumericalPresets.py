#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Tomas Laurenzo
"""Exercise the actual factory data without a second editable fixture catalogue."""
import argparse
import json
import math
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--binary", required=True)
parser.add_argument("--presets", required=True)
args = parser.parse_args()
systems = {"figureEight": 0, "hierarchical": 1, "stable": 2, "controlledChaos": 3, "unbound": 4}
policies = {"leave": 0, "respawn": 1, "prompt": 2}
with open(args.presets, encoding="utf-8") as source:
    document = json.load(source)
presets = document["presets"]
if len(presets) != 24 or len({preset["id"] for preset in presets}) != 24:
    raise SystemExit("Expected the 24 distinct published factory presets")
subprocess.run([args.binary], check=True)
for preset in presets:
    physics = preset["physics"]
    values = (preset["chaos"], physics["gravity"], physics["softening"], physics["speed"])
    if not all(isinstance(value, (int, float)) and math.isfinite(value) for value in values):
        raise SystemExit(f"Non-finite numerical fixture: {preset['id']}")
    command = [args.binary, "--preset", preset["id"], str(systems[preset["system"]]),
               str(preset["seed"]), *(str(value) for value in values),
               str(policies[physics["escapePolicy"]])]
    subprocess.run(command, check=True)
