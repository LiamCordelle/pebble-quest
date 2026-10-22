# Quest kit integration

Quest depends on `shared-kit`, the same Git submodule used by Mondrian and
Underground. `src/c/kit`, `src/pkjs/kit`, `lab.h` and `lab_common.c` are links
into that dependency. There is no copied shared implementation.

Release dependency: shared kit **2.1.1**, extending the published 2.1.0
consumer-settings interface with bounded text inputs. Quest uses the kit's
`faceSettings` phone configuration and existing weak C settings hooks; there
is no parallel receiver implementation. The kit is committed/shared before
Quest records the dependency gitlink. See `src/c/kit/KIT.md` for the interface.

Quest owns `HERO_NAME`, its Clay input, uppercase ASCII normalization, default
`LIAM`, eight-character limit, and watch persistence key 2000. Existing kit
settings keys 1000/1001 and AppMessage key order remain compatible. `HERO_NAME`
is appended as key 10012. A name-only update refreshes immediately, even when
all common settings are unchanged. The name persists across relaunch.

The default shell subscribes continuously to touch. Wrist flick/touch toggles
the detail view for eight seconds. Physical touch-service battery cost remains
unmeasured; `KIT_TOUCH_WINDOW_MS=10000 pebble build` is the inherited A/B option.

22 October 2026: port Quest, add its name setting and character sheet; validate
76 Quest renders, shared native/phone tests and isolated SDK/render checks of
Mondrian and Underground. Final QEMU evidence is in `shots/emulator/`.

Emulator verification lessons: use explicit `AppRunStateStop`/`Start` to prove
cold persistence; Back is not a reliable app exit on a watchface. Save matching
phone settings before restarting: a phone snapshot with weather disabled correctly
clears the watch cache. Let battery callbacks draw before capturing screenshots.
Compare main-window frames in a timeout check: both the clock and live health
can change during the wait. QEMU can
step its clock backwards, causing an unsigned frame-wall-time measurement to
wrap; preserve raw logs and report such samples as clock discontinuities.

Final QEMU verification, 22 October 2026: active Quest UUID verified; Clay's
actual response path applied WIZARD99, 12-hour time and Fahrenheit. A correlated
synthetic 48-hour forecast was accepted once. Explicit AppRunStateStop/Start
preserved the name, settings and weather window pixel for pixel with no second
injected weather reply. Timeout, critical HP, REGEN, SILENCE and reconnection
were captured. Two explicit app exits each reported `Still allocated <0B>`.
The final weather window matches the corresponding host render pixel for pixel.
The original SDK profile was restored and QEMU stopped.

Most QEMU frame wall-time samples were 6–20ms. One was 1012ms, and two unsigned
samples wrapped after backwards clock steps. Raw samples remain in
`shots/emulator/result.json` and the log. These are emulator wall times, not
physical-watch CPU measurements. Physical timing and touch power remain checks
for Liam's Time 2; do not allocate a frame cache based solely on these samples.

## Public release 1.0.1

22 October 2026: approved artwork retained pixel for pixel while moving the
hero name onto the published kit interface. The previous local 2.1.0 prototype
extension is archived in a local dependency stash rather than published as a
competing implementation. The kit's release helper was restored after a local
validation-receipt script overwrote it; workflow tests verify it before release.
Kit 2.1.1 adds only text normalization on top of the existing interface. Tests
cover defaults, types, maximum length, custom normalization, storage and Clay
ready/save; older numeric consumers retain their settings and artwork.

Release dependency pinned to `0890f40ae56e8b08da6c3e3985fd3d1eb8ea0fef` (kit 2.1.1), shared publicly before Quest's source commit.

Published Quest 1.0.1 on 22 October 2026:
https://apps.repebble.com/4b8887048b0f4b89a07ed816. The store account record
confirms the release is listed and published for Emery, with a PBW and both
approved screenshots. Source and kit commit provenance is in
`store/release-1.0.1-provenance.json`; publication metadata is in
`store/published.json`.

## 8,000-step default — 22 October 2026

Updated to shared kit 2.1.2 (`71a935d`). Fresh watch/phone settings and
face fallbacks use 8,000 steps. Valid saved goals are preserved. Protocol keys,
persistence layouts and explicit screenshot fixtures are unchanged. Shared and
consumer tests, all five render suites and fresh-profile QEMU checks cover the
new default and a saved 12,500-step goal across explicit stop/start.
