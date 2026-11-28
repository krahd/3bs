#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Tomas Laurenzo
set -euo pipefail

# Exercise the documented 3.22 minimum with Kitware's final 3.22 patch release.
# The temporary CI installation is not bundled with the application.
: "${GITHUB_PATH:?This installer requires the GitHub Actions environment}"
install_root="${1:?Supply a temporary installation directory}"
release_version=3.22.6
case "$(uname -s)" in
  Linux) archive_name="cmake-${release_version}-linux-x86_64.tar.gz"; binary_path=bin ;;
  Darwin) archive_name="cmake-${release_version}-macos-universal.tar.gz"; binary_path=CMake.app/Contents/bin ;;
  *) echo 'Unsupported CI operating system' >&2; exit 1 ;;
esac
mkdir -p "$install_root"
release_url="https://github.com/Kitware/CMake/releases/download/v${release_version}"
curl --fail --location --retry 3 "$release_url/$archive_name" -o "$install_root/$archive_name"
curl --fail --location --retry 3 "$release_url/cmake-${release_version}-SHA-256.txt" -o "$install_root/SHA-256.txt"
python3 - "$archive_name" "$install_root" <<'PY'
import hashlib
from pathlib import Path
import sys
name, root = sys.argv[1], Path(sys.argv[2])
records = [line.split() for line in (root / 'SHA-256.txt').read_text().splitlines()]
expected = [parts[0] for parts in records if len(parts) == 2 and parts[1] == name]
actual = hashlib.sha256((root / name).read_bytes()).hexdigest()
if expected != [actual]:
    raise SystemExit('Official CMake archive checksum did not match')
print(f'Verified CMake archive SHA-256: {actual}')
PY
tar -xzf "$install_root/$archive_name" --strip-components=1 -C "$install_root"
"$install_root/$binary_path/cmake" --version
printf '%s\n' "$install_root/$binary_path" >> "$GITHUB_PATH"
