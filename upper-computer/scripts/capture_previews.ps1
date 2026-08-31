param(
    [string]$QtRoot = "D:\Qt\6.8.3\msvc2022_64",
    [string]$BuildDir = "D:\PressureOS-build",
    [string]$OutputDir = (Join-Path $PSScriptRoot "..\docs\screenshots")
)

$ErrorActionPreference = "Stop"
$exe = Join-Path $BuildDir "pressureos.exe"
if (-not (Test-Path -LiteralPath $exe)) {
    throw "Build first: $exe was not found."
}

$qtBin = Join-Path $QtRoot "bin"
if (-not (Test-Path -LiteralPath $qtBin)) {
    throw "Qt runtime was not found at $qtBin."
}

$env:PATH = "$qtBin;$env:PATH"
# Keep visual regression images on the same 1024x600 logical canvas even when
# the Windows workstation uses 125%/150% desktop scaling. Qt multiplies its
# own scale factor by AppliedDPI, so the inverse normalises screenshot output.
$appliedDpi = (Get-ItemProperty 'HKCU:\Control Panel\Desktop\WindowMetrics' -ErrorAction SilentlyContinue).AppliedDPI
if ($appliedDpi -and [double]$appliedDpi -gt 0) {
    $env:QT_SCALE_FACTOR = (96.0 / [double]$appliedDpi).ToString([System.Globalization.CultureInfo]::InvariantCulture)
}
$outputFullPath = [System.IO.Path]::GetFullPath($OutputDir)
New-Item -ItemType Directory -Force -Path $outputFullPath | Out-Null

# Remove only the filenames used by the previous eight-screen preview set so
# old and new layouts cannot be mistaken for one another.
$legacyPreviewNames = @(
    "04_task_runner.png",
    "05_templates.png",
    "06_data_studio.png",
    "07_device_center.png",
    "08_assistant.png"
)
foreach ($legacyName in $legacyPreviewNames) {
    Remove-Item -LiteralPath (Join-Path $outputFullPath $legacyName) -Force -ErrorAction SilentlyContinue
}

# Every preview run gets its own database/export directories. This keeps the
# user's real tasks untouched and makes screenshots deterministic.
$runtimeRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("PressureOS-preview-" + [guid]::NewGuid().ToString("N"))
$dataRoot = Join-Path $runtimeRoot "data"
$exportRoot = Join-Path $runtimeRoot "export"
New-Item -ItemType Directory -Force -Path $dataRoot, $exportRoot | Out-Null
$env:PRESSUREOS_DATA_ROOT = $dataRoot
$env:PRESSUREOS_EXPORT_ROOT = $exportRoot

