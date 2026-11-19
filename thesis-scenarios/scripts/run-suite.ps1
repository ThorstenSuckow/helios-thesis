<#
.SYNOPSIS
Runs a measurement suite for the thesis scenarios v1..v6 (Windows version of run-suite.sh).

.DESCRIPTION
Every variant is built once (HEADLESS is a compile-time switch); n and the
frame counts are passed to the programs as runtime arguments. Every variant
whose v<k>_main.cpp exists is built and run <Runs> times.

The order of the variants is shuffled for every (n, run); the seed (suite.txt)
and the resulting order (run_order.csv) are recorded. n must be a perfect square.

Requirements: CMake, Ninja and a C++ compiler in PATH. For MSVC start this script
from a "x64 Native Tools Command Prompt" / Developer PowerShell, or set CXX
(e.g. clang++). Python for glad (jinja2): -Python / $env:PYTHON.
Evaluation and plots are separate: see scripts/README.md.

-NOrder (default ascending):
  ascending   runs outer, n ascending inner: every run walks from small to large n
  shuffle     runs outer, order of n shuffled per run (seeded, recorded in run_order.csv)
  sequential  all runs of one n, then the next n (order as given)

Pauses (seconds, default 0) to let the machine cool down:
  -Pause <s>        before every program run (except the very first)
  -PauseLarge <s>   instead of -Pause if the PREVIOUS run had n >= -LargeN
  -LargeN <n>       threshold for -PauseLarge (default: no n counts as large)
  -PauseRound <s>   additionally after every complete round over all n (not with sequential)

