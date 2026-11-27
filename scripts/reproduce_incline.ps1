$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
Push-Location $RepoRoot
try {
    .venv\Scripts\python.exe -m stem_engine doctor
    .venv\Scripts\python.exe -m stem_engine render examples/incline.py --output outputs/incline.mp4 --width 1920 --height 1080 --fps 60
} finally {
    Pop-Location
}
