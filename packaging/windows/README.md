# PULSAR portable Windows build

The Windows CI produces a self-contained `PULSAR-<version>-windows-x86_64.zip`.
Extract it to a writable folder and launch `bin/pulsar-desktop.exe`; Qt, QML,
the MSVC runtime and FFmpeg DLLs are included in the bundle.

For a local release build, use a Visual Studio developer PowerShell with Qt 6,
CMake, Ninja, `windeployqt`, and vcpkg's `ffmpeg:x64-windows` port installed:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DPULSAR_BUILD_DESKTOP=ON
cmake --build build --parallel
.\packaging\windows\build-release.ps1 -BuildDir build -OutputDir dist
```

The generated archive is portable and does not modify the project format. Open
the same `.pulsar` file on Linux to verify cross-platform save/reopen behavior.