.EXAMPLE
.\run-suite.ps1 -N "4096 16384 40000 99856" -Warmup 1000 -Measure 5000 -Runs 10 `
    -NOrder shuffle -Seed 12345 -Pause 30 -PauseLarge 90 -LargeN 40000 -PauseRound 180

.EXAMPLE
.\run-suite.ps1 -N "10000 40000 90000" -Warmup 1000 -Measure 5000 -Runs 10

Result layout:
  <Out>\<SuiteId>\
      suite.txt, summary.csv, run_order.csv
      build\<variant>.log
      n_<n>\<variant>\run_<NN>\   CSV files, stdout.log, stderr.log, STATUS

Status values: OK, RUN_FAILED, OUTPUT_INVALID, BUILD_FAILED,
BUILD_FAILED_EXPECTED, SKIPPED_NO_SOURCE.

Exit code: 0 if no run/build failed unexpectedly, otherwise 1.

-Resume <suite dir>: continue an interrupted suite. Parameters come from its suite.txt;
runs with STATUS=OK and valid output are kept, all others are deleted and run again.
-BuildType RelWithDebInfo|Release (default RelWithDebInfo; Release builds in cmake-build-suite-release).
-DryRun (only with -Resume): list what would be run, change nothing.
-PrintOrder: only print the order in which the runs would be executed (one line per program run, numbered in
execution order) and exit; nothing is built, run or written. -Warmup/-Measure are not needed.
With -Resume the parameters (incl. seed) of that suite are used.
#>
[CmdletBinding()]
param(
    [string]$N = "",
    [int]$Warmup = -1,
    [int]$Measure = -1,
    [ValidateSet("true", "false")][string]$Headless = "true",
    [int]$Runs = 10,
    [string]$Variants = "v1 v2 v3 v4 v5 v6",
    [string]$ExpectBuildFail = "v4",
    [string]$Out = "",
    [string]$Resume = "",
    [string]$SuiteId = (Get-Date -Format "yyyy-MM-dd_HH-mm-ss"),
    [int]$Seed = (Get-Random -Minimum 0 -Maximum 1000000000),
    [int]$WorkerWait = -1,
    [int]$Threads = -1,
    [int]$SpinDuration = -1,
    [int]$MaxPolling = -1,
    [ValidateSet("ascending", "shuffle", "sequential")][string]$NOrder = "ascending",
    [int]$Pause = 0,
    [int]$PauseLarge = -1,
    [int]$LargeN = 0,
    [int]$PauseRound = 0,
    [ValidateSet("RelWithDebInfo", "Release")][string]$BuildType = "RelWithDebInfo",
    [string]$CMake = "",
    [string]$Python = "",
    [switch]$DryRun,
    [switch]$PrintOrder
)

$ErrorActionPreference = "Continue"
# English compiler messages (logs, compiler version line)
$env:VSLANG = "1033"

$ScriptDir = $PSScriptRoot
# the script lives in <thesis-scenarios>\scripts; CMakeLists.txt and src\ are one level up
$ProjectDir = Split-Path $ScriptDir -Parent
$ScenariosDir = Join-Path $ProjectDir "src"
if (-not $Out) { $Out = [System.IO.Path]::GetFullPath((Join-Path $ProjectDir "..\thesis-measurements\suites")) }
$Out = if ([System.IO.Path]::IsPathRooted($Out)) { $Out } else { Join-Path (Get-Location).Path $Out }
$BuildRoot = if ($env:BUILD_ROOT) { $env:BUILD_ROOT } else { Join-Path $ProjectDir "cmake-build-suite" }
$CxxCompiler = $env:CXX

function Resolve-Python($explicit, $envValue, $venvName) {
    if ($explicit) { return $explicit }
    if ($envValue) { return $envValue }
    $venv = Join-Path $HOME "venvs\$venvName\Scripts\python.exe"
    if (Test-Path $venv) { return $venv }
    $local = Join-Path $ProjectDir ".venv-build\Scripts\python.exe"
    if (Test-Path $local) { return $local }
    return "python"
}
$PythonBin = Resolve-Python $Python $env:PYTHON "glad"

# resume an interrupted suite: all parameters are taken from its suite.txt, runs with
# STATUS=OK and a valid output are kept, everything else is deleted and run again
if ($Resume) {
    $ResumeDir = if ([System.IO.Path]::IsPathRooted($Resume)) { $Resume } else { Join-Path (Get-Location).Path $Resume }
    # a bare suite id is looked up in the output directory
    if (-not (Test-Path (Join-Path $ResumeDir "suite.txt")) -and (Test-Path (Join-Path (Join-Path $Out $Resume) "suite.txt"))) { $ResumeDir = Join-Path $Out $Resume }
    $ResumeDir = [System.IO.Path]::GetFullPath($ResumeDir).TrimEnd('\')
    $suiteTxt = Join-Path $ResumeDir "suite.txt"
    if (-not (Test-Path $suiteTxt)) { Write-Error "No suite.txt in $ResumeDir"; exit 2 }
    $cfg = @{}
    foreach ($line in Get-Content $suiteTxt) {
        if ($line -match '^([^=]+)=(.*)$' -and -not $cfg.ContainsKey($Matches[1])) { $cfg[$Matches[1]] = $Matches[2].Trim() }
    }
    $N = $cfg["n"]; $Warmup = [int]$cfg["warmup_frames"]; $Measure = [int]$cfg["measure_frames"]
    $Runs = [int]$cfg["runs"]; $Seed = [int]$cfg["seed"]; $Headless = $cfg["headless"]
    $NOrder = $cfg["n_order"]; $Pause = [int]$cfg["pause_s"]; $PauseRound = [int]$cfg["pause_round_s"]
    $PauseLarge = if ($cfg["pause_large_s"]) { [int]$cfg["pause_large_s"] } else { -1 }
    $LargeN = if ($cfg["large_n"]) { [int]$cfg["large_n"] } else { 0 }
    $WorkerWait = if ($cfg["worker_wait_ms"] -and $cfg["worker_wait_ms"] -ne "default") { [int]$cfg["worker_wait_ms"] } else { -1 }
    $SpinDuration = if ($cfg["spin_duration"] -and $cfg["spin_duration"] -ne "default") { [int]$cfg["spin_duration"] } else { -1 }
    $MaxPolling = if ($cfg["max_polling"] -and $cfg["max_polling"] -ne "default") { [int]$cfg["max_polling"] } else { -1 }
    $Threads = if ($cfg["threads"] -and $cfg["threads"] -ne "default") { [int]$cfg["threads"] } else { -1 }
    $Variants = $cfg["variants"]; $ExpectBuildFail = $cfg["expect_build_fail"]
    $SuiteId = $cfg["suite_id"]
    if ($cfg["build_type"]) { $BuildType = $cfg["build_type"] }
}

# separate build tree per build type (Release does not touch the RelWithDebInfo build)
if ($BuildType -ne "RelWithDebInfo") { $BuildRoot = "$BuildRoot-" + $BuildType.ToLower() }

if (-not $N -or (-not $PrintOrder -and ($Warmup -lt 0 -or $Measure -lt 0))) {
    Write-Error "-N, -Warmup and -Measure are required (or use -Resume)."
    exit 2
}

# Fisher-Yates shuffle, reproducible for a given seed
function Get-Shuffled($items, [int]$seed) {
    $a = @($items)
    $rng = New-Object System.Random $seed
    for ($i = $a.Count - 1; $i -gt 0; $i--) {
        $j = $rng.Next(0, $i + 1)
        $t = $a[$i]; $a[$i] = $a[$j]; $a[$j] = $t
    }
    return $a
}

# -PrintOrder: only print the order of the runs (same seeds and logic as Invoke-OneRun and the
# main loop), then exit; nothing is built, run or written
if ($PrintOrder) {
    $pNList = @($N -split "\s+" | Where-Object { $_ })
    $pVariants = @($Variants -split "\s+" | Where-Object { $_ })
    $pAsc = @($pNList | Sort-Object { [int]$_ })
    Write-Host "seed=$Seed n_order=$NOrder runs=$Runs"
    $printRun = {
        param($n, $run)
        $o = Get-Shuffled $pVariants ($Seed + [int]$n * 100 + $run)
        foreach ($v in $o) {
            $script:printIdx++
            Write-Host ("#{0:D3}  n={1}  run {2:D2}  {3}" -f $script:printIdx, $n, $run, $v)
        }
    }
    $script:printIdx = 0
    if ($NOrder -eq "sequential") {
        foreach ($n in $pNList) { for ($run = 1; $run -le $Runs; $run++) { & $printRun $n $run } }
    } else {
        for ($run = 1; $run -le $Runs; $run++) {
            $seq = if ($NOrder -eq "shuffle") { Get-Shuffled $pAsc ($Seed + $run) } else { $pAsc }
            foreach ($n in $seq) { & $printRun $n $run }
        }
    }
    exit 0
}

# -DryRun (only with -Resume): list what would be kept / run again, change nothing
if ($DryRun) {
    if (-not $Resume) { Write-Error "-DryRun requires -Resume."; exit 2 }
    $todo = 0; $done = 0
    $nAsc = @(($N -split "\s+" | Where-Object { $_ }) | Sort-Object { [int]$_ })
    $summaryRows = @(Get-Content (Join-Path $ResumeDir "summary.csv"))
    foreach ($run in 1..$Runs) {
        foreach ($n in $nAsc) {
            foreach ($variant in ($Variants -split "\s+" | Where-Object { $_ })) {
                if (-not (Test-Path (Join-Path $ScenariosDir "$variant\src\${variant}_main.cpp"))) { continue }
                $runId = "{0:D2}" -f $run
                $dir = Join-Path $ResumeDir "n_$n\$variant\run_$runId"
                $st = Join-Path $dir "STATUS"
                $ok = (Test-Path $st) -and ((Get-Content $st -Raw).Trim() -eq "OK")
                if ($ok -and $Headless -eq "true") {
                    $csv = Join-Path $dir "schedule_timings.csv"
                    $ok = (Test-Path $csv) -and (@([System.IO.File]::ReadLines($csv)).Count - 1 -eq $Measure)
                }
                if ($ok) { $done++; continue }
                $todo++
                $reason = if (Test-Path $dir) { "incomplete, will be discarded" } else { "missing" }
                Write-Host ("  n={0} {1} run {2}: {3}" -f $n, $variant, $runId, $reason)
            }
        }
    }
    Write-Host "Dry run for ${ResumeDir}: $done run(s) complete, $todo run(s) would be executed."
    exit 0
}

# no compiler given: use MSVC and import the Visual Studio build environment if cl is not in PATH yet
if (-not $CxxCompiler) {
    if (-not (Get-Command cl -ErrorAction SilentlyContinue)) {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
        # prefer VS 2022 (17.x): the MSVC of newer VS versions crashes (ICE) on the module sources
        $vsPath = $null
        if (Test-Path $vswhere) {
            $vsPath = & $vswhere -version "[17.0,18.0)" -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
            if (-not $vsPath) { $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1 }
        }
        $vcvars = if ($vsPath) { Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat" } else { $null }
        if ($vcvars -and (Test-Path $vcvars)) {
            Write-Host "importing MSVC environment: $vcvars"
            cmd /c "`"$vcvars`" >nul && set" | ForEach-Object {
                if ($_ -match '^([^=]+)=(.+)$') { Set-Item -Path "Env:\$($Matches[1])" -Value $Matches[2] }
            }
        }
    }
    if (Get-Command cl -ErrorAction SilentlyContinue) { $CxxCompiler = "cl" }
    else { Write-Warning "No MSVC found and CXX not set: CMake picks whatever compiler is in PATH (C++ modules need MSVC or clang)." }
}

