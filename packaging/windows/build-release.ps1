param(
    [string]$BuildDir = "build",
    [string]$OutputDir = "dist",
    [string]$VcpkgRoot = $env:VCPKG_INSTALLATION_ROOT
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$build = (Resolve-Path (Join-Path $root $BuildDir)).Path
$stage = Join-Path $build "packaging-stage/PULSAR"
$dist = Join-Path $root $OutputDir

function Require-Command([string]$name) {
    if (-not (Get-Command $name -ErrorAction SilentlyContinue)) {
        throw "Required command not found: $name"
    }
}

Require-Command "cmake"
Require-Command "windeployqt"

$exe = Join-Path $stage "bin/pulsar-desktop.exe"
if (-not (Test-Path $exe)) {
    New-Item -ItemType Directory -Force -Path $stage | Out-Null
    & cmake --install $build --prefix $stage
}
if (-not (Test-Path $exe)) { throw "PULSAR executable not found after cmake --install: $exe" }

& windeployqt --release --compiler-runtime --no-translations --qmldir (Join-Path $root "apps/pulsar-desktop/qml") $exe

if ([string]::IsNullOrWhiteSpace($VcpkgRoot)) {
    $VcpkgRoot = $env:VCPKG_ROOT
}
$ffmpegBin = if ($VcpkgRoot) { Join-Path $VcpkgRoot "installed/x64-windows/bin" } else { $null }
if (-not $ffmpegBin -or -not (Test-Path $ffmpegBin)) {
    throw "FFmpeg runtime directory not found. Pass -VcpkgRoot or set VCPKG_INSTALLATION_ROOT."
}

Get-ChildItem $ffmpegBin -File -Filter "*.dll" |
    Where-Object { $_.Name -match "^(av|sw|postproc|vulkan|zimg|x264|libx264)" } |
    Copy-Item -Destination (Split-Path $exe) -Force

Copy-Item (Join-Path $root "LICENSE") (Join-Path $stage "LICENSE.txt") -Force
Copy-Item (Join-Path $root "THIRD_PARTY_NOTICES.md") (Join-Path $stage "THIRD_PARTY_NOTICES.md") -Force
Copy-Item (Join-Path $root "CHANGELOG.md") (Join-Path $stage "CHANGELOG.md") -Force

New-Item -ItemType Directory -Force -Path $dist | Out-Null
$versionLine = & $exe --version | Select-Object -First 1
$version = ($versionLine -replace "^PULSAR\s+", "").Trim()
if ([string]::IsNullOrWhiteSpace($version)) { throw "Could not determine PULSAR version." }
$archive = Join-Path $dist ("PULSAR-{0}-windows-x86_64.zip" -f $version)
if (Test-Path $archive) { Remove-Item -LiteralPath $archive -Force }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $archive -CompressionLevel Optimal
Write-Host "Created $archive"
