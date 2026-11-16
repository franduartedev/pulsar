#!/usr/bin/env pwsh
# Capture both deterministic overlay states in the real Qt desktop. This uses
# only existing Agent Bridge commands; Mission 023 adds no automation API.

param(
  [Parameter(Mandatory)][string]$Exe,
  [Parameter(Mandatory)][string]$Client,
  [Parameter(Mandatory)][string]$Workspace,
  [switch]$Offscreen
)

$ErrorActionPreference = 'Stop'
foreach ($path in @($Exe, $Client, (Join-Path $Workspace 'overlay-position-a.pulsar'),
                    (Join-Path $Workspace 'overlay.pulsar'))) {
  if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Overlay evidence input is missing: $path" }
}
$descriptor = Join-Path $Workspace 'agent-bridge.json'
$stdout = Join-Path $Workspace 'desktop-overlay-stdout.log'
$stderr = Join-Path $Workspace 'desktop-overlay-stderr.log'
$responses = Join-Path $Workspace 'overlay-responses.jsonl'
$oldPlatform = $env:QT_QPA_PLATFORM
if ($Offscreen) { $env:QT_QPA_PLATFORM = 'offscreen' }

function Invoke-Agent([hashtable]$Request) {
  $line = $Request | ConvertTo-Json -Compress
  $output = $line | & $Client --workspace $Workspace --timeout-ms 5000
  if ($LASTEXITCODE -ne 0) { throw "Overlay agent client failed for $($Request.command)." }
  $output | Add-Content -LiteralPath $responses -Encoding UTF8
  $response = $output | ConvertFrom-Json
  if (-not $response.ok) {
    throw "Overlay desktop command failed: $($Request.command) $($response.error.code): $($response.error.message)"
  }
  Start-Sleep -Milliseconds 250
  return $response
}

try {
  Remove-Item -LiteralPath $descriptor -Force -ErrorAction SilentlyContinue
  $arguments = '--agent-bridge --workspace "{0}" --smoke-size 1280x720 --smoke-exit-ms 20000' -f $Workspace
  $process = Start-Process -FilePath $Exe -ArgumentList $arguments `
    -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
  $deadline = [DateTime]::UtcNow.AddSeconds(10)
  while (-not (Test-Path -LiteralPath $descriptor -PathType Leaf)) {
    if ($process.HasExited) { throw "Desktop exited before overlay capture: $($process.ExitCode)" }
    if ([DateTime]::UtcNow -ge $deadline) { throw 'Timed out waiting for overlay desktop.' }
    Start-Sleep -Milliseconds 100
  }
  Invoke-Agent @{ id = 'status'; command = 'status' } | Out-Null
  Invoke-Agent @{ id = 'open-a'; command = 'open_project'; path = 'overlay-position-a.pulsar' } | Out-Null
  Invoke-Agent @{ id = 'seek-a'; command = 'seek'; position_seconds = 1.5 } | Out-Null
  Invoke-Agent @{ id = 'capture-a'; command = 'capture_screenshot'; path = 'overlay-gui-position-a.png' } | Out-Null
  Invoke-Agent @{ id = 'open-b'; command = 'open_project'; path = 'overlay.pulsar' } | Out-Null
  Invoke-Agent @{ id = 'seek-b'; command = 'seek'; position_seconds = 1.5 } | Out-Null
  Invoke-Agent @{ id = 'capture-b'; command = 'capture_screenshot'; path = 'overlay-gui-position-b.png' } | Out-Null
  foreach ($shot in @('overlay-gui-position-a.png', 'overlay-gui-position-b.png')) {
    $item = Get-Item -LiteralPath (Join-Path $Workspace $shot)
    if ($item.Length -lt 1000) { throw "Overlay GUI capture is empty: $shot" }
  }
} finally {
  if ($process -and -not $process.HasExited) { Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue }
  if ($null -ne $oldPlatform) { $env:QT_QPA_PLATFORM = $oldPlatform }
  else { Remove-Item Env:\QT_QPA_PLATFORM -ErrorAction SilentlyContinue }
}

$errorText = Get-Content -LiteralPath $stderr -Raw
if ($errorText -match "Property 'requestPaint' of object|open project failed|cannot decode project") {
  throw "Overlay desktop reported an error: $errorText"
}
Write-Host "Overlay GUI evidence captured in $Workspace"
