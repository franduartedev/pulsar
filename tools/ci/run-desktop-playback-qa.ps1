#!/usr/bin/env pwsh
# Hardware playback QA for the packaged PULSAR desktop.
#
# Generates an isolated synthetic 8s audiovisual fixture whose audio is 4s of
# 440 Hz followed by 4s of 880 Hz, writes a persisted project that cuts it at
# 4s, plays it through the real executable, and checks the mixed PCM captured
# before the audio device plus the playback metrics.
#
# The PCM is checked spectrally: content must be 440 Hz before the cut and
# 880 Hz after it, so a cut mapped to the wrong source range fails even though
# the output is continuous. This is pre-device PCM; physical audibility is not
# validated here (that needs a loopback recording).
#
# CI runners expose no audio endpoint, so the renderer fills its buffer and the
# timeline never reaches the 4s cut. Run this script on a machine with audio
# hardware for split continuity and device-paced playback evidence.
#
# Usage:
#   pwsh tools/ci/run-desktop-playback-qa.ps1 -Exe <path to pulsar-desktop.exe>
#   pwsh tools/ci/run-desktop-playback-qa.ps1 -Exe ... -Offscreen   # no window

param(
  [Parameter(Mandatory)][string]$Exe,
  [string]$Workspace = (Join-Path $env:TEMP 'pulsar-desktop-qa'),
  [int]$PlaybackMs = 8000,
  [int]$PauseAfterMs = 2600,
  [int]$ResumeAfterMs = 3400,
  [int]$ExitMs = 11000,
  [switch]$Offscreen
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $Exe -PathType Leaf)) { throw "Executable not found: $Exe" }
if ($env:PULSAR_TEST_FFMPEG_CLI) {
  $ffmpeg = $env:PULSAR_TEST_FFMPEG_CLI
} else {
  $ffmpeg = (Get-Command ffmpeg -ErrorAction Stop).Source
}
$ffprobe = Join-Path (Split-Path -Parent $ffmpeg) 'ffprobe.exe'
if (-not (Test-Path -LiteralPath $ffprobe -PathType Leaf)) {
  $ffprobe = (Get-Command ffprobe -ErrorAction Stop).Source
}

Remove-Item -Recurse -Force $Workspace -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $Workspace | Out-Null

$media = Join-Path $Workspace 'synthetic audio [QA] #1.mp4'
& $ffmpeg -y -hide_banner -loglevel error `
  -f lavfi -i 'testsrc=size=320x180:rate=30' `
  -f lavfi -i 'sine=frequency=440:sample_rate=48000:duration=4' `
  -f lavfi -i 'sine=frequency=880:sample_rate=48000:duration=4' `
  -filter_complex '[1:a][2:a]concat=n=2:v=0:a=1[a]' `
  -map 0:v -map '[a]' -t 8 `
  -c:v mpeg4 -q:v 5 -pix_fmt yuv420p -c:a aac -b:a 128k $media
if ($LASTEXITCODE -ne 0) { throw "Fixture generation failed with exit code $LASTEXITCODE." }
if (-not (Test-Path -LiteralPath $media -PathType Leaf)) { throw "Fixture was not created at $media" }
$mediaLength = (Get-Item -LiteralPath $media).Length
$streamTypes = @(& $ffprobe -v error -show_entries stream=codec_type -of csv=p=0 $media |
  ForEach-Object { $_.Trim() } | Where-Object { $_ })
if ($streamTypes -notcontains 'video' -or $streamTypes -notcontains 'audio') {
  throw "Fixture streams are incomplete: $($streamTypes -join ', ')"
}
Write-Host "fixture: $mediaLength bytes; streams: $($streamTypes -join ', ')"

