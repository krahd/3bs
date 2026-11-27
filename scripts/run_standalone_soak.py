#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Tomas Laurenzo
"""Run the real CoreMIDI duration gate and record an explicit pass/fail/skip."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import time
parser = argparse.ArgumentParser()
parser.add_argument('--binary', required=True)
parser.add_argument('--seconds', type=int, default=1800)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
if not 1 <= args.seconds <= 1800:
    parser.error('--seconds must be 1..1800')
args.output.mkdir(parents=True, exist_ok=True)
environment = dict(os.environ, THREEBS_STANDALONE_STRESS_SECONDS=str(args.seconds))
started = time.monotonic()
measurement = None
with (args.output / 'standalone-soak.log').open('w') as log:
    process = subprocess.Popen([args.binary], env=environment, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, text=True)
    for line in process.stdout:
        print(line, end='', flush=True)
        log.write(line)
        log.flush()
        if line.startswith('{"standalone_runtime":'):
            measurement = json.loads(line)['standalone_runtime']
    result = process.wait()
status = 'passed' if result == 0 else 'skipped' if result == 77 else 'failed'
measurement_complete = measurement is not None and measurement.get('stress_seconds', 0) >= args.seconds
if result == 0 and not measurement_complete:
    status = 'failed'
record = {'status': status, 'requested_seconds': args.seconds, 'elapsed_seconds': time.monotonic() - started,
          'exit_code': result, 'measurement': measurement,
          'scope': 'actual virtual CoreMIDI delivery while message-thread dispatch is stalled; physical-device and DAW sessions remain separate'}
(args.output / 'standalone-soak.json').write_text(json.dumps(record, indent=2) + '\n')
if result == 0 and not measurement_complete:
    raise SystemExit('Missing full-duration measurement')
raise SystemExit(0 if result in (0, 77) else result)
