#!/usr/bin/env pwsh
# Regression test for QA fixture paths that contain spaces, brackets and '#'.
#
# PowerShell resolves unescaped '[' and ']' in a path as wildcard character
# classes, so `Test-Path $path` reports $false for a file that exists. The
# Windows QA smoke test once treated a successfully generated MP4 as missing
# for exactly this reason. This script pins the behaviour the CI steps rely on:
# every existence, size and enumeration check must use -LiteralPath.

$ErrorActionPreference = 'Stop'

$root = Join-Path ([IO.Path]::GetTempPath()) ("pulsar-literal-path-{0}" -f [Guid]::NewGuid().ToString('N'))
$checks = 0

function Assert-True {
  param(
    [Parameter(Mandatory)][bool]$Condition,
    [Parameter(Mandatory)][string]$Message
  )
  if (-not $Condition) { throw $Message }
  $script:checks++
}

try {
  New-Item -ItemType Directory -Force -Path $root | Out-Null

  $names = @(
    'synthetic audio [CI] #1.mp4'
    'persisted cut [CI] #1.pulsar'
    'plain file with spaces.txt'
    'hash # only.txt'
  )

  foreach ($name in $names) {
    $path = Join-Path $root $name
    $content = "fixture:$name"
    [IO.File]::WriteAllText($path, $content, (New-Object Text.UTF8Encoding($false)))

    # The file really exists on disk before any assertion runs.
    Assert-True (Test-Path -LiteralPath $path -PathType Leaf) "Literal path check failed for '$name'."
    Assert-True ((Get-Item -LiteralPath $path).Length -eq $content.Length) "Literal size lookup failed for '$name'."
    Assert-True ((Get-Content -LiteralPath $path -Raw) -eq $content) "Literal read failed for '$name'."

    # Paths with brackets must demonstrate the wildcard hazard; plain names
    # are valid wildcard patterns and therefore unaffected.
    if ($name -match '[\[\]]') {
      Assert-True (-not (Test-Path $path)) "Expected wildcard semantics to reject '$name'; the regression would not be meaningful otherwise."
    }

    # Enumeration must find the file by its exact name.
    $enumerated = @(Get-ChildItem -LiteralPath $root -File | Where-Object { $_.Name -eq $name })
    Assert-True ($enumerated.Count -eq 1) "Literal enumeration found $($enumerated.Count) entries named '$name'."

    Write-Host ("ok  {0} ({1} bytes, literal path)" -f $name, $content.Length)
  }

  $expected = $names.Count
  $actual = @(Get-ChildItem -LiteralPath $root -File).Count
  Assert-True ($actual -eq $expected) "Expected $expected fixture files, found $actual."

  Write-Host "Literal path handling verified with $checks assertions across $actual files."
} finally {
  Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
}