$mediaLocator = $media.Replace('\', '/')
$project = Join-Path $Workspace 'persisted cut [QA] #1.pulsar'
$projectLines = @(
  'PULSAR_PROJECT 1'
  'TIMELINE 4'
  'TRACK 8 VIDEO'
  'VIDEO_TRACK 8'
  "ASSET 2 0 8 1 1 `"$mediaLocator`" `"mpeg4`" 320 180 1 15360 30 1 `"mov,mp4,m4a,3gp,3g2,mj2`" AUDIO 1 48000 1 1 48000 8 1 1"
  'CLIP 20 8 2 0 1 1 4 1 1 0 1 1'
  'CLIP 21 8 2 4 1 1 8 1 1 4 1 1'
  'ALLOCATOR 10 30 5 9'
  'END'
)
[IO.File]::WriteAllText($project, ($projectLines -join "`n") + "`n", (New-Object Text.UTF8Encoding($false)))

$shot = Join-Path $Workspace 'persisted-cut.png'
$pcm = Join-Path $Workspace 'playback-pcm.log'
$stdout = Join-Path $Workspace 'stdout.log'
$stderr = Join-Path $Workspace 'stderr.log'

$oldPlatform = $env:QT_QPA_PLATFORM
$oldWorkspace = $env:PULSAR_QA_WORKSPACE
$oldPcm = $env:PULSAR_QA_PCM_PATH
$env:PULSAR_QA_WORKSPACE = $Workspace
$env:PULSAR_QA_PCM_PATH = $pcm
if ($Offscreen) { $env:QT_QPA_PLATFORM = 'offscreen' }
else { Remove-Item Env:\QT_QPA_PLATFORM -ErrorAction SilentlyContinue }

try {
  $arguments = '"{0}" --smoke-size 1280x720 --smoke-exit-ms {1} --smoke-screenshot "{2}" --qa-playback-ms {3} --qa-pause-after-ms {4} --qa-resume-after-ms {5} --dump-performance-on-exit' -f $project, $ExitMs, $shot, $PlaybackMs, $PauseAfterMs, $ResumeAfterMs
  $process = Start-Process -FilePath $Exe -ArgumentList $arguments `
    -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru -Wait
} finally {
  if ($null -ne $oldPlatform) { $env:QT_QPA_PLATFORM = $oldPlatform } else { Remove-Item Env:\QT_QPA_PLATFORM -ErrorAction SilentlyContinue }
  if ($null -ne $oldWorkspace) { $env:PULSAR_QA_WORKSPACE = $oldWorkspace } else { Remove-Item Env:\PULSAR_QA_WORKSPACE -ErrorAction SilentlyContinue }
  if ($null -ne $oldPcm) { $env:PULSAR_QA_PCM_PATH = $oldPcm } else { Remove-Item Env:\PULSAR_QA_PCM_PATH -ErrorAction SilentlyContinue }
}

if ($process.ExitCode -ne 0) { throw "Packaged desktop exited with code $($process.ExitCode)." }
if (-not (Test-Path -LiteralPath $shot -PathType Leaf)) { throw "No screenshot was produced." }
$errorText = Get-Content -LiteralPath $stderr -Raw
if ($errorText -match 'open project failed|media file does not exist|cannot decode project') {
  throw "The packaged desktop failed to open the persisted cut project: $errorText"
}
# Guard the Inspector Canvas repaint regression: this TypeError means a
# Connections handler resolved requestPaint on the wrong object again.
if ($errorText -match "Property 'requestPaint' of object") {
  throw "The packaged desktop painted a Canvas through a non-Canvas object: $errorText"
}

$metrics = Get-Content -LiteralPath $stdout -Raw
Write-Host '--- playback metrics ---'
Write-Host $metrics
if ($metrics -notmatch 'audio.source_opens=[1-9][0-9]*') { throw "No audio source was opened." }
if ($metrics -notmatch 'audio.render_p95_us=[1-9][0-9]*') { throw "No measurable PCM render." }

if (-not (Test-Path -LiteralPath $pcm -PathType Leaf) -or (Get-Item -LiteralPath $pcm).Length -lt 100) {
  throw "PCM capture is missing or empty."
}
$pcmText = Get-Content -LiteralPath $pcm -Raw
$blockCount = ([regex]::Matches($pcmText, '(?m)^BLOCK ')).Count
$positions = @([regex]::Matches($pcmText, '(?m)^BLOCK timeline=(-?\d+)/(\d+)/(\d+) frames=(\d+)') |
  ForEach-Object { [double]$_.Groups[1].Value * [double]$_.Groups[2].Value / [double]$_.Groups[3].Value })
$pcmStart = ($positions | Measure-Object -Minimum).Minimum
$pcmEnd = ($positions | Measure-Object -Maximum).Maximum
Write-Host ("PCM timeline: {0:F3}s -> {1:F3}s; blocks: {2}; screenshot: {3} bytes" -f
  $pcmStart, $pcmEnd, $blockCount, (Get-Item -LiteralPath $shot).Length)

if (-not ($positions | Where-Object { $_ -lt 4.0 })) {
  throw "PCM never rendered audio before the 4s split (timeline $pcmStart -> $pcmEnd)."
}
if (-not ($positions | Where-Object { $_ -ge 4.0 })) {
  throw "PCM never rendered audio after the 4s cut (timeline $pcmStart -> $pcmEnd). Set -Offscreen only for smoke checks; split continuity needs a consuming audio device."
}

# Continuity is not enough: the fixture is 440 Hz before 4s and 880 Hz after it,
# so the captured PCM must change spectral content at the cut.
$spectral = Join-Path $PSScriptRoot 'assert-pcm-spectral-cut.ps1'
& $spectral -PcmPath $pcm

Write-Host 'Playback QA passed: audio rendered across the 4s cut with PCM evidence.'
Write-Host 'Spectral check passed: 440 Hz before the cut, 880 Hz after the cut.'
Write-Host 'Physical audibility is NOT validated here (pre-device PCM); use a loopback recording for that.'
Write-Host "Evidence directory: $Workspace"
