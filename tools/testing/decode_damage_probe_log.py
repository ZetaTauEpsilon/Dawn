"""Summarise captured B7E3C0 damage-probe lines.

The probe caps its raw per-hit rows, so the window lines carry the authoritative
totals and the rows carry the handle-level detail. This reports both, and answers
the two questions the capture exists for: whether B7E3C0 observes the local
player's outgoing hits at all, and whether the attacker handle it passes matches
the local controlled entity exactly or only by pool row.

Nothing here infers a hit that was not logged; a quiet capture reports zero.
"""
import argparse
import json
import math
import re
import sys
from collections import Counter
from pathlib import Path

STAMP = re.compile(r"\bt=(\d+)\b")
PROBE = re.compile(r"\bev=damage_probe\s+stage=(summary|window)\b")
FIELD = re.compile(r"\b([a-z_]+)=(-?[0-9A-Za-z_.+-]+)")

COUNTS = ("calls", "span_ms", "out", "out_row", "in", "in_row", "self", "other",
          "unknown", "killed", "mode", "regions", "finite", "nonfinite", "nonpositive")
AMOUNTS = ("sum", "out_sum", "min", "max")
DIRECTIONS = ("out", "out_row", "in", "in_row", "self", "other", "unknown")


# Handles are logged as %08X. An all-digit one would otherwise read as decimal and
# misname the entity, so these keep their printed form.
HEX_FIELDS = ("attacker", "target", "local")


def number(key, text):
    """Parse a probe value, keeping handles, inf and nan in the form the log used."""
    if key in HEX_FIELDS:
        return text
    try:
        return int(text)
    except ValueError:
        pass
    try:
        return float(text)
    except ValueError:
        return text


def parse(lines):
    rows, windows = [], []
    for line in lines:
        kind = PROBE.search(line)
        if not kind:
            continue
        fields = {key: number(key, value) for key, value in FIELD.findall(line)}
        stamp = STAMP.search(line)
        fields["t"] = int(stamp.group(1)) if stamp else None
        (rows if kind.group(1) == "summary" else windows).append(fields)
    return rows, windows


def totals(windows):
    """Add the window lines. These are complete; the raw rows are capped."""
    result = {key: 0 for key in COUNTS}
    result["sum"] = result["out_sum"] = 0.0
    result["min"], result["max"] = math.inf, -math.inf
    for window in windows:
        for key in COUNTS:
            result[key] += window.get(key, 0)
        for key in ("sum", "out_sum"):
            result[key] += window.get(key, 0.0)
        if window.get("finite", 0):
            result["min"] = min(result["min"], window.get("min", math.inf))
            result["max"] = max(result["max"], window.get("max", -math.inf))
    if not result["finite"]:
        result["min"] = result["max"] = 0.0
    return result


def plural(count, singular, many=None):
    return "%d %s" % (count, singular if count == 1 else (many or singular + "s"))


def summaries(count):
    return plural(count, "summary", "summaries")


