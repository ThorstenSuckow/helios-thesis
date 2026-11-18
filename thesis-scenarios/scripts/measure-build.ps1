param(
    [string[]]$Variants = @('v1', 'v2', 'v3', 'v5'),

    [int]$Runs = 5,

    # Build mit Rendering (ohne -DHEADLESS=1); Ablage in build-measure-render-* bzw. build-measure-logs-render
    [switch]$Render,

    # Release: Ablage in build-measure[-render]-release-* bzw. build-measure-logs[-render]-release
    [ValidateSet('RelWithDebInfo', 'Release')]
    [string]$BuildType = 'RelWithDebInfo',

    [string]$CMake = "C:\Program Files\JetBrains\CLion 2025.2\bin\cmake\win\x64\bin\cmake.exe",

    [string]$Vcvars = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
)

$ErrorActionPreference = 'Stop'

# Skript liegt in <thesis-scenarios>\scripts; gearbeitet wird im Projektverzeichnis
Set-Location (Split-Path $PSScriptRoot -Parent)

if (-not (Test-Path $CMake)) { throw "cmake nicht gefunden: $CMake" }

# MSVC-Umgebung (PATH, INCLUDE, LIB) laden, falls nicht in einer Developer-Shell gestartet
if (-not $env:VSCMD_VER) {
    if (-not (Test-Path $Vcvars)) { throw "vcvars64.bat nicht gefunden: $Vcvars" }
    cmd /c "`"$Vcvars`" >nul && set" | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($Matches[1])" -Value $Matches[2] }
    }
}

function Invoke-Native {
    param([scriptblock]$Command)
    & $Command
    if ($LASTEXITCODE -ne 0) { throw "Befehl fehlgeschlagen (Exit $LASTEXITCODE): $Command" }
}

$venvPython = "$PWD/.venv-build/Scripts/python.exe"
if (-not (Test-Path $venvPython)) {
    Write-Host "Erstelle .venv-build (jinja2 für glad)"
    Invoke-Native { python -m venv .venv-build }
    Invoke-Native { & $venvPython -m pip install --quiet jinja2 }
}

$modeTag = if ($Render) { '-render' } else { '' }
$headlessFlag = if ($Render) { '-DHEADLESS=0' } else { '-DHEADLESS=1' }
$cfgTag = if ($BuildType -eq 'Release') { '-release' } else { '' }
$logRoot = $(if ($Render) { 'build-measure-logs-render' } else { 'build-measure-logs' }) + $cfgTag

foreach ($noConflict in @($false, $true)) {
$suffix = if ($noConflict) { '-noconflict' } else { '' }
$logSub = if ($noConflict) { '/noconflict' } else { '' }
$conflictFlag = if ($noConflict) { 'ON' } else { 'OFF' }

foreach ($name in $Variants) {
    $target = "${name}_main"
    $buildDir = "build-measure$modeTag$cfgTag-$name$suffix"
    $logDir = "$logRoot/$name$logSub"

    Write-Host "=== Variante $name ($target) conflict-analysis-off=$conflictFlag render=$Render type=$BuildType ==="

    Invoke-Native {
        & $CMake -S . -B $buildDir -G Ninja `
            "-DCMAKE_BUILD_TYPE=$BuildType" `
            "-DCMAKE_CXX_FLAGS=$headlessFlag" `
            "-DHELIOS_CONFLICT_ANALYSIS_OFF=$conflictFlag" `
            "-DPython_EXECUTABLE=$venvPython" `
            "-DCMAKE_C_COMPILER=cl" `
            "-DCMAKE_CXX_COMPILER=cl"
    }

    New-Item -ItemType Directory -Force -Path $logDir | Out-Null
    $times = @()

    foreach ($i in 1..$Runs) {
        Write-Host "Messlauf $i/$Runs ($target)"

        Invoke-Native { & $CMake --build $buildDir --target clean | Out-Null }
        Remove-Item -Force -ErrorAction SilentlyContinue "$buildDir/.ninja_log"

        $elapsed = Measure-Command {
            Invoke-Native { & $CMake --build $buildDir --target $target --parallel 1 | Out-Null }
        }
        Write-Host ("real {0:N2}" -f $elapsed.TotalSeconds)
        $times += [pscustomobject]@{ Run = $i; Seconds = [math]::Round($elapsed.TotalSeconds, 2) }

        Copy-Item "$buildDir/.ninja_log" "$logDir/ninja_run_$i.log"
    }

    $times | Export-Csv -NoTypeInformation "$logDir/times.csv"
}
}