# several cmake installations may be in PATH (e.g. Strawberry Perl ships an old one): use the newest
if ($CMake) { $CMakeBin = $CMake }
elseif ($env:CMAKE) { $CMakeBin = $env:CMAKE }
else {
    $best = Get-Command cmake -All -ErrorAction SilentlyContinue | Sort-Object Version -Descending | Select-Object -First 1
    $CMakeBin = if ($best) { $best.Source } else { "cmake" }
}
Write-Host "cmake: $CMakeBin"

$NList = @($N -split "\s+" | Where-Object { $_ })
$VariantList = @($Variants -split "\s+" | Where-Object { $_ })
$ExpectFailList = @($ExpectBuildFail -split "\s+" | Where-Object { $_ })

foreach ($n in $NList) {
    $v = 0L
    if (-not [long]::TryParse($n, [ref]$v) -or $v -le 0) { Write-Error "n=$n is not a perfect square."; exit 2 }
    $r = [long][math]::Round([math]::Sqrt($v))
    if ($r * $r -ne $v) { Write-Error "n=$n is not a perfect square."; exit 2 }
}

$SuiteDir = if ($Resume) { $ResumeDir } else { Join-Path $Out $SuiteId }
if ($Resume) {
    if (-not (Test-Path (Join-Path $SuiteDir "summary.csv"))) { Write-Error "No summary.csv in $SuiteDir"; exit 2 }
} else {
    if (Test-Path $SuiteDir) { Write-Error "Suite directory already exists: $SuiteDir"; exit 2 }
}
New-Item -ItemType Directory -Path (Join-Path $SuiteDir "build") -Force | Out-Null

