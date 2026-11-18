param(
    [string[]]$Variants = @('v1', 'v2', 'v3', 'v5'),

    # Release: nutzt das vorhandene build-measure-release-<v> (nur Neu-Linken mit /MAP)
    [ValidateSet('RelWithDebInfo', 'Release')]
    [string]$BuildType = 'RelWithDebInfo',

    [string]$CMake = "C:\Program Files\JetBrains\CLion 2025.2\bin\cmake\win\x64\bin\cmake.exe",

    [string]$Vcvars = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
)

# Baut jede Variante (HEADLESS, mit Konfliktanalyse) mit /OPT:REF /OPT:ICF /INCREMENTAL:NO /MAP.
# CMake setzt fuer RelWithDebInfo sonst "/debug /INCREMENTAL"; /debug schaltet REF/ICF ab.
# Ergebnis: build-measure-size-<v>\...\<v>_main.map bzw. build-measure-release-<v>\... (Auswertung: build\map-count.py)

$ErrorActionPreference = 'Stop'

# Skript liegt in <thesis-scenarios>\scripts; gearbeitet wird im Projektverzeichnis
Set-Location (Split-Path $PSScriptRoot -Parent)

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
if (-not (Test-Path $venvPython)) { throw ".venv-build fehlt (zuerst measure-build.ps1 ausfuehren)" }

$linkVar = "CMAKE_EXE_LINKER_FLAGS_" + $BuildType.ToUpper()
$linkFlags = if ($BuildType -eq 'Release') { "/INCREMENTAL:NO /OPT:REF /OPT:ICF /MAP" } else { "/debug /INCREMENTAL:NO /OPT:REF /OPT:ICF /MAP" }

foreach ($name in $Variants) {
    $buildDir = if ($BuildType -eq 'Release') { "build-measure-release-$name" } else { "build-measure-size-$name" }
    Write-Host "=== Variante $name ($buildDir, $BuildType) ==="

    Invoke-Native {
        & $CMake -S . -B $buildDir -G Ninja `
            "-DCMAKE_BUILD_TYPE=$BuildType" `
            "-DCMAKE_CXX_FLAGS=-DHEADLESS=1" `
            "-D$linkVar=$linkFlags" `
            "-DHELIOS_CONFLICT_ANALYSIS_OFF=OFF" `
            "-DPython_EXECUTABLE=$venvPython" `
            "-DCMAKE_C_COMPILER=cl" `
            "-DCMAKE_CXX_COMPILER=cl"
    }

    Invoke-Native { & $CMake --build $buildDir --target "${name}_main" }
}
