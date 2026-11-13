# Apurrr2500 roadmap

## Done in 0.1.0 (test build)
- [x] DSP core in plain C++ with tests (`sh tests/run.sh`), identical gain reduction to the mockup engine on four scenarios
- [x] VST3 plugin with the native editor ported from the approved mockup (generated Layout.h), resizable, presets, Guide, Settings, About
- [x] CI on Linux, Windows, macOS; installers, manual, release workflow

## Verified (measured here)
- Core: ranges, static curve vs measured reduction, latency 0, knob-to-sound within one sample, attack and release time constants, stability on silence, DC, full scale, NaN and Inf, aliasing/distortion figures, CPU, sample rates 44.1 to 192 kHz
- Plugin classes (ApurrSnapshot --selftest): parameter mapping for every choice item, state round trip, old-state fixture, bit-exact Bypass and IN off, Ext SC side-chain, editor create and resize

## Not verified
- Calibration (0 dBu = -22 dBFS, needle 0 VU = -18 dBFS) comes from the design and was never compared with a real hardware unit
- Sound and feel by ear against other compressors
- Installers on real Windows and macOS machines; Flatpak extension in a real Flatpak DAW; plugin in real hosts
- Resizing, HiDPI and keyboard focus in real hosts

## Done in v0.1.2
- The VST3 now carries its icon: macOS bundle Resources/icon.icns plus CFBundleIconFile, Windows icon.ico and desktop.ini inside the plugin folder (installer marks the folder read only so Explorer shows it). Built in CI only; not seen on real machines yet.

## Old vs New text (settled)
The mockup text used to say Type Old lets a sudden hit through for a moment. The engine measures the opposite at fast attacks (0.3 ms attack, 0.7 ms into a burst: New 2.2 dB, Old 6.8 dB; at 10 ms: 8.92 vs 9.78 dB; both settle at 9.08 dB), so the mockup, plugin and manual text now follow the measurement. Not judged by ear yet.

## Later
- Notarised and signed installers if funding allows
- Listening tests and tuning of presets
