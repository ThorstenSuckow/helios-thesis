#!/usr/bin/env bash
#
# Runs a measurement suite for the thesis scenarios v1..v6.
#
# Every variant is built once (HEADLESS is a compile-time switch); n and the
# frame counts are passed to the programs as runtime arguments. Every
# variant whose v<k>_main.cpp exists is built and run <runs> times.
#
# Usage:
#   ./run-suite.sh --n "10000 40000 90000" --warmup 1000 --measure 5000 \
#                  [--headless true|false] [--runs 10] \
#                  [--variants "v1 v2 v3 v4 v5 v6"] [--expect-build-fail "v4"] \
#                  [--out <dir>] [--suite-id <id>] [--seed <int>] [--worker-wait <ms>] [--spin-duration <n>] [--max-polling <n>] [--threads <count>] \
#                  [--build-type RelWithDebInfo|Release] \
#                  [--n-order ascending|shuffle|sequential] \
#                  [--pause <s>] [--pause-large <s> --large-n <n>] [--pause-round <s>]
#
#   ./run-suite.sh --print-order --n "10000 40000" --runs 10 [--variants "..."] [--seed <int>] \
#                  [--n-order ascending|shuffle|sequential]
#       only prints the order in which the runs would be executed (one line per run, in
#       execution order) and exits; nothing is built, run or written. --warmup/--measure
#       are not needed. With --resume <suite-dir> the parameters (incl. seed) of that suite are used.
#
#   ./run-suite.sh --resume <suite-dir> [--dry-run]
#       continues an interrupted suite: parameters come from <suite-dir>/suite.txt, runs with
#       STATUS=OK and valid output are kept, all others are deleted and run again.
#       --dry-run only lists what would be run (changes nothing).
#
# The order of the variants is shuffled for every (n, run); the seed (suite.txt)
# and the resulting order (run_order.csv) are recorded.
#
# Pauses (seconds, default 0) to let the machine cool down:
#   --pause <s>        before every program run (except the very first)
#   --pause-large <s>  instead of --pause if the PREVIOUS run had n >= --large-n
#   --large-n <n>      threshold for --pause-large (default: no n counts as large)
#   --pause-round <s>  additionally after every complete round over all n (runs outer; not with sequential)
# Example: --pause 30 --pause-large 90 --large-n 40000 --pause-round 180
#
# --n-order (default ascending):
#   ascending   runs outer, n ascending inner: every run walks from small to large n, so the heat of
#               large n does not hit the small ones and each n is spread over the whole suite
#   shuffle     runs outer, order of n shuffled per run (seeded, recorded in run_order.csv)
#   sequential  all runs of one n, then the next n (order as given)
#
# n must be a perfect square (grid of sqrt(n) x sqrt(n) objects).
#
# Result layout:
#   <out>/<suite-id>/
#       suite.txt                      parameters + environment
#       summary.csv                    one line per (n, variant, run) incl. status
#       build/n_<n>_<variant>.log      build logs
#       n_<n>/<variant>/run_<NN>/      CSV files, stdout.log, stderr.log, STATUS
#
# Status values:
#   OK                    exit code 0 and expected output present
#   RUN_FAILED            exit code != 0
#   OUTPUT_INVALID        exit code 0, but output missing / wrong row count
#   BUILD_FAILED          build failed (unexpected)
#   BUILD_FAILED_EXPECTED build failed and variant is listed in --expect-build-fail
#   SKIPPED_NO_SOURCE     v<k>_main.cpp does not exist
#
# Exit code: 0 if no run/build failed unexpectedly, otherwise 1.

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# the script lives in <thesis-scenarios>/scripts; CMakeLists.txt and src/ are one level up
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
SCENARIOS_DIR="${PROJECT_DIR}/src"