# prevent idle/system sleep while the suite runs (reset when the process ends)
try {
    Add-Type -Namespace Helios -Name Power -MemberDefinition @'
[System.Runtime.InteropServices.DllImport("kernel32.dll")]
public static extern uint SetThreadExecutionState(uint esFlags);
'@
    # ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED
    [void][Helios.Power]::SetThreadExecutionState(0x80000003)
} catch { }

if ($Headless -ne "true") {
    Write-Warning "-Headless false builds the GUI variant (window, frame pacing). Boot/warmup frames of the engine count towards warmup/measure frames."
}

function Write-Ascii($path, $text, [switch]$Append) {
    if ($Append) { Add-Content -Path $path -Value $text -Encoding Ascii }
    else { Set-Content -Path $path -Value $text -Encoding Ascii }
}

$Summary = Join-Path $SuiteDir "summary.csv"
if (-not $Resume) { Write-Ascii $Summary "n,variant,run,status,exit_code,seconds,directory" }

$compilerVersion = "default"
if ($CxxCompiler) {
    try {
        $banner = (& $CxxCompiler --version 2>&1 | Out-String)
        if ($banner -match 'Version (\d+\.\d+\.\d+)') { $compilerVersion = "MSVC $($Matches[1])" }
        else { $compilerVersion = ($banner -split "\r?\n" | Select-Object -First 1) -replace '[^\x20-\x7E]', '?' }
    } catch { $compilerVersion = "unknown" }
}
$cpu = try { (Get-CimInstance Win32_Processor | Select-Object -First 1).Name } catch { "" }
$logicalCpus = $env:NUMBER_OF_PROCESSORS
$hostInfo = try { (Get-CimInstance Win32_OperatingSystem).Caption + " " + [System.Environment]::OSVersion.Version + " " + $env:PROCESSOR_ARCHITECTURE } catch { "Windows" }

