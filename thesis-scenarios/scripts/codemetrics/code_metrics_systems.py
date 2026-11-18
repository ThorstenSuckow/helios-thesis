#!/usr/bin/env python3
"""Collect LOC (cloc) and code complexity (lizard) for the systems in src/lib/systems.

Requirements:
    cloc    (CLI, e.g. `brew install cloc`)
    lizard  (`pip install lizard`)

Outputs (in --out):
    systems_metrics.csv    one row per file: cloc (blank/comment/code) + lizard (functions, CCN)
    systems_functions.csv  one row per function: nloc, CCN, tokens, parameters, length

.ixx is unknown to both tools: cloc is forced to C++, lizard analyses the source as C++.
"""
from __future__ import annotations

import argparse
import csv
import shutil
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_SRC = SCRIPT_DIR.parent.parent / "src" / "lib" / "systems"
# files without own implementation (module aggregation)
DEFAULT_EXCLUDE = {"_modules.ixx"}


def run_cloc(files: list[Path]) -> dict[str, dict[str, int]]:
    """Return {file name: {blank, comment, code}} from cloc."""
    if shutil.which("cloc") is None:
        sys.exit("cloc not found. Install it, e.g. `brew install cloc`.")
    cmd = ["cloc", "--by-file", "--csv", "--quiet", "--force-lang=C++,ixx",
           "--list-file=-"]
    proc = subprocess.run(cmd, input="\n".join(str(f) for f in files),
                          capture_output=True, text=True)
    if proc.returncode != 0:
        sys.exit(f"cloc failed: {proc.stderr.strip()}")

    result: dict[str, dict[str, int]] = {}
    lines = [l for l in proc.stdout.splitlines() if l.strip()]
    reader = csv.DictReader(lines)
    for row in reader:
        name = Path(row["filename"]).name
        if name == "SUM":
            continue
        result[name] = {
            "blank": int(row["blank"]),
            "comment": int(row["comment"]),
            "code": int(row["code"]),
        }
    return result


def run_lizard(files: list[Path]):
    """Return (per-file dict, per-function list) from lizard."""
    try:
        import lizard
    except ImportError:
        sys.exit("lizard not found. Install it with `pip install lizard`.")

    per_file: dict[str, dict] = {}
    functions: list[dict] = []
    for f in files:
        # pass a .cpp name so that lizard selects the C++ reader
        info = lizard.analyze_file.analyze_source_code(
            f.stem + ".cpp", f.read_text(encoding="utf-8"))
        ccns = [fn.cyclomatic_complexity for fn in info.function_list]
        per_file[f.name] = {
            "functions": len(ccns),
            "sum_ccn": sum(ccns),
            "avg_ccn": round(sum(ccns) / len(ccns), 2) if ccns else 0.0,
            "max_ccn": max(ccns) if ccns else 0,
        }
        for fn in info.function_list:
            functions.append({
                "file": f.name,
                "function": fn.long_name,
                "nloc": fn.nloc,
                "ccn": fn.cyclomatic_complexity,
                "tokens": fn.token_count,
                "parameters": len(fn.parameters),
                "length": fn.length,
                "start_line": fn.start_line,
            })
    return per_file, functions


def write_csv(path: Path, rows: list[dict]) -> None:
    if not rows:
        return
    with path.open("w", newline="", encoding="utf-8") as fh:
        writer = csv.DictWriter(fh, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--src", type=Path, default=DEFAULT_SRC,
                        help="directory with the system files (default: src/lib/systems)")
    parser.add_argument("--out", type=Path, default=SCRIPT_DIR / "system_metrics",
                        help="output directory for the CSV files")
    parser.add_argument("--include-modules", action="store_true",
                        help="also analyse _modules.ixx")
    args = parser.parse_args()

    files = sorted(p for p in args.src.glob("*.ixx")
                   if args.include_modules or p.name not in DEFAULT_EXCLUDE)
    if not files:
        sys.exit(f"No .ixx files in {args.src}")

    cloc = run_cloc(files)
    liz, functions = run_lizard(files)

    rows = []
    for f in files:
        c = cloc.get(f.name, {"blank": 0, "comment": 0, "code": 0})
        rows.append({"file": f.name, **c, **liz[f.name]})
    total = {"file": "SUM"}
    for key in ("blank", "comment", "code", "functions", "sum_ccn"):
        total[key] = sum(r[key] for r in rows)
    total["avg_ccn"] = round(total["sum_ccn"] / total["functions"], 2) if total["functions"] else 0.0
    total["max_ccn"] = max(r["max_ccn"] for r in rows)
    rows.append(total)

    args.out.mkdir(parents=True, exist_ok=True)
    write_csv(args.out / "systems_metrics.csv", rows)
    write_csv(args.out / "systems_functions.csv", functions)

    header = f"{'file':42}{'code':>6}{'cmt':>6}{'blank':>7}{'fn':>4}{'avgCCN':>8}{'maxCCN':>8}"
    print(header)
    for r in rows:
        print(f"{r['file']:42}{r['code']:>6}{r['comment']:>6}{r['blank']:>7}"
              f"{r['functions']:>4}{r['avg_ccn']:>8}{r['max_ccn']:>8}")
    print(f"\nWritten to {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())






