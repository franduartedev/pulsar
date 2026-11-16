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
$video = Join-Path $Workspace 'agent video [QA] #1.mp4'
$audio = Join-Path $Workspace 'agent audio [QA] #2.wav'
& $ffmpeg -y -hide_banner -loglevel error `
  -f lavfi -i 'testsrc=size=320x180:rate=24:duration=6' `
  -an -c:v mpeg4 -q:v 6 -pix_fmt yuv420p $video
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $video -PathType Leaf)) {
  throw 'Agent video fixture generation failed.'
}
& $ffmpeg -y -hide_banner -loglevel error `
  -f lavfi -i 'sine=frequency=440:sample_rate=48000:duration=3' `
  -f lavfi -i 'sine=frequency=880:sample_rate=48000:duration=3' `
  -filter_complex '[0:a][1:a]concat=n=2:v=0:a=1[a]' `
  -map '[a]' -c:a pcm_s16le $audio
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $audio -PathType Leaf)) {
  throw 'Agent audio fixture generation failed.'
}

$project = Join-Path $Workspace 'agent-input.pulsar'
$saved = Join-Path $Workspace 'agent-saved.pulsar'
$projectLines = @(
  'PULSAR_PROJECT 2'
  'TIMELINE 4'
  'TRACK 8 VIDEO'
  'TRACK 9 AUDIO'
  'VIDEO_TRACK 8'
  'ALLOCATOR 10 20 5 10'
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

  $importVideo = Invoke-Agent @{ id = 'import-video'; command = 'import_media'; path = 'agent video [QA] #1.mp4' }
  $importAudio = Invoke-Agent @{ id = 'import-audio'; command = 'import_media'; path = 'agent audio [QA] #2.wav' }
  $assetDeadline = [DateTime]::UtcNow.AddSeconds(15)
  do {
    $assets = Invoke-Agent @{ id = 'assets-poll'; command = 'media_assets' }
    if (-not $assets.busy -and $assets.import_pending -eq 0) { break }
    if ([DateTime]::UtcNow -ge $assetDeadline) { throw 'Timed out waiting for real media imports.' }
    Start-Sleep -Milliseconds 100
  } while ($true)
  if ($assets.assets.Count -ne 2) {
    throw "Expected two imported assets: $($assets.assets | ConvertTo-Json -Depth 6 -Compress)"
  }
  $videoAsset = $assets.assets | Where-Object name -eq 'agent video [QA] #1.mp4'
  $audioAsset = $assets.assets | Where-Object name -eq 'agent audio [QA] #2.wav'
  if (-not $videoAsset -or $videoAsset.kind -ne 'video' -or -not $audioAsset -or $audioAsset.kind -ne 'audio') {
    throw "Imported asset identities or kinds are wrong: $($assets.assets | ConvertTo-Json -Depth 6 -Compress)"
  }

  $videoClip = Invoke-Agent @{ id = 'add-video'; command = 'add_clip'; media_id = $videoAsset.id; track_id = 8; timeline_start_seconds = 0.0 }
  $audioClip = Invoke-Agent @{ id = 'add-audio'; command = 'add_clip'; media_id = $audioAsset.id; track_id = 9; timeline_start_seconds = 0.0 }
  $before = Invoke-Agent @{ id = 'timeline-before'; command = 'timeline_state' }
  $beforeJson = $before.tracks | ConvertTo-Json -Depth 10 -Compress
  if ($before.tracks.Count -ne 2 -or $before.tracks[0].clips.Count -ne 1 -or $before.tracks[1].clips.Count -ne 1) {
    throw "The two real clips were not placed on their requested tracks: $beforeJson"
  }

  $split = Invoke-Agent @{ id = 'split-1'; command = 'split_clip'; clip_id = $videoClip.clip_id; position_seconds = 3.0 }
  $after = Invoke-Agent @{ id = 'timeline-after'; command = 'timeline_state' }
  $afterJson = $after.tracks | ConvertTo-Json -Depth 10 -Compress
  if ($after.tracks[0].clips.Count -ne 2 -or $after.tracks[1].clips.Count -ne 1) {
    throw "Split did not preserve the independent audio clip: $afterJson"
  }
  $clips = @($after.tracks[0].clips | Sort-Object start_seconds)
  if ([Math]::Abs($clips[0].end_seconds - 3.0) -gt 0.000001 -or
      [Math]::Abs($clips[1].start_seconds - 3.0) -gt 0.000001) {
    throw "Split boundary is not 3 seconds: $afterJson"
  }

  $remove = Invoke-Agent @{ id = 'remove-1'; command = 'remove_clip'; clip_id = $split.created_clip_id }
  $replacement = Invoke-Agent @{ id = 'add-replacement'; command = 'add_clip'; media_id = $videoAsset.id; track_id = 8; timeline_start_seconds = 3.0; source_start_seconds = 3.0; source_end_seconds = $videoAsset.duration_seconds }
  $final = Invoke-Agent @{ id = 'timeline-final'; command = 'timeline_state' }
  $finalJson = $final.tracks | ConvertTo-Json -Depth 10 -Compress
  if ($final.tracks[0].clips.Count -ne 2 -or $final.tracks[1].clips.Count -ne 1) {
    throw "Remove/re-add did not produce the expected montage: $finalJson"
  }

  $save = Invoke-Agent @{ id = 'save-1'; command = 'save_project'; path = 'agent-saved.pulsar' }
  if (-not (Test-Path -LiteralPath $saved -PathType Leaf)) { throw 'Saved project was not created.' }
  $reopen = Invoke-Agent @{ id = 'open-2'; command = 'open_project'; path = 'agent-saved.pulsar' }
  $reopened = Invoke-Agent @{ id = 'timeline-reopened'; command = 'timeline_state' }
  $reopenedJson = $reopened.tracks | ConvertTo-Json -Depth 10 -Compress
  if ($reopenedJson -ne $finalJson) {
    throw "Timeline changed after save/reopen.`nBefore: $finalJson`nAfter: $reopenedJson"
  }
  $reopenedAssets = Invoke-Agent @{ id = 'assets-reopened'; command = 'media_assets' }
  if ($reopenedAssets.assets.Count -ne 2) { throw 'Imported media did not persist after reopen.' }

  $seek = Invoke-Agent @{ id = 'seek-1'; command = 'seek'; position_seconds = 0.0 }
  $play = Invoke-Agent @{ id = 'play-1'; command = 'play' }
  Start-Sleep -Seconds 2
  $metrics = Invoke-Agent @{ id = 'metrics-1'; command = 'get_metrics' }
  $pause = Invoke-Agent @{ id = 'pause-1'; command = 'pause' }
  $capture = Invoke-Agent @{ id = 'capture-1'; command = 'capture_screenshot'; path = 'agent-editor.png' }
  if (-not (Test-Path -LiteralPath $shot -PathType Leaf) -or (Get-Item -LiteralPath $shot).Length -lt 1000) {
    throw 'Agent screenshot is missing or empty.'
  }
  if ($metrics.diagnostics -notmatch 'audio_source_opens=[1-9][0-9]*') {
    throw "Real playback did not open an audio source: $($metrics.diagnostics)"
  }

  # Security checks use the real endpoint and must not mutate anything outside
  # the workspace.
  $escape = @{ id = 'escape-1'; command = 'save_project'; path = '../agent-escape.pulsar' } |
    ConvertTo-Json -Compress | & $Client --workspace $Workspace --timeout-ms 5000 | ConvertFrom-Json
  if ($escape.ok -or $escape.error.code -ne 'invalid_request') { throw 'Workspace escape was not rejected.' }
  if (Test-Path -LiteralPath (Join-Path (Split-Path -Parent $Workspace) 'agent-escape.pulsar')) {
    throw 'Workspace escape created a file.'
  }
  $mediaEscape = @{ id = 'media-escape'; command = 'import_media'; path = '../outside.mp4' } |
    ConvertTo-Json -Compress | & $Client --workspace $Workspace --timeout-ms 5000 | ConvertFrom-Json
  if ($mediaEscape.ok -or $mediaEscape.error.code -ne 'invalid_request') {
    throw 'Media workspace escape was not rejected.'
  }
  $absoluteMedia = @{ id = 'media-absolute'; command = 'import_media'; path = $video } |
    ConvertTo-Json -Compress | & $Client --workspace $Workspace --timeout-ms 5000 | ConvertFrom-Json
  if ($absoluteMedia.ok -or $absoluteMedia.error.code -ne 'invalid_request') {
    throw 'Absolute media path was not rejected.'
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

  Write-Host 'Agent desktop media QA passed: video/audio imported, placed, split, removed, re-added, persisted and reopened.'
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