@(
    "suite_id=$SuiteId"
    "started=$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss zzz')"
    "headless=$Headless"
    "n=$($NList -join ' ')"
    "n_count=$($NList.Count)"
    "warmup_frames=$Warmup"
    "measure_frames=$Measure"
    "runs=$Runs"
    "seed=$Seed"
    "n_order=$NOrder"
    "pause_s=$Pause"
    "pause_large_s=$(if ($PauseLarge -ge 0) { $PauseLarge } else { '' })"
    "large_n=$(if ($LargeN -gt 0) { $LargeN } else { '' })"
    "pause_round_s=$PauseRound"
    "worker_wait_ms=$(if ($WorkerWait -ge 0) { $WorkerWait } else { 'default' })"
    "threads=$(if ($Threads -ge 0) { $Threads } else { 'default' })"
    "spin_duration=$(if ($SpinDuration -ge 0) { $SpinDuration } else { 'default' })"
    "max_polling=$(if ($MaxPolling -ge 0) { $MaxPolling } else { 'default' })"
    "variants=$($VariantList -join ' ')"
    "expect_build_fail=$($ExpectFailList -join ' ')"
    "build_type=$BuildType"
    "compiler=$(if ($CxxCompiler) { $CxxCompiler } else { 'cmake default' })"
    "compiler_version=$compilerVersion"
    "host=$hostInfo"
    "logical_cpus=$logicalCpus"
    "cpu=$cpu"
) | ForEach-Object { if (-not $Resume) { $_ } } | Set-Content -Path (Join-Path $SuiteDir "suite.txt") -Encoding Ascii
if ($Resume) { Write-Ascii (Join-Path $SuiteDir "suite.txt") "resumed=$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss zzz')" -Append }

$RunOrder = Join-Path $SuiteDir "run_order.csv"
if (-not $Resume) { Write-Ascii $RunOrder "n,run,order" }

$script:Failed = $false
$BuildState = @{}   # variant -> @{ State = ok|fail|skip; Binary = path }

function Record($n, $variant, $run, $status, $exitCode, $seconds, $dir) {
    Write-Ascii $Summary "$n,$variant,$run,$status,$exitCode,$seconds,$dir" -Append
}

function Get-SummaryRows { @(Get-Content $Summary | Select-Object -Skip 1 | Where-Object { $_ }) }