function Save-PressureOSPreview {
    param(
        [Parameter(Mandatory = $true)][string]$Route,
        [Parameter(Mandatory = $true)][string]$Name,
        [Nullable[int]]$TaskStage,
        [switch]$Assistant,
        [string]$AssistantArticle = "",
        [switch]$CreateTask,
        [string]$KeyboardMode = ""
    )

    $screenshotFile = Join-Path $outputFullPath "$Name.png"
    $stdoutLog = Join-Path $runtimeRoot "$Name.stdout.log"
    $stderrLog = Join-Path $runtimeRoot "$Name.stderr.log"
    $arguments = @("--windowed", "--page", $Route, "--screenshot", "`"$screenshotFile`"")
    if ($PSBoundParameters.ContainsKey("TaskStage")) {
        $arguments += @("--task-stage", ([int]$TaskStage).ToString())
    }
    if ($Assistant) {
        $arguments += "--assistant"
    }
    if (-not [string]::IsNullOrWhiteSpace($AssistantArticle)) {
        $arguments += @("--assistant-article", $AssistantArticle)
    }
    if ($CreateTask) {
        $arguments += "--create-task-dialog"
    }
    if (-not [string]::IsNullOrWhiteSpace($KeyboardMode)) {
        $arguments += @("--keyboard-preview", $KeyboardMode)
    }

    $process = $null
    try {
        $process = Start-Process -FilePath $exe -ArgumentList $arguments -PassThru -WindowStyle Hidden `
            -RedirectStandardOutput $stdoutLog -RedirectStandardError $stderrLog
        if (-not $process.WaitForExit(30000)) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
            throw "PressureOS timed out while rendering $Route."
        }
        $process.Refresh()
        # On Windows PowerShell 5.1 a short-lived WIN32_EXECUTABLE can return a
        # Process object without a readable ExitCode. The screenshot and log
        # checks below remain authoritative in that case.
        if ($null -ne $process.ExitCode -and $process.ExitCode -ne 0) {
            $details = Get-Content -LiteralPath $stderrLog -Raw -ErrorAction SilentlyContinue
            throw "PressureOS exited with code $($process.ExitCode) while rendering $Route.`n$details"
        }

        $runtimeWarnings = Get-Content -LiteralPath $stderrLog -Raw -ErrorAction SilentlyContinue
        if (-not [string]::IsNullOrWhiteSpace($runtimeWarnings)) {
            throw "Runtime warning on $Route`n$runtimeWarnings"
        }
        if (-not (Test-Path -LiteralPath $screenshotFile)) {
            throw "PressureOS did not create $screenshotFile."
        }
        Write-Host "Captured $Name" -ForegroundColor Green
    }
    finally {
        if ($null -ne $process -and -not $process.HasExited) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        }
    }
}

try {
    Save-PressureOSPreview -Route home -Name "01_home"
    Save-PressureOSPreview -Route measure -Name "02_measurement"
    Save-PressureOSPreview -Route tasks -Name "03_tasks"
    Save-PressureOSPreview -Route runner -Name "04_task_overview" -TaskStage 0
    Save-PressureOSPreview -Route runner -Name "05_task_record" -TaskStage 1
    Save-PressureOSPreview -Route runner -Name "06_task_processing" -TaskStage 2
    Save-PressureOSPreview -Route runner -Name "07_task_analysis" -TaskStage 3
    Save-PressureOSPreview -Route runner -Name "08_task_export" -TaskStage 4
    Save-PressureOSPreview -Route templates -Name "09_templates"
    Save-PressureOSPreview -Route data -Name "10_data_studio"
    Save-PressureOSPreview -Route device -Name "11_device_center"
    Save-PressureOSPreview -Route home -Name "12_assistant" -Assistant
    Save-PressureOSPreview -Route runner -Name "12b_assistant_answer" -TaskStage 3 -AssistantArticle "residual_outlier"
    Save-PressureOSPreview -Route tasks -Name "13_create_task" -CreateTask
    Save-PressureOSPreview -Route tasks -Name "14_keyboard_text" -CreateTask -KeyboardMode text
    Save-PressureOSPreview -Route tasks -Name "15_keyboard_numeric" -CreateTask -KeyboardMode numeric
    Save-PressureOSPreview -Route tasks -Name "16_keyboard_symbols" -CreateTask -KeyboardMode symbols
}
finally {
    # Recursive cleanup is restricted to the unique directory created under
    # the system temp root above.
    $tempRoot = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
    $runtimeFullPath = [System.IO.Path]::GetFullPath($runtimeRoot)
    if ($runtimeFullPath.StartsWith($tempRoot, [System.StringComparison]::OrdinalIgnoreCase) -and
        (Split-Path -Leaf $runtimeFullPath).StartsWith("PressureOS-preview-")) {
        Remove-Item -LiteralPath $runtimeFullPath -Recurse -Force -ErrorAction SilentlyContinue
    } else {
        Write-Warning "Skipped cleanup because the runtime path was outside the expected temp root: $runtimeFullPath"
    }
}

Write-Host "Preview set complete: $outputFullPath" -ForegroundColor Cyan
