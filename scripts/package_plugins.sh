#!/bin/zsh
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Tomas Laurenzo

set -euo pipefail

if (( $# != 1 )); then
    printf 'Usage: %s v<version>\n' "$0" >&2
    exit 2
fi

version=$1
if [[ ! $version =~ '^v[0-9]+\.[0-9]+\.[0-9]+([-.][0-9A-Za-z.]+)?$' ]]; then
    printf 'Invalid release version: %s\n' "$version" >&2
    exit 2
fi

root_dir=${0:A:h:h}
build_dir="$root_dir/build/release"
output_dir="$root_dir/dist/$version"
stage_dir="$output_dir/.stage"
version_name=${version#v}
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
cmake --build "$build_dir" --parallel "$jobs"
ctest --test-dir "$build_dir" --output-on-failure

au_source="$build_dir/src/plugin/ThreeBSAU_artefacts/Release/AU/The Three Body Solution.component"
vst3_source="$build_dir/src/plugin/ThreeBSVST3_artefacts/Release/VST3/The Three Body Solution.vst3"
standalone_source="$build_dir/src/standalone/ThreeBSStandalone_artefacts/Release/The Three Body Solution.app"
for bundle in "$au_source" "$vst3_source" "$standalone_source"; do
    test -d "$bundle"
    codesign --verify --deep --strict "$bundle"
done

rm -rf "$output_dir"
mkdir -p "$stage_dir/AU" "$stage_dir/VST3" "$stage_dir/Standalone" "$stage_dir/Complete"
ditto "$au_source" "$stage_dir/AU/The Three Body Solution.component"
ditto "$vst3_source" "$stage_dir/VST3/The Three Body Solution.vst3"
ditto "$standalone_source" "$stage_dir/Standalone/The Three Body Solution.app"
ditto "$au_source" "$stage_dir/Complete/The Three Body Solution.component"
ditto "$vst3_source" "$stage_dir/Complete/The Three Body Solution.vst3"
ditto "$standalone_source" "$stage_dir/Complete/The Three Body Solution.app"

commit=$(git -C "$root_dir" rev-parse HEAD)
for format in AU VST3 Standalone Complete; do
    cp "$root_dir/LICENSE" "$stage_dir/$format/LICENSE"
    cp "$root_dir/THIRD_PARTY_NOTICES.md" "$stage_dir/$format/THIRD_PARTY_NOTICES.md"
    cp "$root_dir/doc/INSTALL.md" "$stage_dir/$format/INSTALL.md"
    cat > "$stage_dir/$format/BUILD-PROVENANCE.txt" <<EOF
The Three Body Solution $version
Git commit: $commit
Platform: arm64 macOS 13+
Configuration: Release
Signing: ad hoc
Built with: cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 -DTHREEBS_BUILD_PLUGINS=ON -DTHREEBS_BUILD_TESTS=ON -DTHREEBS_ADHOC_SIGN=ON
EOF
done

au_archive="$output_dir/ThreeBS-AU-$version_name-macOS-arm64.zip"
vst3_archive="$output_dir/ThreeBS-VST3-$version_name-macOS-arm64.zip"
standalone_archive="$output_dir/ThreeBS-Standalone-$version_name-macOS-arm64.zip"
complete_archive="$output_dir/The-Three-Body-Solution-$version_name-macOS-arm64.zip"
ditto -c -k --sequesterRsrc "$stage_dir/AU" "$au_archive"
ditto -c -k --sequesterRsrc "$stage_dir/VST3" "$vst3_archive"
ditto -c -k --sequesterRsrc "$stage_dir/Standalone" "$standalone_archive"
ditto -c -k --sequesterRsrc "$stage_dir/Complete" "$complete_archive"
rm -rf "$stage_dir"

(
    cd "$output_dir"
    shasum -a 256 "${complete_archive:t}" "${au_archive:t}" \
        "${vst3_archive:t}" "${standalone_archive:t}" > SHA256SUMS.txt
)

printf 'Release assets written to %s\n' "$output_dir"
cat "$output_dir/SHA256SUMS.txt"