HEADLESS="true"
N_LIST=""
WARMUP=""
MEASURE=""
RUNS=10
VARIANTS="v1 v2 v3 v4 v5 v6"
EXPECT_BUILD_FAIL="v4"
OUT_DIR="$(cd "${PROJECT_DIR}/../../.." && pwd)/thesis-measurements/suites"
SUITE_ID="$(date +%Y-%m-%d_%H-%M-%S)"
BUILD_ROOT="${BUILD_ROOT:-${PROJECT_DIR}/cmake-build-suite}"
CXX_COMPILER="${CXX:-/opt/homebrew/opt/llvm/bin/clang++}"
BUILD_TYPE="RelWithDebInfo"
# Python with the glad generator dependencies (jinja2); override via PYTHON=...
PYTHON_BIN="${PYTHON:-${HOME}/venvs/glad/bin/python}"
# seed for the per-run shuffle of the variant order; pass --seed (value in suite.txt) to reproduce an order
SEED="$(( (RANDOM << 15) | RANDOM ))"
# order of the n values: ascending (runs outer, n ascending inner), shuffle (runs outer, n shuffled per run),
# sequential (all runs of one n, then the next n, in the given order)
N_ORDER="ascending"
PAUSE=0
PAUSE_LARGE=""
LARGE_N=""
PAUSE_ROUND=0
LAST_N=""
# idle time in ms before a ThreadPool worker sleeps; empty = program default
WORKER_WAIT=""
THREADS=""
# spin iterations of an idle ThreadPool worker; empty = program default
SPIN_DURATION=""
# max. number of idle ThreadPool workers polling simultaneously; empty = program default
MAX_POLLING=""

RESUME=""
DRY_RUN="false"
PRINT_ORDER="false"
usage() { sed -n '2,45p' "$0" | sed 's/^# \{0,1\}//'; exit 2; }

while [ $# -gt 0 ]; do
    case "$1" in
        --headless)          HEADLESS="$2"; shift 2 ;;
        --n)                 N_LIST="$2"; shift 2 ;;
        --warmup)            WARMUP="$2"; shift 2 ;;
        --measure)           MEASURE="$2"; shift 2 ;;
        --runs)              RUNS="$2"; shift 2 ;;
        --variants)          VARIANTS="$2"; shift 2 ;;
        --expect-build-fail) EXPECT_BUILD_FAIL="$2"; shift 2 ;;
        --out)               OUT_DIR="$2"; shift 2 ;;
        --suite-id)          SUITE_ID="$2"; shift 2 ;;
        --seed)              SEED="$2"; shift 2 ;;
        --n-order)           N_ORDER="$2"; shift 2 ;;
        --pause)             PAUSE="$2"; shift 2 ;;
        --pause-large)       PAUSE_LARGE="$2"; shift 2 ;;
        --large-n)           LARGE_N="$2"; shift 2 ;;
        --pause-round)       PAUSE_ROUND="$2"; shift 2 ;;
        --worker-wait)       WORKER_WAIT="$2"; shift 2 ;;
        --threads)           THREADS="$2"; shift 2 ;;
        --spin-duration)     SPIN_DURATION="$2"; shift 2 ;;
        --max-polling)       MAX_POLLING="$2"; shift 2 ;;
        --build-type)        BUILD_TYPE="$2"; shift 2 ;;
        --resume)            RESUME="$2"; shift 2 ;;
        --dry-run)           DRY_RUN="true"; shift ;;
        --print-order)       PRINT_ORDER="true"; shift ;;
        -h|--help)           usage ;;
        *) echo "Unknown option: $1" >&2; usage ;;
    esac
done

# resume an interrupted suite: all parameters are taken from its suite.txt, runs with
# STATUS=OK and a valid output are kept, everything else is deleted and run again
if [ -n "$RESUME" ]; then
    # a bare suite id is looked up in the output directory
    [ -f "${RESUME}/suite.txt" ] || [ ! -f "${OUT_DIR}/${RESUME}/suite.txt" ] || RESUME="${OUT_DIR}/${RESUME}"
    RESUME="$(cd "$RESUME" 2>/dev/null && pwd)" || { echo "Suite directory not found." >&2; exit 2; }
    [ -f "${RESUME}/suite.txt" ] || { echo "No suite.txt in ${RESUME}" >&2; exit 2; }
    [ -f "${RESUME}/summary.csv" ] || { echo "No summary.csv in ${RESUME}" >&2; exit 2; }
    cfg() { sed -n "s/^$1=//p" "${RESUME}/suite.txt" | head -n 1 | tr -d '\r'; }
    N_LIST="$(cfg n)"; WARMUP="$(cfg warmup_frames)"; MEASURE="$(cfg measure_frames)"
    RUNS="$(cfg runs)"; SEED="$(cfg seed)"; HEADLESS="$(cfg headless)"; N_ORDER="$(cfg n_order)"
    PAUSE="$(cfg pause_s)"; PAUSE_LARGE="$(cfg pause_large_s)"; LARGE_N="$(cfg large_n)"
    PAUSE_ROUND="$(cfg pause_round_s)"
    WORKER_WAIT="$(cfg worker_wait_ms)"; [ "$WORKER_WAIT" = "default" ] && WORKER_WAIT=""
    THREADS="$(cfg threads)"; [ "$THREADS" = "default" ] && THREADS=""
    SPIN_DURATION="$(cfg spin_duration)"; [ "$SPIN_DURATION" = "default" ] && SPIN_DURATION=""
    MAX_POLLING="$(cfg max_polling)"; [ "$MAX_POLLING" = "default" ] && MAX_POLLING=""
    VARIANTS="$(cfg variants)"; EXPECT_BUILD_FAIL="$(cfg expect_build_fail)"
    SUITE_ID="$(cfg suite_id)"
    [ -n "$(cfg build_type)" ] && BUILD_TYPE="$(cfg build_type)"
