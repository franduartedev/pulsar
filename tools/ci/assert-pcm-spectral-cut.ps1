#!/usr/bin/env pwsh
# Spectral assertion for the QA playback PCM capture.
#
# The QA fixture is 8 s of audio: 440 Hz for the first 4 s and 880 Hz after
# that, cut by the project at 4 s. Continuity alone cannot prove the cut used
# the right source range, so this script measures both tones before and after
# the cut with a Hann-windowed single-bin DFT and fails when the content does
# not switch.
#
# The capture is written before the audio device (see QaPcmCapture), so this
# validates the mixed PCM, not physical audibility.
#
# Usage:
#   pwsh tools/ci/assert-pcm-spectral-cut.ps1 -PcmPath <playback-pcm.log>

param(
  [Parameter(Mandatory)][string]$PcmPath,
  [double]$CutSeconds = 4.0,
  [double]$LowHz = 440.0,
  [double]$HighHz = 880.0,
  [double]$MinimumAmplitude = 0.05,
  [double]$BeforeMarginSeconds = 0.05,
  [double]$WindowSeconds = 0.45
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $PcmPath -PathType Leaf)) {
  throw "PCM capture not found: $PcmPath"
}
$pcmText = Get-Content -LiteralPath $PcmPath -Raw
if ([string]::IsNullOrWhiteSpace($pcmText)) { throw "PCM capture is empty: $PcmPath" }

$format = [regex]::Match($pcmText, 'FORMAT sample_rate=(\d+) channels=(\d+)')
if (-not $format.Success) { throw "PCM capture has no FORMAT header: $PcmPath" }
$sampleRate = [int]$format.Groups[1].Value
$channels = [int]$format.Groups[2].Value
if ($sampleRate -le 0 -or $channels -le 0) {
  throw "PCM FORMAT header is invalid: sample_rate=$sampleRate channels=$channels"
}

# Hann-windowed single-bin DFT, expressed as peak amplitude. One full 1024-frame
# mix block holds ~21 ms, enough to separate 440 Hz from 880 Hz by a wide margin.
function Get-ToneLevel {
  param([double[]]$Values, [int]$Frames, [double]$Frequency)

  $twoPi = 2.0 * [Math]::PI
  $real = 0.0
  $imaginary = 0.0
  $windowSum = 0.0
  for ($frame = 0; $frame -lt $Frames; ++$frame) {
    $taper = 0.5 - 0.5 * [Math]::Cos($twoPi * $frame / ($Frames - 1))
    $angle = $twoPi * $Frequency * $frame / $sampleRate
    $sample = $Values[$frame * $channels] * $taper
    $real += $sample * [Math]::Cos($angle)
    $imaginary -= $sample * [Math]::Sin($angle)
    $windowSum += $taper
  }
  return 2.0 * [Math]::Sqrt($real * $real + $imaginary * $imaginary) / $windowSum
}

function Get-WindowLevels {
  param([double]$FromSeconds, [double]$ToSeconds)

  $rows = New-Object System.Collections.Generic.List[object]
  $blockPattern = '(?m)^BLOCK timeline=(-?\d+)/(\d+)/(\d+) frames=(\d+) samples=(\d+) values=([^\r\n]*)'
  foreach ($match in [regex]::Matches($pcmText, $blockPattern)) {
    $position = [double]$match.Groups[1].Value * [double]$match.Groups[2].Value /
                [double]$match.Groups[3].Value
    if ($position -lt $FromSeconds -or $position -ge $ToSeconds) { continue }
    $frames = [int]$match.Groups[4].Value
    if ($frames -lt 512) { continue }
    $values = [double[]]$match.Groups[6].Value.Split(',')
    if ($values.Count -ne $frames * $channels) { continue }

    # A block that is mostly silence (startup, device stall or the tail of a
    # fade) cannot carry a stable tone, so it is not evidence either way.
    $peak = 0.0
    for ($frame = 0; $frame -lt $frames; ++$frame) {
      $sample = [Math]::Abs($values[$frame * $channels])
      if ($sample -gt $peak) { $peak = $sample }
    }
    if ($peak -lt 0.02) { continue }

    $rows.Add([pscustomobject]@{
        Position = $position
        Low = Get-ToneLevel -Values $values -Frames $frames -Frequency $LowHz
        High = Get-ToneLevel -Values $values -Frames $frames -Frequency $HighHz
      })
  }
  return $rows
}

$beforeFrom = $CutSeconds - $WindowSeconds
$beforeTo = $CutSeconds - $BeforeMarginSeconds
$afterFrom = $CutSeconds + $BeforeMarginSeconds
$afterTo = $CutSeconds + $WindowSeconds

$before = @(Get-WindowLevels -FromSeconds $beforeFrom -ToSeconds $beforeTo)
$after = @(Get-WindowLevels -FromSeconds $afterFrom -ToSeconds $afterTo)

if ($before.Count -lt 2) {
  throw ("Only {0} measurable PCM block(s) before {1:F1}s; cannot verify the {2} Hz segment." -f
    $before.Count, $CutSeconds, $LowHz)
}
if ($after.Count -lt 2) {
  throw ("Only {0} measurable PCM block(s) after {1:F1}s; cannot verify the {2} Hz segment." -f
    $after.Count, $CutSeconds, $HighHz)
}

$beforeLow = ($before | Measure-Object -Property Low -Average).Average
$beforeHigh = ($before | Measure-Object -Property High -Average).Average
$afterLow = ($after | Measure-Object -Property Low -Average).Average
$afterHigh = ($after | Measure-Object -Property High -Average).Average

Write-Host ("Spectral cut: before {0} Hz={1:F4}, {2} Hz={3:F4} ({4} blocks); after {0} Hz={5:F4}, {2} Hz={6:F4} ({7} blocks)" -f
  $LowHz, $beforeLow, $HighHz, $beforeHigh, $before.Count,
  $afterLow, $afterHigh, $after.Count)

if ($beforeLow -le $MinimumAmplitude) {
  throw ("Audio before the cut is not the {0} Hz segment (amplitude {1:F4}, expected > {2:F4})." -f
    $LowHz, $beforeLow, $MinimumAmplitude)
}
if ($beforeHigh -ge $beforeLow / 4.0) {
  throw ("Audio before the cut contains unexpected {0} Hz content ({1:F4} vs {2:F4})." -f
    $HighHz, $beforeHigh, $beforeLow)
}
if ($afterHigh -le $MinimumAmplitude) {
  throw ("Audio after the cut is not the {0} Hz segment (amplitude {1:F4}, expected > {2:F4})." -f
    $HighHz, $afterHigh, $MinimumAmplitude)
}
if ($afterLow -ge $afterHigh / 4.0) {
  throw ("Audio after the cut still contains {0} Hz content ({1:F4} vs {2:F4}); the cut did not switch to the second source." -f
    $LowHz, $afterLow, $afterHigh)
}
if ($afterHigh -le $beforeHigh * 4.0) {
  throw ("{0} Hz content did not appear after the cut ({1:F4} -> {2:F4})." -f
    $HighHz, $beforeHigh, $afterHigh)
}

Write-Host "Spectral cut assertion passed: content switches from $LowHz Hz to $HighHz Hz at $CutSeconds s."