def verdict(total):
    """State only what the counts support, and name what is still unestablished."""
    lines = []
    if not total["calls"]:
        return ["no damage_probe windows in this capture: the hook never ran, the log level "
                "excluded it, or no damage was dealt"]
    if not total["out"] + total["out_row"]:
        lines.append("NO outgoing summaries: B7E3C0 fired %d times and never named the local "
                     "player as attacker. A meter cannot be built on it as hooked."
                     % total["calls"])
    elif total["out"] and not total["out_row"]:
        lines.append("attacker matches the local controlled entity exactly (%s); salt needs no "
                     "special handling" % summaries(total["out"]))
    elif total["out_row"] and not total["out"]:
        lines.append("attacker matches the local pool row but NEVER the salt (%s): B7E3C0 "
                     "carries a differently salted handle, so a meter must compare rows and "
                     "prove identity another way" % summaries(total["out_row"]))
    else:
        lines.append("attacker matches exactly %d times and by row only %d times: the salt is "
                     "not stable across summaries and must be accounted for"
                     % (total["out"], total["out_row"]))
    if total["unknown"]:
        lines.append("no local player resolved on %s (4B2260 returned nothing), so direction is "
                     "unestablished there" % summaries(total["unknown"]))
    if total["nonfinite"]:
        lines.append("%s %s not finite and must be rejected by the meter"
                     % (plural(total["nonfinite"], "amount"),
                        "was" if total["nonfinite"] == 1 else "were"))
    if total["nonpositive"]:
        lines.append("%s %s at or below zero: B7E3C0 also reports hits that dealt nothing, so a "
                     "meter must filter them"
                     % (plural(total["nonpositive"], "finite amount"),
                        "was" if total["nonpositive"] == 1 else "were"))
    if total["in"] or total["in_row"]:
        lines.append("%s targeted the local player, so the same hook can feed an incoming-damage "
                     "display" % summaries(total["in"] + total["in_row"]))
    if total["other"]:
        lines.append("%s involved neither end, so B7E3C0 is engine-wide and not player-scoped; "
                     "filtering is required" % summaries(total["other"]))
    if total["regions"] < total["calls"]:
        lines.append("the regions argument was absent on %d of %d summaries, so it cannot be a "
                     "required source of per-hit detail"
                     % (total["calls"] - total["regions"], total["calls"]))
    if total["span_ms"] and total["out_sum"]:
        lines.append("observed outgoing rate over %.1fs of windows: %.1f damage/s"
                     % (total["span_ms"] / 1000.0,
                        total["out_sum"] * 1000.0 / total["span_ms"]))
    lines.append("coverage against the hits actually dealt is NOT established here: this counts "
                 "what B7E3C0 reported, not what the game applied")
    return lines


def report(rows, windows, total):
    print("== damage probe capture ==")
    print("raw rows: %d (capped per run)   windows: %d" % (len(rows), len(windows)))
    print("summaries: %d over %.1fs" % (total["calls"], total["span_ms"] / 1000.0))
    print("direction: " + "  ".join("%s=%d" % (key, total[key]) for key in DIRECTIONS))
    print("flags:     killed=%d mode=%d regions=%d" % (total["killed"], total["mode"], total["regions"]))
    print("amounts:   finite=%d nonfinite=%d nonpositive=%d min=%g max=%g sum=%g out_sum=%g"
          % (total["finite"], total["nonfinite"], total["nonpositive"],
             total["min"], total["max"], total["sum"], total["out_sum"]))
    if rows:
        print("\n-- raw rows --")
        attackers = Counter(row.get("attacker") for row in rows)
        targets = Counter(row.get("target") for row in rows)
        locals_ = Counter(row.get("local") for row in rows)
        print("distinct attacker=%d target=%d local=%d"
              % (len(attackers), len(targets), len(locals_)))
        print("local handles: " + ", ".join(sorted(str(h) for h in locals_)))
        print("by direction:  " + "  ".join(
            "%s=%d" % (key, count) for key, count in Counter(row.get("dir") for row in rows).most_common()))
        print("first rows:")
        for row in rows[:8]:
            print("  seq=%s dir=%s attacker=%s target=%s local=%s killed=%s amount=%s"
                  % (row.get("seq"), row.get("dir"), row.get("attacker"), row.get("target"),
                     row.get("local"), row.get("killed"), row.get("amount")))
    print("\n-- what this establishes --")
    for line in verdict(total):
        print("  * " + line)


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("log", nargs="?", type=Path,
                        help="dawn.log to read; omitted reads standard input")
    parser.add_argument("--json", action="store_true", help="emit the totals as JSON instead")
    args = parser.parse_args()
    text = args.log.read_text(encoding="utf-8", errors="replace").splitlines() \
        if args.log else sys.stdin.read().splitlines()
    rows, windows = parse(text)
    total = totals(windows)
    if args.json:
        print(json.dumps({"rows": rows, "windows": windows, "totals": total,
                          "verdict": verdict(total)}, indent=2))
    else:
        report(rows, windows, total)


if __name__ == "__main__":
    main()