elif [ "$DRY_RUN" = "true" ]; then
    echo "--dry-run requires --resume." >&2
    exit 2
fi

if [ -z "$N_LIST" ] || { [ "$PRINT_ORDER" != "true" ] && { [ -z "$WARMUP" ] || [ -z "$MEASURE" ]; }; }; then
    echo "--n, --warmup and --measure are required." >&2
    usage
fi
case "$HEADLESS" in true|false) ;; *) echo "--headless must be true or false" >&2; exit 2 ;; esac
case "$BUILD_TYPE" in RelWithDebInfo|Release) ;; *) echo "--build-type must be RelWithDebInfo or Release" >&2; exit 2 ;; esac
# separate build tree per build type
[ "$BUILD_TYPE" = "RelWithDebInfo" ] || BUILD_ROOT="${BUILD_ROOT}-$(echo "$BUILD_TYPE" | tr '[:upper:]' '[:lower:]')"

# integer square root, empty output if n is not a perfect square
axis_of() {
    awk -v n="$1" 'BEGIN { r = int(sqrt(n) + 0.5); if (r * r == n && r > 0) print r }'
}

for n in $N_LIST; do
    if [ -z "$(axis_of "$n")" ]; then
        echo "n=$n is not a perfect square." >&2
        exit 2
    fi
done

# Fisher-Yates shuffle of the lines on stdin, reproducible for a given seed
shuffle() {
    awk -v seed="$1" 'BEGIN { srand(seed) }
        { a[NR] = $0 }
        END {
            for (i = NR; i > 1; i--) { j = int(rand() * i) + 1; t = a[i]; a[i] = a[j]; a[j] = t }
            for (i = 1; i <= NR; i++) print a[i]
        }'
}

# --print-order: only print the order of the runs (same seeds and logic as run_one and the
# main loop below), then exit; nothing is built, run or written
if [ "$PRINT_ORDER" = "true" ]; then
    case "$N_ORDER" in
        ascending|shuffle|sequential) ;;
        *) echo "--n-order must be ascending, shuffle or sequential." >&2; exit 2 ;;
    esac
    N_ASC="$(printf '%s\n' $N_LIST | sort -n | tr '\n' ' ')"
    print_idx=0
    print_run() { # n run
        local v
        for v in $(printf '%s\n' $VARIANTS | shuffle $(( SEED + $1 * 100 + $2 ))); do
            print_idx=$(( print_idx + 1 ))
            printf '#%03d  n=%s  run %02d  %s\n' "$print_idx" "$1" "$2" "$v"
        done
    }
    echo "seed=${SEED} n_order=${N_ORDER} runs=${RUNS}"
    if [ "$N_ORDER" = "sequential" ]; then
        for n in $N_LIST; do
            run=1
            while [ "$run" -le "$RUNS" ]; do print_run "$n" "$run"; run=$(( run + 1 )); done
        done
    else
        run=1
        while [ "$run" -le "$RUNS" ]; do
            if [ "$N_ORDER" = "shuffle" ]; then
                n_seq="$(printf '%s\n' $N_ASC | shuffle $(( SEED + run )) | tr '\n' ' ')"
            else
                n_seq="$N_ASC"
            fi
            for n in $n_seq; do print_run "$n" "$run"; done
            run=$(( run + 1 ))
        done
    fi
    exit 0
