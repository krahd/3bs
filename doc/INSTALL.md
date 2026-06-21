# Install The Three Body Solution

The `v0.1.0-alpha.1` release contains ad-hoc-signed Apple silicon binaries for
macOS 13 and newer. They are alpha builds: they are not notarized and Logic and
Ableton host validation is still pending. Download the complete bundle or an
individual format from the [GitHub release][release].

## Verify the download

Download `SHA256SUMS.txt` from the release, then run the matching command from
the directory containing the archive:

```bash
shasum -a 256 -c SHA256SUMS.txt
```

The command reports `OK` for archives present in that directory. A missing
archive is harmless when only one format was downloaded; a checksum mismatch
is not.

## Standalone

1. Move `The Three Body Solution.app` to `/Applications`.
2. Control-click the application and choose **Open** on first launch.
3. Choose `Virtual MIDI: 3bs` or another output on the Settings page.
4. Route that MIDI source to a software instrument.

Press `P` for the clean presentation view and `F` for fullscreen. The
standalone emits MIDI and does not synthesize audio itself.

## Audio Unit

Copy `3bs.component` to your user Audio Unit directory:

```bash
mkdir -p ~/Library/Audio/Plug-Ins/Components
cp -R "The Three Body Solution.component" ~/Library/Audio/Plug-Ins/Components/
killall -9 AudioComponentRegistrar 2>/dev/null || true
auval -v aumi Tbs1 Krhd
```

Insert `3bs` as a MIDI effect before a software instrument in Logic Pro. The
published alpha has not yet completed Logic validation; report the host and
macOS version with any issue.

## VST3

Copy `3bs.vst3` to your user VST3 directory:

```bash
mkdir -p ~/Library/Audio/Plug-Ins/VST3
cp -R "The Three Body Solution.vst3" ~/Library/Audio/Plug-Ins/VST3/
```

Rescan plugins in the host. In Ableton Live, place `3bs` on one MIDI track and
receive its MIDI output on an instrument track. Ableton merges internally
routed MIDI channels, so independent channel destinations require multiple
instances or a multitimbral instrument.

## Gatekeeper

The archives are ad-hoc signed but not notarized. Prefer the Finder
Control-click **Open** flow for the standalone. For plugin quarantine issues,
inspect the downloaded bundle before deciding whether to remove its quarantine
attribute:

```bash
xattr -lr "The Three Body Solution.component"
xattr -lr "The Three Body Solution.vst3"
```

Do not bypass Gatekeeper for a file whose checksum does not match the release.

## Uninstall

Remove the copied application or plugin bundle. User configuration files are
portable `.3bs` files saved only where you choose; the application does not
install a background service.

[release]: https://github.com/krahd/3bs/releases/tag/v0.1.0-alpha.1
