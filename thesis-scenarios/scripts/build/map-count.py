import re
import sys
from collections import defaultdict
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[2]  # <thesis-scenarios>
ROOT = Path(sys.argv[1]) if len(sys.argv) > 1 else PROJECT
# Optional 2. Argument "release": liest build-measure-release-<v> statt build-measure-size-<v>
RELEASE = len(sys.argv) > 2 and sys.argv[2].lower() == "release"
DOMAIN_RE = re.compile(r"U(\w+Domain)@handles@thesis@@")
LINE_RE = re.compile(r"^\s*([0-9a-f]{4}):([0-9a-f]{8})\s+(\S+)\s+([0-9a-f]{16})\b")
SEG_RE = re.compile(r"^\s*([0-9a-f]{4}):([0-9a-f]{8})\s+([0-9a-f]{8})H\s+(\S+)\s+CODE", re.I)


def parse(path):
    text_len = 0
    syms = []
    in_publics = False
    for line in path.read_text(errors="ignore").splitlines():
        m = SEG_RE.match(line)
        if m and m.group(1) == "0001":
            text_len = max(text_len, int(m.group(2), 16) + int(m.group(3), 16))
        if "Publics by Value" in line:
            in_publics = True
            continue
        if in_publics and line.lstrip().startswith("entry point"):
            continue
        if in_publics:
            m = LINE_RE.match(line)
            if m and m.group(1) == "0001":
                syms.append((int(m.group(2), 16), m.group(3)))
    return text_len, syms


rows = []
for variant in ("v1", "v2", "v3", "v5"):
    maps = list(ROOT.glob(f"{'build-measure-release' if RELEASE else 'build-measure-size'}-{variant}/**/{variant}_main.map"))
    if not maps:
        continue
    text_len, syms = parse(maps[0])
    addrs = sorted({a for a, _ in syms})
    size = {a: (addrs[i + 1] if i + 1 < len(addrs) else text_len) - a for i, a in enumerate(addrs)}
    per = defaultdict(lambda: {"sym": 0, "addr": set()})
    for a, name in syms:
        for d in set(DOMAIN_RE.findall(name)):
            per[d]["sym"] += 1
            per[d]["addr"].add(a)
    rows.append((variant, "(gesamt .text)", len(syms), len(addrs), text_len))
    for d in sorted(per):
        rows.append((variant, d, per[d]["sym"], len(per[d]["addr"]), sum(size[a] for a in per[d]["addr"])))

print(f"{'Variante':<9} {'Domain':<26} {'Symbole':>8} {'distinct Adr.':>14} {'Bytes .text':>12}")
for r in rows:
    print(f"{r[0]:<9} {r[1]:<26} {r[2]:>8} {r[3]:>14} {r[4]:>12}")