fi

SUITE_DIR="${OUT_DIR}/${SUITE_ID}"
[ -n "$RESUME" ] && SUITE_DIR="$RESUME"

# --dry-run (only with --resume): list what would be kept / run again, change nothing
if [ "$DRY_RUN" = "true" ]; then
    todo=0; done_count=0
    N_ASC="$(printf '%s\n' $N_LIST | sort -n | tr '\n' ' ')"
    run=1
    while [ "$run" -le "$RUNS" ]; do
        run_id="$(printf '%02d' "$run")"
        for n in $N_ASC; do
            for variant in $VARIANTS; do
                [ -f "${SCENARIOS_DIR}/${variant}/src/${variant}_main.cpp" ] || continue
                dir="${SUITE_DIR}/n_${n}/${variant}/run_${run_id}"
                ok=1
                [ "$(cat "${dir}/STATUS" 2>/dev/null)" = "OK" ] || ok=0
                if [ "$ok" -eq 1 ] && [ "$HEADLESS" = "true" ]; then
                    [ -f "${dir}/schedule_timings.csv" ] && [ $(( $(wc -l < "${dir}/schedule_timings.csv") - 1 )) -eq "$MEASURE" ] || ok=0
                fi
                if [ "$ok" -eq 1 ]; then
                    done_count=$(( done_count + 1 ))
                else
                    todo=$(( todo + 1 ))
                    if [ -d "$dir" ]; then reason="incomplete, will be discarded"; else reason="missing"; fi
                    echo "  n=${n} ${variant} run ${run_id}: ${reason}"
                fi
            done
        done
        run=$(( run + 1 ))
    done
    echo "Dry run for ${SUITE_DIR}: ${done_count} run(s) complete, ${todo} run(s) would be executed."
    exit 0
fi

