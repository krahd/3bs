#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Tomas Laurenzo
"""Package an already tested macOS candidate, without altering published releases."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--build', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--commit', required=True)
args = parser.parse_args()
if not args.commit or any(c not in '0123456789abcdef' for c in args.commit) or len(args.commit) != 40:
    parser.error('--commit must be a full Git SHA')
root = Path(__file__).resolve().parents[1]
args.output.mkdir(parents=True, exist_ok=True)
bundles = {
    'AU': args.build / 'src/plugin/ThreeBSAU_artefacts/Release/AU/The Three Body Solution.component',
    'VST3': args.build / 'src/plugin/ThreeBSVST3_artefacts/Release/VST3/The Three Body Solution.vst3',
    'Standalone': args.build / 'src/standalone/ThreeBSStandalone_artefacts/Release/The Three Body Solution.app',
}
for bundle in bundles.values():
    if not bundle.is_dir():
        raise SystemExit(f'Missing bundle: {bundle}')
    subprocess.run(['/usr/bin/codesign', '--verify', '--deep', '--strict', str(bundle)], check=True)
provenance = {
    'commit': args.commit, 'kind': 'development candidate', 'platform': 'arm64 macOS 13+',
    'build': 'Release', 'signing': 'ad hoc; not notarised',
    'machine': platform.platform(),
    'juce': subprocess.check_output(['git', '-C', str(root), 'submodule', 'status'], text=True).strip(),
    'qualification': 'Automated CI evidence is separate; Logic/Ableton and physical routing require recorded host sessions.',
}
checksums = []
for name, selected in [(key, [path]) for key, path in bundles.items()] + [('Complete', list(bundles.values()))]:
    with tempfile.TemporaryDirectory(prefix='threebs-candidate-') as stage:
        folder = Path(stage) / 'The Three Body Solution'
        folder.mkdir()
        for source in selected:
            subprocess.run(['/usr/bin/ditto', str(source), str(folder / source.name)], check=True)
        for source in ['LICENSE', 'THIRD_PARTY_NOTICES.md', 'README.md', 'RELIABILITY-ACCEPTANCE.md']:
            (folder / Path(source).name).write_bytes((root / source).read_bytes())
        (folder / 'BUILD-PROVENANCE.json').write_text(json.dumps(provenance, indent=2) + '\n')
        archive = args.output.resolve() / f'ThreeBS-{name}-{args.commit[:12]}-macOS-arm64.zip'
        if archive.exists():
            raise SystemExit(f'Refusing to overwrite candidate archive: {archive}')
        subprocess.run(['/usr/bin/ditto', '-c', '-k', '--sequesterRsrc', '--keepParent', str(folder), str(archive)], check=True)
        checksums.append(f'{hashlib.sha256(archive.read_bytes()).hexdigest()}  {archive.name}')
(args.output / 'SHA256SUMS.txt').write_text('\n'.join(checksums) + '\n')
(args.output / 'BUILD-PROVENANCE.json').write_text(json.dumps(provenance, indent=2) + '\n')
print('\n'.join(checksums))