function Build-Variant($variant) {
    # the binary does not depend on n/frames (runtime parameters): build once per variant
    if ($BuildState.ContainsKey($variant)) { return }

    $source = Join-Path $ScenariosDir "$variant\src\${variant}_main.cpp"
    if (-not (Test-Path $source)) { $BuildState[$variant] = @{ State = "skip" }; return }

    $buildDir = Join-Path $BuildRoot "h$Headless"
    $log = Join-Path $SuiteDir $(if ($Resume) { "build\$variant.resume.log" } else { "build\$variant.log" })
    Write-Host ">>> build $variant"

    $configure = @(
        "-S", $ProjectDir, "-B", $buildDir, "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=$BuildType",
        "-DPython_EXECUTABLE=$PythonBin",
        "-DCMAKE_CXX_FLAGS=-DHEADLESS=$Headless"
    )
    if ($CxxCompiler) { $configure += "-DCMAKE_CXX_COMPILER=$CxxCompiler" }

    & $CMakeBin @configure 2>&1 | Out-File -FilePath $log -Encoding Ascii
    if ($LASTEXITCODE -ne 0) { $BuildState[$variant] = @{ State = "fail" }; return }

    & $CMakeBin --build $buildDir --target "${variant}_main" 2>&1 | Out-File -FilePath $log -Encoding Ascii -Append
    if ($LASTEXITCODE -ne 0) { $BuildState[$variant] = @{ State = "fail" }; return }

    $binary = Join-Path $buildDir "scenarios\src\$variant\src\${variant}_main.exe"
    if (Test-Path $binary) { $BuildState[$variant] = @{ State = "ok"; Binary = $binary } }
    else { $BuildState[$variant] = @{ State = "fail" } }
}

function Test-Output($dir) {
    if ($Headless -ne "true") { return $true }
    $file = Join-Path $dir "schedule_timings.csv"
    if (-not (Test-Path $file)) { return $false }
    $rows = @([System.IO.File]::ReadLines($file)).Count - 1
    return ($rows -eq $Measure)
}

$stopwatch = [System.Diagnostics.Stopwatch]::new()

# build everything first (the binary does not depend on n), measurements follow
foreach ($variant in $VariantList) { Build-Variant $variant }

$script:LastN = $null

function Wait-BeforeRun($n) {
    if ($null -eq $script:LastN) { $script:LastN = [int]$n; return }
    $seconds = $Pause
    if ($PauseLarge -ge 0 -and $LargeN -gt 0 -and $script:LastN -ge $LargeN) { $seconds = $PauseLarge }
    if ($seconds -gt 0) {
        Write-Host "    pause $seconds s (previous run n=$($script:LastN))"
        Start-Sleep -Seconds $seconds
    }
    $script:LastN = [int]$n
}

