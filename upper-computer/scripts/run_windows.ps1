param(
    [ValidateSet("home", "measure", "tasks", "runner", "templates", "data", "device")]
    [string]$Page = "home",
    [string]$QtRoot = "D:\Qt\6.8.3\msvc2022_64",
    [string]$BuildDir = "D:\PressureOS-build",
    [switch]$Fullscreen
)

$ErrorActionPreference = "Stop"
$exe = Join-Path $BuildDir "pressureos.exe"
if (-not (Test-Path $exe)) {
    & (Join-Path $PSScriptRoot "build_windows.ps1") -QtRoot $QtRoot -BuildDir $BuildDir
}

$env:PATH = "$QtRoot\bin;$env:PATH"
$mode = if ($Fullscreen) { "--fullscreen" } else { "--windowed" }
& $exe $mode --page $Page
