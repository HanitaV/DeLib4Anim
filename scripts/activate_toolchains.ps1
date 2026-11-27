# Activate portable MSVC x64, Windows 11 SDK, CUDA 13.5, Compute Sanitizer, Nsight, and Skia toolchains
$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
$ToolchainsRoot = Join-Path $RepoRoot ".stem_cache\toolchains"

$MsvcEnvJson = Join-Path $ToolchainsRoot "msvc\msvc_env.json"
if (Test-Path $MsvcEnvJson) {
    $Msvc = Get-Content $MsvcEnvJson -Raw | ConvertFrom-Json
    $env:INCLUDE = ($Msvc.INCLUDE -join ";")
    $env:LIB = ($Msvc.LIB -join ";")
    $msvcBins = ($Msvc.PATH -join ";")
    $env:PATH = "$msvcBins;" + $env:PATH
    $env:VSCMD_VER = "17.14"
    $env:VCINSTALLDIR = "$($Msvc.msvc_root)\VC\"
    $env:WindowsSdkDir = "$($Msvc.msvc_root)\Windows Kits\10\"
    $env:UniversalCRTSdkDir = "$($Msvc.msvc_root)\Windows Kits\10\"
    $env:UCRTVersion = $Msvc.sdk_version
}

$CudaRoot = Join-Path $ToolchainsRoot "cuda"
if (Test-Path $CudaRoot) {
    $env:CUDA_PATH = $CudaRoot
    $cudaDirs = @(
        (Join-Path $CudaRoot "bin"),
        (Join-Path $CudaRoot "nvvm\bin"),
        (Join-Path $CudaRoot "compute-sanitizer")
    )
    $nsysExe = Get-ChildItem -Path (Join-Path $CudaRoot "nsight-systems") -Filter "nsys.exe" -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match "target-windows-x64" } | Select-Object -First 1
    if ($nsysExe) { $cudaDirs += $nsysExe.DirectoryName }
    $ncuExe = Get-ChildItem -Path (Join-Path $CudaRoot "nsight-compute") -Filter "ncu.exe" -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match "target\\windows-desktop-win7-x64" } | Select-Object -First 1
    if ($ncuExe) { $cudaDirs += $ncuExe.DirectoryName }

    $validCudaDirs = $cudaDirs | Where-Object { Test-Path $_ }
    if ($validCudaDirs) {
        $env:PATH = ($validCudaDirs -join ";") + ";" + $env:PATH
    }
}

$SkiaRoot = Join-Path $ToolchainsRoot "skia"
if (Test-Path $SkiaRoot) {
    $env:SKIA_ROOT = $SkiaRoot
}
