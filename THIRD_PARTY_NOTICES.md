# Third-Party Notices

The Three Body Solution is licensed under AGPL-3.0-only. Third-party code keeps
its own licence.

## JUCE

JUCE is Copyright (c) Raw Material Software Limited and contributors. The
project uses JUCE 8.0.13 under the GNU Affero General Public License version 3.

- Project: <https://github.com/juce-framework/JUCE>
- Licence: <https://github.com/juce-framework/JUCE/blob/8.0.13/LICENSE.md>

JUCE contains dependencies under compatible licences. Their notices are kept
in the pinned JUCE source tree. Review JUCE's `LICENSE.md` and the referenced
dependency notices whenever the pinned version or selected modules change.

## Platform SDKs

The project links against Apple system frameworks and uses the Audio Unit SDK
and Steinberg VST3 SDK distributed with JUCE. Their notices remain in JUCE's
source tree. Apple SDK use is governed by the applicable Apple developer terms.

## HYG Star Database

The generated `resources/stars/hyg-v41-mag65.csv` is
derived from HYG v4.1 by David Nash and contributors. HYG data is licensed
separately under Creative Commons Attribution-ShareAlike 4.0 International and
is not covered by the 3bs AGPL licence.

- Project: <https://github.com/astronexus/HYG-Database>
- Pinned source commit: `c7f7f883fe678cc7680169a50ccd7dcc49b060ce`
- Licence: <https://creativecommons.org/licenses/by-sa/4.0/>

The checked-in generated file contains the 8,921 catalogue entries at apparent
magnitude 6.5 or brighter. The original `bright-stars.csv` remains as a
defensive fallback.

## Visual References

The Metal renderer is an original implementation informed by the layered
planet, atmosphere, star-billboard, and post-processing techniques demonstrated
by Sebastian Lague's MIT-licensed projects below. No code, textures, meshes, or
other assets from either repository are included in 3bs.

- Fluid Planet: <https://github.com/SebLague/Fluid-Planet>
- Geographical Adventures: <https://github.com/SebLague/Geographical-Adventures>
