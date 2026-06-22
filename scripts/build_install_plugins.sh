#!/bin/zsh
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Tomas Laurenzo

set -euo pipefail

root_dir=${0:A:h:h}
build_dir="$root_dir/build/release"
detected_jobs=$(sysctl -n hw.logicalcpu 2>/dev/null \
    || getconf _NPROCESSORS_ONLN 2>/dev/null \
    || printf '4')
jobs=${THREEBS_BUILD_JOBS:-$detected_jobs}

cmake -S "$root_dir" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
    -DTHREEBS_BUILD_PLUGINS=ON \
    -DTHREEBS_BUILD_TESTS=ON \
    -DTHREEBS_ADHOC_SIGN=ON
cmake --build "$build_dir" --target ThreeBSAU_AU ThreeBSVST3_VST3 \
    --clean-first --parallel "$jobs"

au_source="$build_dir/src/plugin/ThreeBSAU_artefacts/Release/AU/The Three Body Solution.component"
vst3_source="$build_dir/src/plugin/ThreeBSVST3_artefacts/Release/VST3/The Three Body Solution.vst3"
au_directory="$HOME/Library/Audio/Plug-Ins/Components"
vst3_directory="$HOME/Library/Audio/Plug-Ins/VST3"
au_destination="$au_directory/The Three Body Solution.component"
vst3_destination="$vst3_directory/The Three Body Solution.vst3"

for bundle in "$au_source" "$vst3_source"; do
    test -d "$bundle"
    codesign --verify --deep --strict "$bundle"
done

mkdir -p "$au_directory" "$vst3_directory"
rm -rf "$au_destination" "$vst3_destination"
ditto "$au_source" "$au_destination"
ditto "$vst3_source" "$vst3_destination"

codesign --verify --deep --strict "$au_destination"
codesign --verify --deep --strict "$vst3_destination"
killall AudioComponentRegistrar 2>/dev/null || true

printf 'Installed Audio Unit: %s\n' "$au_destination"
printf 'Installed VST3: %s\n' "$vst3_destination"
printf 'Restart open plugin hosts or rescan plugins before testing.\n'