if [ -n "$RESUME" ]; then
    # stale build states would suppress the rebuild
    rm -f "${SUITE_DIR}"/build/*.state
elif [ -e "$SUITE_DIR" ]; then
    echo "Suite directory already exists: $SUITE_DIR" >&2
    exit 2
fi
mkdir -p "${SUITE_DIR}/build"

# macOS: prevent idle/system/disk sleep while the suite runs (ends together with this script)
if command -v caffeinate > /dev/null 2>&1; then
    caffeinate -dimsu -w $$ &
fi

if [ "$HEADLESS" != "true" ]; then
    echo "NOTE: --headless false builds the GUI variant (window, frame pacing)." >&2
    echo "      Boot/warmup frames of the engine count towards warmup/measure frames." >&2
fi

SUMMARY="${SUITE_DIR}/summary.csv"
[ -n "$RESUME" ] || echo "n,variant,run,status,exit_code,seconds,directory" > "$SUMMARY"

write_suite_txt() {
    echo "suite_id=${SUITE_ID}"
    echo "started=$(date '+%Y-%m-%d %H:%M:%S %z')"
    echo "headless=${HEADLESS}"
    echo "n=$(echo $N_LIST)"
    echo "n_count=$(echo $N_LIST | wc -w | tr -d ' ')"
    echo "warmup_frames=${WARMUP}"
    echo "measure_frames=${MEASURE}"
    echo "runs=${RUNS}"
    echo "seed=${SEED}"
    echo "n_order=${N_ORDER}"
    echo "pause_s=${PAUSE}"
    echo "pause_large_s=${PAUSE_LARGE}"
    echo "large_n=${LARGE_N}"
    echo "pause_round_s=${PAUSE_ROUND}"
    echo "worker_wait_ms=${WORKER_WAIT:-default}"
    echo "threads=${THREADS:-default}"
    echo "spin_duration=${SPIN_DURATION:-default}"
    echo "max_polling=${MAX_POLLING:-default}"
    echo "variants=$(echo $VARIANTS)"
    echo "expect_build_fail=$(echo $EXPECT_BUILD_FAIL)"
    echo "build_type=${BUILD_TYPE}"
    echo "compiler=${CXX_COMPILER}"
    echo "compiler_version=$("$CXX_COMPILER" --version 2>&1 | head -n 1)"
    echo "host=$(uname -srm)"
    echo "logical_cpus=$(sysctl -n hw.logicalcpu 2>/dev/null || nproc 2>/dev/null)"
    echo "cpu=$(sysctl -n machdep.cpu.brand_string 2>/dev/null)"
}
if [ -n "$RESUME" ]; then
    echo "resumed=$(date '+%Y-%m-%d %H:%M:%S %z')" >> "${SUITE_DIR}/suite.txt"
else
    write_suite_txt > "${SUITE_DIR}/suite.txt"
fi

contains() { case " $2 " in *" $1 "*) return 0 ;; *) return 1 ;; esac; }

[ -n "$RESUME" ] || echo "n,run,order" > "${SUITE_DIR}/run_order.csv"

FAILED=0
# build state per (n, variant): "ok" / "fail" / "skip" -> files in $SUITE_DIR/build
state_file() { echo "${SUITE_DIR}/build/$2.state"; }

record() { # n variant run status exit seconds dir
    echo "$1,$2,$3,$4,$5,$6,$7" >> "$SUMMARY"
}

has_summary_row() { grep -q -- "$1" "$SUMMARY"; }

build_variant() { # n axis variant
    local n="$1" axis="$2" variant="$3"
    local source="${SCENARIOS_DIR}/${variant}/src/${variant}_main.cpp"
    local state; state="$(state_file "$n" "$variant")"

    # the binary does not depend on n/frames (runtime parameters): build once per variant
    [ -f "$state" ] && return

    if [ ! -f "$source" ]; then
        echo "skip" > "$state"
        return
    fi

    local build_dir="${BUILD_ROOT}/h${HEADLESS}"
    local flags="-DHEADLESS=${HEADLESS}"
    local log="${SUITE_DIR}/build/${variant}.log"
    [ -n "$RESUME" ] && log="${SUITE_DIR}/build/${variant}.resume.log"

    echo ">>> build ${variant}"
    if ! cmake -S "$PROJECT_DIR" -B "$build_dir" -G Ninja \
            -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
            -DCMAKE_CXX_COMPILER="$CXX_COMPILER" \
            -DPython_EXECUTABLE="$PYTHON_BIN" \
            -DCMAKE_CXX_FLAGS="$flags" > "$log" 2>&1; then
        echo "fail" > "$state"; return
    fi
    # target name = name of the main source file (src/<variant>/src/<variant>_main.cpp)
    local target="${variant}_main"
    if ! cmake --build "$build_dir" --target "$target" >> "$log" 2>&1; then
        echo "fail" > "$state"; return
    fi

    local binary="${build_dir}/scenarios/src/${variant}/src/${variant}_main"
    if [ -x "$binary" ]; then
        echo "ok $binary" > "$state"
    else
        echo "fail" > "$state"
    fi
}

check_output() { # dir -> 0 if valid
    local dir="$1"
    if [ "$HEADLESS" != "true" ]; then
        return 0
    fi
    local file="${dir}/schedule_timings.csv"
    [ -f "$file" ] || return 1
    local rows; rows=$(( $(wc -l < "$file") - 1 ))
    [ "$rows" -eq "$MEASURE" ]
}

# build everything first (state per (n, variant)), measurements follow
for n in $N_LIST; do
    axis="$(axis_of "$n")"
    for variant in $VARIANTS; do
        build_variant "$n" "$axis" "$variant"
    done
done

run_binary() { # binary n run_dir [extra args...]
    local binary="$1" n="$2" run_dir="$3"
    shift 3
    (
        cd "$(dirname "$binary")" || exit 1
        HELIOS_MEASUREMENTS_DIR="$run_dir" "$binary" \
            --n "$n" --warmup "$WARMUP" --measure "$MEASURE" "$@" \
            > "${run_dir}/stdout.log" 2> "${run_dir}/stderr.log"
    )
}

pause_before_run() { # n of the run that is about to start
    local seconds="$PAUSE"
    if [ -n "$LAST_N" ]; then
        if [ -n "$PAUSE_LARGE" ] && [ -n "$LARGE_N" ] && [ "$LAST_N" -ge "$LARGE_N" ]; then
            seconds="$PAUSE_LARGE"
        fi
        if [ "$seconds" -gt 0 ]; then
            echo "    pause ${seconds} s (previous run n=${LAST_N})"
            sleep "$seconds"
        fi
    fi
    LAST_N="$1"
}

run_one() { # n run
    local n="$1" run="$2"
    local run_id order state run_dir rel_dir binary start extra_args code seconds status variant
    run_id="$(printf '%02d' "$run")"
    # the variant order is shuffled per (n, run) so no variant is systematically first/last
    order="$(printf '%s\n' $VARIANTS | shuffle $(( SEED + n * 100 + run )) | tr '\n' ' ')"
    order="${order% }"
    order_line="${n},${run_id},${order// /;}"
    if [ -z "$RESUME" ] || ! grep -qxF "$order_line" "${SUITE_DIR}/run_order.csv"; then
        echo "$order_line" >> "${SUITE_DIR}/run_order.csv"
    fi
    for variant in $order; do
        state="$(cat "$(state_file "$n" "$variant")")"
        run_dir="${SUITE_DIR}/n_${n}/${variant}/run_${run_id}"
        rel_dir="n_${n}/${variant}/run_${run_id}"

        case "$state" in
            skip)
                [ "$run" -eq 1 ] && ! has_summary_row "^${n},${variant},-," && record "$n" "$variant" "-" "SKIPPED_NO_SOURCE" "-" "-" "-"
                continue ;;
            fail)
                if [ "$run" -eq 1 ] && ! has_summary_row "^${n},${variant},-,"; then
                    if contains "$variant" "$EXPECT_BUILD_FAIL"; then
                        record "$n" "$variant" "-" "BUILD_FAILED_EXPECTED" "-" "-" "build/${variant}.log"
                    else
                        record "$n" "$variant" "-" "BUILD_FAILED" "-" "-" "build/${variant}.log"; FAILED=1
                    fi
                fi
                continue ;;
        esac

        if [ -n "$RESUME" ] && [ -d "$run_dir" ]; then
            if [ "$(cat "${run_dir}/STATUS" 2>/dev/null)" = "OK" ] && check_output "$run_dir"; then
                has_summary_row ",${rel_dir}\$" || record "$n" "$variant" "$run_id" "OK" 0 "-" "$rel_dir"
                continue
            fi
            echo "    discarding incomplete ${rel_dir}"
            rm -rf "$run_dir"
            grep -v ",${rel_dir}\$" "$SUMMARY" > "${SUMMARY}.tmp"; mv "${SUMMARY}.tmp" "$SUMMARY"
        fi

        binary="${state#ok }"
        mkdir -p "$run_dir"
        pause_before_run "$n"
        echo ">>> n=${n} ${variant} run ${run_id}/${RUNS}"

        start=$SECONDS
        # cwd = binary directory: shaders/meshes are loaded relative to it (resources/)
        extra_args=()
        [ -n "$WORKER_WAIT" ] && extra_args=(--worker-wait "$WORKER_WAIT")
        [ -n "$SPIN_DURATION" ] && extra_args+=(--spin-duration "$SPIN_DURATION")
        [ -n "$MAX_POLLING" ] && extra_args+=(--max-polling "$MAX_POLLING")
        [ -n "$THREADS" ] && extra_args+=(--threads "$THREADS")
        run_binary "$binary" "$n" "$run_dir" ${extra_args[@]+"${extra_args[@]}"}
        code=$?
        seconds=$(( SECONDS - start ))

        if [ "$code" -ne 0 ]; then
            status="RUN_FAILED"; FAILED=1
        elif ! check_output "$run_dir"; then
            status="OUTPUT_INVALID"; FAILED=1
        else
            status="OK"
        fi
        echo "$status" > "${run_dir}/STATUS"
        record "$n" "$variant" "$run_id" "$status" "$code" "$seconds" "$rel_dir"
        [ "$status" = "OK" ] || echo "!!! ${variant} n=${n} run ${run_id}: ${status} (exit ${code})" >&2
    done
}

# sort order of the n values
case "$N_ORDER" in
    ascending|shuffle|sequential) ;;
    *) echo "--n-order must be ascending, shuffle or sequential." >&2; exit 2 ;;
esac
N_ASC="$(printf '%s\n' $N_LIST | sort -n | tr '\n' ' ')"

if [ "$N_ORDER" = "sequential" ]; then
    for n in $N_LIST; do
        run=1
        while [ "$run" -le "$RUNS" ]; do
            run_one "$n" "$run"
            run=$(( run + 1 ))
        done
    done
else
    run=1
    while [ "$run" -le "$RUNS" ]; do
        if [ "$N_ORDER" = "shuffle" ]; then
            n_seq="$(printf '%s\n' $N_ASC | shuffle $(( SEED + run )) | tr '\n' ' ')"
        else
            n_seq="$N_ASC"
        fi
        for n in $n_seq; do
            run_one "$n" "$run"
        done
        if [ "$PAUSE_ROUND" -gt 0 ] && [ "$run" -lt "$RUNS" ]; then
            echo "    pause ${PAUSE_ROUND} s (end of round ${run}/${RUNS})"
            sleep "$PAUSE_ROUND"
        fi
        run=$(( run + 1 ))
    done
fi

# build configuration as resolved by CMake (flags of the build type, effective compile command)
cache_value() { sed -n "s/^$1:[A-Z]*=//p" "$CACHE_FILE" | head -n 1; }
CACHE_FILE="${BUILD_ROOT}/h${HEADLESS}/CMakeCache.txt"
ok_variants=""; fail_variants=""; skip_variants=""
for variant in $VARIANTS; do
    sf="${SUITE_DIR}/build/${variant}.state"
    [ -f "$sf" ] || continue
    case "$(cat "$sf")" in
        ok*)  ok_variants="${ok_variants:+$ok_variants }$variant" ;;
        fail) fail_variants="${fail_variants:+$fail_variants }$variant" ;;
        skip) skip_variants="${skip_variants:+$skip_variants }$variant" ;;
    esac
done
{
    echo "variants_built=${ok_variants}"
    echo "variants_build_failed=${fail_variants}"
    echo "variants_skipped_no_source=${skip_variants}"
    if [ -f "$CACHE_FILE" ]; then
        type_upper="$(echo "$BUILD_TYPE" | tr '[:lower:]' '[:upper:]')"
        echo "cmake_version=$(cmake --version 2>&1 | head -n 1)"
        echo "cmake_generator=$(cache_value CMAKE_GENERATOR)"
        echo "cmake_make_program=$(cache_value CMAKE_MAKE_PROGRAM)"
        echo "cxx_compiler_path=$(cache_value CMAKE_CXX_COMPILER)"
        echo "cxx_flags=$(cache_value CMAKE_CXX_FLAGS)"
        echo "cxx_flags_${BUILD_TYPE}=$(cache_value CMAKE_CXX_FLAGS_${type_upper})"
        echo "exe_linker_flags=$(cache_value CMAKE_EXE_LINKER_FLAGS)"
        echo "exe_linker_flags_${BUILD_TYPE}=$(cache_value CMAKE_EXE_LINKER_FLAGS_${type_upper})"
        first_ok="${ok_variants%% *}"
        if [ -n "$first_ok" ]; then
            # effective compile flags of the main source (target options + resolved build type flags)
            line="$(ninja -C "${BUILD_ROOT}/h${HEADLESS}" -t commands "${first_ok}_main" 2>/dev/null | grep "${first_ok}_main.cpp" | head -n 1)"
            flags="$(printf '%s\n' "$line" | tr ' ' '\n' | grep -E '^-' | grep -Ev '^-(I|isystem|o$|c$|MD$|MT|MF|x$|fmodule|fprebuilt)' | tr '\n' ' ')"
            echo "effective_compile_flags=${flags% }"
        fi
    fi
} >> "${SUITE_DIR}/suite.txt"
echo "finished=$(date '+%Y-%m-%d %H:%M:%S %z')" >> "${SUITE_DIR}/suite.txt"
echo "Suite: ${SUITE_DIR}"

SUITE_NAME="$(basename "$SUITE_DIR")"
echo "Auswertung: python scripts/evaluation/evaluate_all.py --suite ${SUITE_NAME}"
echo "Plots:      python scripts/plots/make_plots.py --suite ${SUITE_NAME}"

if [ "$FAILED" -ne 0 ]; then
    echo "There were failed builds/runs, see ${SUMMARY}" >&2
    exit 1
fi
exit 0


