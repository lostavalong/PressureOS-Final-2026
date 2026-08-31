param(
    [string]$QtRoot = "D:\Qt\6.8.3\msvc2022_64",
    [string]$BuildDir = "D:\PressureOS-build",
    [string]$AsciiSourceLink = "D:\PressureOS-src"
)

$ErrorActionPreference = "Stop"
$source = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

if (-not (Test-Path (Join-Path $QtRoot "bin\qmake.exe"))) {
    throw "Qt not found at $QtRoot. Pass -QtRoot with a Qt 6.4+ MSVC kit."
}

# Some Windows Qt tools still mishandle non-ASCII source paths. A directory
# junction gives the tools an ASCII path without moving or duplicating source.
$buildSource = $source
if ($source -match '[^\x00-\x7F]') {
    if (Test-Path -LiteralPath $AsciiSourceLink) {
        $item = Get-Item -LiteralPath $AsciiSourceLink
        $targets = @($item.Target)
        if ($item.LinkType -ne "Junction" -or $targets -notcontains $source) {
            throw "$AsciiSourceLink already exists and is not a junction to this source tree."
        }
    } else {
        New-Item -ItemType Junction -Path $AsciiSourceLink -Target $source | Out-Null
    }
    $buildSource = $AsciiSourceLink
}

$vsRoot = "D:\VisualStudio"
$vcvars = Join-Path $vsRoot "VC\Auxiliary\Build\vcvars64.bat"
$cmake = Join-Path $vsRoot "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$ninjaDir = Join-Path $vsRoot "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"

if (-not (Test-Path $cmake)) {
    $cmake = (Get-Command cmake -ErrorAction Stop).Source
}
if (-not (Test-Path $vcvars)) {
    throw "MSVC developer environment was not found. Install Visual Studio C++ desktop tools."
}

$env:PATH = "$ninjaDir;$QtRoot\bin;$env:PATH"
$configure = "`"$cmake`" -S `"$buildSource`" -B `"$BuildDir`" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=`"$QtRoot`""
$build = "`"$cmake`" --build `"$BuildDir`" --config Release --parallel"

cmd /c "`"$vcvars`" >nul && $configure && $build"
if ($LASTEXITCODE -ne 0) { throw "PressureOS build failed with exit code $LASTEXITCODE" }

Write-Host ""
Write-Host "Build complete: $BuildDir\pressureos.exe" -ForegroundColor Green
