#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Tomas Laurenzo
"""Install, discover and validate the AU on an isolated GitHub-hosted macOS runner."""
import argparse
import json
import os
from pathlib import Path
import plistlib
import signal
import subprocess
import sys
import time


COMPONENT_NAME = 'The Three Body Solution.component'
EXECUTABLE_NAME = 'The Three Body Solution'
BUNDLE_ID = 'com.tomaslaurenzo.threebodysolution.au'
COMPONENT_IDS = ('aumi', 'Tbs1', 'Tmlz')
DISCOVERY_SECONDS = 45.0


def verify_metadata(metadata):
    if not isinstance(metadata, dict):
        raise ValueError('Expected an AU property-list dictionary')
    if metadata.get('CFBundleIdentifier') != BUNDLE_ID:
        raise ValueError(f'Unexpected AU bundle identifier: {metadata.get("CFBundleIdentifier")!r}')
    if metadata.get('CFBundleExecutable') != EXECUTABLE_NAME:
        raise ValueError(f'Unexpected AU executable: {metadata.get("CFBundleExecutable")!r}')
    components = metadata.get('AudioComponents')
    if not isinstance(components, list) or len(components) != 1 or not isinstance(components[0], dict):
        raise ValueError('Expected exactly one AudioComponents entry')
    component = components[0]
    actual = tuple(component.get(key) for key in ('type', 'subtype', 'manufacturer'))
    if actual != COMPONENT_IDS:
        raise ValueError(f'Unexpected AU identifiers: {actual!r}; expected {COMPONENT_IDS!r}')
    if not isinstance(component.get('factoryFunction'), str) or not component['factoryFunction']:
        raise ValueError('Missing AU factory function')


def contains_component(listing):
    # auval -a lists type, subtype and manufacturer as its first three columns.
    return any(tuple(line.split()[:3]) == COMPONENT_IDS for line in listing.splitlines())


def run_logged(command, destination, *, timeout=60.0, allowed=(0,), echo=False):
    with destination.open('a', encoding='utf-8') as log:
        log.write(f'COMMAND {json.dumps(command)}\n')
        log.flush()
        try:
            result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                    text=True, errors='replace', timeout=timeout, check=False)
        except subprocess.TimeoutExpired as error:
            output = error.stdout or ''
            if isinstance(output, bytes):
                output = output.decode('utf-8', errors='replace')
            log.write(output + f'\nTIMEOUT after {timeout:.3f} seconds\n')
            raise RuntimeError(f'Command timed out; see {destination}') from error
        log.write(result.stdout + f'\nEXIT {result.returncode}\n')
    if echo:
        print(result.stdout, end='', flush=True)
    if result.returncode not in allowed:
        raise RuntimeError(f'Command failed with exit {result.returncode}; see {destination}')
    return result


def refresh_registrar(destination):
    # Only this isolated runner user's registrar can be selected. pgrep exit 1
    # means there is no existing process; other errors remain failures.
    # Match the full argument vector rather than a potentially truncated name.
    result = run_logged(['/usr/bin/pgrep', '-u', str(os.geteuid()), '-f',
                         '(^|/)AudioComponentRegistrar([[:space:]]|$)'],
                        destination, allowed=(0, 1))
    if result.returncode == 1:
        with destination.open('a', encoding='utf-8') as log:
            log.write('No existing per-user registrar; discovery will start it.\n')
        return
    for entry in result.stdout.split():
        pid = int(entry)
        if pid <= 1:
            raise RuntimeError(f'Invalid registrar PID: {pid}')
        with destination.open('a', encoding='utf-8') as log:
            try:
                os.kill(pid, signal.SIGKILL)
                log.write(f'Refreshed per-user AudioComponentRegistrar PID {pid}.\n')
            except ProcessLookupError:
                log.write(f'Registrar PID {pid} exited before refresh.\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--component', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if (sys.platform != 'darwin' or os.environ.get('GITHUB_ACTIONS') != 'true'
            or os.environ.get('RUNNER_ENVIRONMENT') != 'github-hosted'):
        raise RuntimeError('AU installation/registrar refresh requires a GitHub-hosted macOS runner')
    source = args.component.resolve()
    if source.name != COMPONENT_NAME or not source.is_dir():
        raise RuntimeError(f'Missing expected AU bundle: {source}')
    args.output.mkdir(parents=True, exist_ok=True)
    install_log = args.output / 'au-installation.txt'
    installed = Path.home() / 'Library/Audio/Plug-Ins/Components' / COMPONENT_NAME
    installed.parent.mkdir(parents=True, exist_ok=True)
    run_logged(['/usr/bin/ditto', str(source), str(installed)], install_log)

    with (installed / 'Contents/Info.plist').open('rb') as stream:
        metadata = plistlib.load(stream)
    (args.output / 'au-installed-plist.json').write_text(json.dumps(metadata, indent=2) + '\n', encoding='utf-8')
    verify_metadata(metadata)
    binary = installed / 'Contents/MacOS' / EXECUTABLE_NAME
    architectures = run_logged(['/usr/bin/lipo', '-archs', str(binary)], install_log).stdout.split()
    if architectures != ['arm64']:
        raise RuntimeError(f'Unexpected installed AU architectures: {architectures!r}')
    run_logged(['/usr/bin/codesign', '--display', '--verbose=4', str(installed)], install_log)
    run_logged(['/usr/bin/codesign', '--verify', '--deep', '--strict', '--verbose=4', str(installed)], install_log)
    print(f'Installed AU metadata, arm64 architecture and signature verified: {installed}', flush=True)

    refresh_registrar(args.output / 'au-registrar.txt')
    discovery_log = args.output / 'au-discovery.txt'
    deadline = time.monotonic() + DISCOVERY_SECONDS
    auval = ['/usr/bin/arch', '-arm64', '/usr/bin/auval']
    while True:
        remaining = deadline - time.monotonic()
        if remaining <= 0.0:
            raise RuntimeError(f'AU {COMPONENT_IDS!r} was not discovered within {DISCOVERY_SECONDS:g} seconds; '
                               f'see {discovery_log}')
        listing = run_logged(auval + ['-a'], discovery_log, timeout=min(15.0, remaining)).stdout
        if contains_component(listing):
            break
        time.sleep(min(1.0, max(0.0, deadline - time.monotonic())))
    print(f'Discovered exact AU identifiers {COMPONENT_IDS!r}; running authoritative validation once.', flush=True)
    run_logged(auval + ['-v', *COMPONENT_IDS], args.output / 'auval.txt', timeout=180.0, echo=True)


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, ValueError) as error:
        raise SystemExit(str(error)) from error
