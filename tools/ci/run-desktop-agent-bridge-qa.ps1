#!/usr/bin/env pwsh
# End-to-end QA for the local Agent Bridge controlling the real Qt desktop.

param(
  [Parameter(Mandatory)][string]$Exe,
  [string]$Client,
  [string]$Workspace = (Join-Path $env:TEMP 'pulsar-desktop-agent-qa'),
  [switch]$Offscreen
)

$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $Exe -PathType Leaf)) { throw "Desktop executable not found: $Exe" }
if (-not $Client) { $Client = Join-Path (Split-Path -Parent $Exe) 'pulsar-agent-client.exe' }
if (-not (Test-Path -LiteralPath $Client -PathType Leaf)) {
  $linuxClient = Join-Path (Split-Path -Parent $Exe) 'pulsar-agent-client'
  if (Test-Path -LiteralPath $linuxClient -PathType Leaf) { $Client = $linuxClient }
  else { throw "Agent client not found: $Client" }
}
$ffmpeg = if ($env:PULSAR_TEST_FFMPEG_CLI) {
  $env:PULSAR_TEST_FFMPEG_CLI
} else {
  (Get-Command ffmpeg -ErrorAction Stop).Source
}

Remove-Item -Recurse -Force $Workspace -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $Workspace | Out-Null
$media = Join-Path $Workspace 'agent fixture [QA] #1.mp4'
& $ffmpeg -y -hide_banner -loglevel error `
  -f lavfi -i 'testsrc=size=320x180:rate=30' `
  -f lavfi -i 'sine=frequency=440:sample_rate=48000:duration=4' `
  -f lavfi -i 'sine=frequency=880:sample_rate=48000:duration=4' `
  -filter_complex '[1:a][2:a]concat=n=2:v=0:a=1[a]' `
  -map 0:v -map '[a]' -t 8 `
  -c:v mpeg4 -q:v 5 -pix_fmt yuv420p -c:a aac -b:a 128k $media
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $media -PathType Leaf)) {
  throw 'Agent QA fixture generation failed.'
}

