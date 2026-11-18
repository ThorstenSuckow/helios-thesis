import sys
from pathlib import Path
from statistics import mean

COMMON = ["main.cpp", "main (link)", "DeltaManager", "TaggedDeltaManager", "LambdaManager", "BetaSystem",
          "DeltaDetSystem", "DeltaResSystem", "UniDeltaDetSystem", "UniDeltaResSystem", "LambdaSystem",
          "TaggedDeltaDetSystem", "TaggedDeltaResSystem"]

def avg(log_dir, variant, unit):
    if unit == "main (link)":
        match = lambda o: o.endswith(f"/{variant}_main.exe")
    elif unit == "main.cpp":
        match = lambda o: f"CMakeFiles/{variant}_main.dir/" in o and o.endswith(f"/{variant}_main.cpp.obj")
    else:
        match = lambda o: f"CMakeFiles/{variant}_main.dir/" in o and o.endswith(f"/{unit}.ixx.obj")
    times = []
    for log in sorted(log_dir.glob("ninja_run_*.log")):
        for line in log.read_text().splitlines()[1:]:
            start, end, _, output, _ = line.split("\t")
            if match(output):
                times.append((int(end) - int(start)) / 1000)
    return (mean(times), min(times), max(times), len(times)) if times else None


def fmt(v):
    return f"{v[0]:.2f} [{v[1]:.2f}-{v[2]:.2f}]" if v is not None else "-"


PROJECT = Path(__file__).resolve().parents[2]  # <thesis-scenarios>
LOG_ROOT = Path(sys.argv[1]) if len(sys.argv) > 1 else PROJECT / "build-measure-logs"
if not LOG_ROOT.is_absolute() and not LOG_ROOT.exists():
    LOG_ROOT = PROJECT / LOG_ROOT

for variant_dir in sorted(p for p in LOG_ROOT.iterdir() if p.is_dir()):
    variant = variant_dir.name
    off_dir = variant_dir / "noconflict"
    print(f"== {variant} ==  (Sekunden: Mittelwert [Min-Max])")
    print(f"  {'Einheit':<22} {'mit Analyse':>22} {'ohne':>22} {'Delta':>8}")
    for unit in COMMON:
        on = avg(variant_dir, variant, unit)
        off = avg(off_dir, variant, unit) if off_dir.is_dir() else None
        delta = f"{on[0] - off[0]:.2f}" if on and off else "-"
        n = f"n={on[3]}/{off[3]}" if on and off else ""
        print(f"  {unit:<22} {fmt(on):>22} {fmt(off):>22} {delta:>8} {n}")