function Invoke-OneRun($n, [int]$run) {
    $runId = "{0:D2}" -f $run
    # the variant order is shuffled per (n, run) so no variant is systematically first/last
    $order = Get-Shuffled $VariantList ($Seed + [int]$n * 100 + $run)
    $orderLine = "$n,$runId,$($order -join ';')"
    if (-not ($Resume -and (Get-Content $RunOrder) -contains $orderLine)) { Write-Ascii $RunOrder $orderLine -Append }

    foreach ($variant in $order) {
        $st = $BuildState[$variant]
        $relDir = "n_$n/$variant/run_$runId"
        $runDir = Join-Path $SuiteDir "n_$n\$variant\run_$runId"

        if ($st.State -eq "skip") {
            if ($run -eq 1 -and -not ($Resume -and (Get-SummaryRows) -match "^$n,$variant,-,")) { Record $n $variant "-" "SKIPPED_NO_SOURCE" "-" "-" "-" }
            continue
        }
        if ($st.State -eq "fail") {
            if ($run -eq 1 -and -not ($Resume -and (Get-SummaryRows) -match "^$n,$variant,-,")) {
                if ($ExpectFailList -contains $variant) {
                    Record $n $variant "-" "BUILD_FAILED_EXPECTED" "-" "-" "build/$variant.log"
                } else {
                    Record $n $variant "-" "BUILD_FAILED" "-" "-" "build/$variant.log"
                    $script:Failed = $true
                }
            }
            continue
        }

        if ($Resume -and (Test-Path $runDir)) {
            $statusFile = Join-Path $runDir "STATUS"
            if ((Test-Path $statusFile) -and ((Get-Content $statusFile -Raw).Trim() -eq "OK") -and (Test-Output $runDir)) {
                if (-not ((Get-SummaryRows) -match ",$([regex]::Escape($relDir))$")) { Record $n $variant $runId "OK" 0 "-" $relDir }
                continue
            }
            Write-Host "    discarding incomplete $relDir"
            Remove-Item -Recurse -Force $runDir
            $kept = @(Get-SummaryRows | Where-Object { $_ -notmatch ",$([regex]::Escape($relDir))$" })
            Write-Ascii $Summary (@("n,variant,run,status,exit_code,seconds,directory") + $kept)
        }

        New-Item -ItemType Directory -Path $runDir -Force | Out-Null
        Wait-BeforeRun $n
        Write-Host ">>> n=$n $variant run $runId/$Runs"

        $env:HELIOS_MEASUREMENTS_DIR = $runDir
        $stopwatch.Restart()
        # cwd = binary directory: shaders/meshes are loaded relative to it (resources/)
        $runArgs = @("--n", $n, "--warmup", $Warmup, "--measure", $Measure)
        if ($WorkerWait -ge 0) { $runArgs += @("--worker-wait", $WorkerWait) }
        if ($SpinDuration -ge 0) { $runArgs += @("--spin-duration", $SpinDuration) }
        if ($MaxPolling -ge 0) { $runArgs += @("--max-polling", $MaxPolling) }
        if ($Threads -ge 0) { $runArgs += @("--threads", $Threads) }
        $proc = Start-Process -FilePath $st.Binary `
            -ArgumentList $runArgs `
            -WorkingDirectory (Split-Path $st.Binary -Parent) `
            -RedirectStandardOutput (Join-Path $runDir "stdout.log") `
            -RedirectStandardError (Join-Path $runDir "stderr.log") `
            -NoNewWindow -Wait -PassThru
        $code = $proc.ExitCode
        $seconds = [int][math]::Floor($stopwatch.Elapsed.TotalSeconds)
        Remove-Item Env:\HELIOS_MEASUREMENTS_DIR -ErrorAction SilentlyContinue

        if ($code -ne 0) { $status = "RUN_FAILED"; $script:Failed = $true }
        elseif (-not (Test-Output $runDir)) { $status = "OUTPUT_INVALID"; $script:Failed = $true }
        else { $status = "OK" }

        Write-Ascii (Join-Path $runDir "STATUS") $status
        Record $n $variant $runId $status $code $seconds $relDir
        if ($status -ne "OK") {
            Write-Host "!!! $variant n=$n run ${runId}: $status (exit $code)" -ForegroundColor Red
        }
    }
}

$NAscending = @($NList | Sort-Object { [int]$_ })

if ($NOrder -eq "sequential") {
    foreach ($n in $NList) {
        for ($run = 1; $run -le $Runs; $run++) { Invoke-OneRun $n $run }
    }
} else {
    for ($run = 1; $run -le $Runs; $run++) {
        $nSequence = if ($NOrder -eq "shuffle") { Get-Shuffled $NAscending ($Seed + $run) } else { $NAscending }
        foreach ($n in $nSequence) { Invoke-OneRun $n $run }
        if ($PauseRound -gt 0 -and $run -lt $Runs) {
            Write-Host "    pause $PauseRound s (end of round $run/$Runs)"
            Start-Sleep -Seconds $PauseRound
        }
    }
}

# build configuration as resolved by CMake (flags of the build type, effective compile command)
function Get-CacheValue($cache, $name) {
    $m = Select-String -Path $cache -Pattern "^${name}:[A-Z]+=(.*)$" | Select-Object -First 1
    if ($m) { return $m.Matches[0].Groups[1].Value.Trim() }
    return ""
}
$cacheFile = Join-Path (Join-Path $BuildRoot "h$Headless") "CMakeCache.txt"
$buildInfo = @()
$okVariants = @($VariantList | Where-Object { $BuildState[$_].State -eq "ok" })
$failVariants = @($VariantList | Where-Object { $BuildState[$_].State -eq "fail" })
$skipVariants = @($VariantList | Where-Object { $BuildState[$_].State -eq "skip" })
$buildInfo += "variants_built=$($okVariants -join ' ')"
$buildInfo += "variants_build_failed=$($failVariants -join ' ')"
$buildInfo += "variants_skipped_no_source=$($skipVariants -join ' ')"
if (Test-Path $cacheFile) {
    $typeUpper = $BuildType.ToUpper()
    $buildInfo += "cmake=$CMakeBin"
    $buildInfo += "cmake_version=$((& $CMakeBin --version 2>&1 | Select-Object -First 1))"
    $buildInfo += "cmake_generator=$(Get-CacheValue $cacheFile 'CMAKE_GENERATOR')"
    $buildInfo += "cmake_make_program=$(Get-CacheValue $cacheFile 'CMAKE_MAKE_PROGRAM')"
    $buildInfo += "cxx_compiler_path=$(Get-CacheValue $cacheFile 'CMAKE_CXX_COMPILER')"
    $buildInfo += "cxx_flags=$(Get-CacheValue $cacheFile 'CMAKE_CXX_FLAGS')"
    $buildInfo += "cxx_flags_${BuildType}=$(Get-CacheValue $cacheFile "CMAKE_CXX_FLAGS_$typeUpper")"
    $buildInfo += "exe_linker_flags=$(Get-CacheValue $cacheFile 'CMAKE_EXE_LINKER_FLAGS')"
    $buildInfo += "exe_linker_flags_${BuildType}=$(Get-CacheValue $cacheFile "CMAKE_EXE_LINKER_FLAGS_$typeUpper")"
    if ($okVariants.Count -gt 0) {
        # effective compile flags of the main source (includes target options and the cache flags)
        $ninja = Get-CacheValue $cacheFile 'CMAKE_MAKE_PROGRAM'
        $v = $okVariants[0]
        $cmds = & $ninja -C (Join-Path $BuildRoot "h$Headless") -t commands "${v}_main" 2>$null
        $line = $cmds | Where-Object { $_ -match "${v}_main\.cpp" } | Select-Object -First 1
        if ($line) {
            $drop = '^(@|/Fo|/Fd|/FS$|[-/](nologo|TP|showIncludes|scanDependencies)$|/c$|-c$|-o$|-MD$|-MT|-MF|-I|-isystem|/I|[A-Za-z]:[\\/]|"?[A-Za-z]:[\\/])'
            $flags = ($line -split '\s+' | Where-Object { $_ -and $_ -notmatch $drop -and $_ -notmatch '\.(cpp|obj|o|ixx|ifc|d|ddi)$' }) | Select-Object -Skip 1
            $buildInfo += "effective_compile_flags=$($flags -join ' ')"
        }
    }
}
Write-Ascii (Join-Path $SuiteDir "suite.txt") $buildInfo -Append
Write-Ascii (Join-Path $SuiteDir "suite.txt") "finished=$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss zzz')" -Append
Write-Host "Suite: $SuiteDir"

$SuiteName = Split-Path -Leaf $SuiteDir
Write-Host "Auswertung: python scripts/evaluation/evaluate_all.py --suite $SuiteName"
Write-Host "Plots:      python scripts/plots/make_plots.py --suite $SuiteName"

if ($script:Failed) {
    Write-Host "There were failed builds/runs, see $Summary" -ForegroundColor Red
    exit 1
}
exit 0