$mediaLocator = $media.Replace('\', '/')
$project = Join-Path $Workspace 'agent-input.pulsar'
$saved = Join-Path $Workspace 'agent-saved.pulsar'
$projectLines = @(
  'PULSAR_PROJECT 1'
  'TIMELINE 4'
  'TRACK 8 VIDEO'
  'VIDEO_TRACK 8'
  "ASSET 2 0 8 1 1 `"$mediaLocator`" `"mpeg4`" 320 180 1 15360 30 1 `"mov,mp4,m4a,3gp,3g2,mj2`" AUDIO 1 48000 1 1 48000 8 1 1"
  'CLIP 20 8 2 0 1 1 8 1 1 0 1 1'
  'ALLOCATOR 10 30 5 9'
  'END'
)
[IO.File]::WriteAllText($project, ($projectLines -join "`n") + "`n", (New-Object Text.UTF8Encoding($false)))

$descriptor = Join-Path $Workspace 'agent-bridge.json'
$stdout = Join-Path $Workspace 'desktop-stdout.log'
$stderr = Join-Path $Workspace 'desktop-stderr.log'
$pcm = Join-Path $Workspace 'agent-playback-pcm.log'
$shot = Join-Path $Workspace 'agent-editor.png'
$responses = Join-Path $Workspace 'agent-responses.jsonl'

$oldPlatform = $env:QT_QPA_PLATFORM
$oldWorkspace = $env:PULSAR_QA_WORKSPACE
$oldPcm = $env:PULSAR_QA_PCM_PATH
$env:PULSAR_QA_WORKSPACE = $Workspace
$env:PULSAR_QA_PCM_PATH = $pcm
if ($Offscreen) { $env:QT_QPA_PLATFORM = 'offscreen' }
else { Remove-Item Env:\QT_QPA_PLATFORM -ErrorAction SilentlyContinue }

function Invoke-Agent([hashtable]$Request) {
  $line = $Request | ConvertTo-Json -Compress
  $output = $line | & $Client --workspace $Workspace --timeout-ms 5000
  if ($LASTEXITCODE -ne 0) { throw "Agent client failed for $($Request.command)." }
  $output | Add-Content -LiteralPath $responses -Encoding UTF8
  $response = $output | ConvertFrom-Json
  if (-not $response.ok) {
    throw "Agent command $($Request.command) failed: $($response.error.code): $($response.error.message)"
  }
  Start-Sleep -Milliseconds 100
  return $response
}

try {
  $arguments = '--agent-bridge --workspace "{0}" --smoke-size 1280x720 --smoke-exit-ms 30000' -f $Workspace
  $process = Start-Process -FilePath $Exe -ArgumentList $arguments `
    -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
  $deadline = [DateTime]::UtcNow.AddSeconds(10)
  while (-not (Test-Path -LiteralPath $descriptor -PathType Leaf)) {
    if ($process.HasExited) { throw "Desktop exited before creating the bridge descriptor: $($process.ExitCode)" }
    if ([DateTime]::UtcNow -ge $deadline) { throw 'Timed out waiting for the bridge descriptor.' }
    Start-Sleep -Milliseconds 100
  }

  $status = Invoke-Agent @{ id = 'status-1'; command = 'status' }
  $open = Invoke-Agent @{ id = 'open-1'; command = 'open_project'; path = 'agent-input.pulsar' }
  $before = Invoke-Agent @{ id = 'timeline-before'; command = 'timeline_state' }
  $beforeJson = $before.tracks | ConvertTo-Json -Depth 10 -Compress
  if ($before.tracks[0].clips.Count -ne 1 -or $before.tracks[0].clips[0].id -ne 20) {
    throw "Unexpected timeline before split: $beforeJson"
  }

  $split = Invoke-Agent @{ id = 'split-1'; command = 'split_clip'; clip_id = 20; position_seconds = 4.0 }
  $after = Invoke-Agent @{ id = 'timeline-after'; command = 'timeline_state' }
  $afterJson = $after.tracks | ConvertTo-Json -Depth 10 -Compress
  if ($after.tracks[0].clips.Count -ne 2) { throw "Split did not create two clips: $afterJson" }
  $clips = @($after.tracks[0].clips | Sort-Object start_seconds)
  if ([Math]::Abs($clips[0].end_seconds - 4.0) -gt 0.000001 -or
      [Math]::Abs($clips[1].start_seconds - 4.0) -gt 0.000001) {
    throw "Split boundary is not 4 seconds: $afterJson"
  }

  $save = Invoke-Agent @{ id = 'save-1'; command = 'save_project'; path = 'agent-saved.pulsar' }
  if (-not (Test-Path -LiteralPath $saved -PathType Leaf)) { throw 'Saved project was not created.' }
  $reopen = Invoke-Agent @{ id = 'open-2'; command = 'open_project'; path = 'agent-saved.pulsar' }
  $reopened = Invoke-Agent @{ id = 'timeline-reopened'; command = 'timeline_state' }
  $reopenedJson = $reopened.tracks | ConvertTo-Json -Depth 10 -Compress
  if ($reopenedJson -ne $afterJson) {
    throw "Timeline changed after save/reopen.`nBefore: $afterJson`nAfter: $reopenedJson"
  }

  $seek = Invoke-Agent @{ id = 'seek-1'; command = 'seek'; position_seconds = 0.0 }
  $play = Invoke-Agent @{ id = 'play-1'; command = 'play' }
  Start-Sleep -Seconds 2
  $metrics = Invoke-Agent @{ id = 'metrics-1'; command = 'get_metrics' }
  $pause = Invoke-Agent @{ id = 'pause-1'; command = 'pause' }
  $capture = Invoke-Agent @{ id = 'capture-1'; command = 'capture_screenshot'; path = 'agent-editor.png' }
  if (-not (Test-Path -LiteralPath $shot -PathType Leaf) -or (Get-Item -LiteralPath $shot).Length -lt 1000) {
    throw 'Agent screenshot is missing or empty.'
  }
  if ($metrics.performance -notmatch 'audio.source_opens=[1-9][0-9]*') {
    throw "Real playback did not open an audio source: $($metrics.performance)"
  }

  # Security checks use the real endpoint and must not mutate anything outside
  # the workspace.
  $escape = @{ id = 'escape-1'; command = 'save_project'; path = '../agent-escape.pulsar' } |
    ConvertTo-Json -Compress | & $Client --workspace $Workspace --timeout-ms 5000 | ConvertFrom-Json
  if ($escape.ok -or $escape.error.code -ne 'invalid_request') { throw 'Workspace escape was not rejected.' }
  if (Test-Path -LiteralPath (Join-Path (Split-Path -Parent $Workspace) 'agent-escape.pulsar')) {
    throw 'Workspace escape created a file.'
  }
  $badTimeout = @{ id = 'timeout-1'; command = 'status'; timeout_ms = 6000 } |
    ConvertTo-Json -Compress | & $Client --workspace $Workspace --timeout-ms 5000 | ConvertFrom-Json
  if ($badTimeout.ok -or $badTimeout.error.code -ne 'invalid_timeout') {
    throw 'Invalid request timeout was not rejected.'
  }
  $unknown = @{ id = 'unknown-1'; command = 'run_shell' } |
    ConvertTo-Json -Compress | & $Client --workspace $Workspace --timeout-ms 5000 | ConvertFrom-Json
  if ($unknown.ok -or $unknown.error.code -ne 'unknown_command') {
    throw 'Unknown/arbitrary command was not rejected.'
  }

  Write-Host "Agent desktop QA passed: clip 20 split at 4s, persisted and reopened."
  Write-Host "Playback metrics collected; screenshot bytes: $((Get-Item -LiteralPath $shot).Length)."
  Write-Host "PCM capture is pre-device evidence only; physical audibility still requires loopback."
  Write-Host "Evidence directory: $Workspace"
} finally {
  if ($process -and -not $process.HasExited) { Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue }
  if ($null -ne $oldPlatform) { $env:QT_QPA_PLATFORM = $oldPlatform }
  else { Remove-Item Env:\QT_QPA_PLATFORM -ErrorAction SilentlyContinue }
  if ($null -ne $oldWorkspace) { $env:PULSAR_QA_WORKSPACE = $oldWorkspace }
  else { Remove-Item Env:\PULSAR_QA_WORKSPACE -ErrorAction SilentlyContinue }
  if ($null -ne $oldPcm) { $env:PULSAR_QA_PCM_PATH = $oldPcm }
  else { Remove-Item Env:\PULSAR_QA_PCM_PATH -ErrorAction SilentlyContinue }
}

$errorText = Get-Content -LiteralPath $stderr -Raw
if ($errorText -match "Property 'requestPaint' of object") {
  throw "Inspector requestPaint regression returned: $errorText"
}
