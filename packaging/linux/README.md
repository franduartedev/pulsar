# Linux packaging

`./packaging/linux/build-release.sh` builds a Release configuration and produces, in `dist/`:

- `PULSAR-<version>-linux-x86_64.tar.gz` — portable bundle (`./pulsar` launcher; no install),
- `PULSAR-<version>-x86_64.AppImage` — single-file build (needs FUSE 2 on the host),
- `SHA256SUMS`.

It uses `linuxdeploy` + the Qt plugin (downloaded, SHA-256 pinned in the script), bundles Qt 6,
FFmpeg and their dependencies, adds the sound pack under `usr/share/pulsar/assets`, prunes the
GTK platform theme and records the shipped Debian packages and their copyright files under
`usr/share/doc/pulsar-third-party/`. Nothing is uploaded or published.

Requirements on the build machine: the build dependencies from the top-level README plus
`qmlimportscanner` and the QML modules the app loads (the script verifies them).
Built on Ubuntu 24.04, so the result needs glibc ≥ 2.39.

Files: `pulsar-launcher.sh`, `pulsar.desktop`, `pulsar.svg`, `pulsar-256.png`,
`qml-bundle-hints.qml` (makes the scanner include modules only used by file dialogs).
