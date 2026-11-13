Apurrr2500 test build (pre-release). A free, open source stereo bus compressor in the style of a classic hardware unit. By Studio 87.

One zip, everything works offline:
- Windows installer (.exe) with uninstaller
- macOS installer (.pkg) with an "Uninstall Apurrr2500" app
- Linux: Flatpak extension for Flatpak DAWs, and a plain .vst3 tarball
- PDF manual with an interface tour, INSTALL.txt, LICENSE and SHA256SUMS

About the "unknown publisher" warnings: the installers are not signed (signing needs a paid certificate; this is a free project). Windows may show "Windows protected your PC": click More info, then Run anyway. macOS may block the package: open System Settings, Privacy & Security, and choose Open Anyway. The checksums are in SHA256SUMS and the source is in this repository.

What is verified: the compressor core is tested in plain C++ (ranges, level calibration, response time, stability on silence, DC, full scale and NaN, aliasing, CPU, sample rates) and matches the approved mockup's engine. The plugin loads in pluginval on Linux.

What is not verified: the installers have not been run on real Windows or macOS machines yet, the plugin has not been tried in real hosts on every system, and the calibration (0 dBu = -22 dBFS, 0 VU = -18 dBFS) comes from the design mockup and has not been checked against a real hardware unit. See ROADMAP.md.

Free to use. Collects nothing. Optional support: https://ko-fi.com/studio87

New in 0.1.2: the VST3 carries its own icon on macOS and Windows.
