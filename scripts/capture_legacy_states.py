#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Tomas Laurenzo
"""Build a fixture producer against the exact audited source, outside the source tree."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--build', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
commit = subprocess.check_output(['git', '-C', str(args.source), 'rev-parse', 'HEAD'], text=True).strip()
expected = '06066967b243e250d97da37ea0195b31f5c9521c'
if commit != expected:
    raise SystemExit(f'Fixture producer must be pinned to {expected}, found {commit}')
shutil.copyfile(root / 'tests/CaptureLegacyStates.cpp', args.source / 'tests/CaptureLegacyStates.cpp')
cmake = args.source / 'src/plugin/CMakeLists.txt'
cmake.write_text(cmake.read_text() + '''
add_executable(threebs_capture_legacy ../../tests/CaptureLegacyStates.cpp)
target_include_directories(threebs_capture_legacy PRIVATE ..)
target_compile_definitions(threebs_capture_legacy PRIVATE THREEBS_AU_MIDI_EFFECT=0)
target_link_libraries(threebs_capture_legacy PRIVATE ThreeBSVST3 threebs_presentation juce::juce_audio_utils juce::juce_gui_extra)
''')
subprocess.run(['cmake', '-S', str(args.source), '-B', str(args.build), '-DCMAKE_BUILD_TYPE=Release',
                '-DCMAKE_OSX_ARCHITECTURES=arm64', '-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0',
                '-DTHREEBS_BUILD_PLUGINS=ON', '-DTHREEBS_BUILD_TESTS=OFF'], check=True)
subprocess.run(['cmake', '--build', str(args.build), '--target', 'threebs_capture_legacy', '--parallel', '4'], check=True)
args.output.mkdir(parents=True, exist_ok=True)
subprocess.run([str(args.build / 'src/plugin/threebs_capture_legacy'), str(args.output.resolve())], check=True)
manifest = {'producer_commit': commit, 'producer_kind': 'actual prior-version executable with added capture harness',
            'host_schema': 7, 'portable_schema': 3,
            'scope': 'generated compatibility fixtures; no claim these are historical user sessions',
            'files': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in args.output.iterdir() if p.is_file()}}
(args.output / 'PROVENANCE.json').write_text(json.dumps(manifest, indent=2) + '\n')
