#!/usr/bin/env python3
"""PIL verification of the UiListElement render harness.

Runs after `qml/tests/tst_uilistelement.qml`; see that file for the
qmltestrunner invocation. The two screenshots land in the runner's working
directory:

    python3 qml/tests/verify_uilistelement.py uilistelement.log \
        uilistelement-base.png uilistelement-hot.png

Reads CARD BASE / CARD HOT geometry + computed colours from the harness log and
checks the two PNGs against them:

  * the card fill matches Theme.cardBackground
  * the 1px border matches Theme.border when idle and Theme.ring when hovered
    (this is the regression: the old hover painted a square row fill over the
    card's rounded corners instead of recolouring the rounded border)
  * both top corners are rounded (corner pixel falls back to the page bg)
  * the fill arc one row below the top edge matches the radius
"""
import math
import re
import sys
from PIL import Image

LOG = sys.argv[1] if len(sys.argv) > 1 else "uilistelement.log"
BASE_PNG = sys.argv[2] if len(sys.argv) > 2 else "uilistelement-base.png"
HOT_PNG = sys.argv[3] if len(sys.argv) > 3 else "uilistelement-hot.png"

RADIUS = 12
TOL = 8

card_re = re.compile(
    r"CARD (BASE|HOT) (\S+) x=([\d.-]+) y=([\d.-]+) w=([\d.-]+) h=([\d.-]+) "
    r"radius=([\d.-]+) "
    r"fill=([\d.]+),([\d.]+),([\d.]+),([\d.]+) "
    r"border=([\d.]+),([\d.]+),([\d.]+),([\d.]+) "
    r"borderW=(\d+) hovered=(\w+)")

cards = {"BASE": [], "HOT": []}
lines = open(LOG, errors="replace").readlines()
for line in lines:
    m = card_re.search(line)
    if m:
        cards[m.group(1)].append(dict(
            name=m.group(2),
            x=float(m.group(3)), y=float(m.group(4)),
            w=float(m.group(5)), h=float(m.group(6)),
            radius=float(m.group(7)),
            fill=tuple(float(m.group(i)) for i in range(8, 12)),
            border=tuple(float(m.group(i)) for i in range(12, 16)),
            borderW=int(m.group(16)),
            hovered=m.group(17) == "true"))

if not cards["BASE"] or not cards["HOT"]:
    sys.exit("need both CARD BASE and CARD HOT lines in %s" % LOG)

base = Image.open(BASE_PNG).convert("RGB")
hot = Image.open(HOT_PNG).convert("RGB")
BG = base.getpixel((5, 5))

results = []


def to255(t):
    return tuple(max(0, min(255, round(c * 255))) for c in t)


def close(px, exp, tol):
    return all(abs(px[i] - exp[i]) <= tol for i in range(3))


def record(ok, what):
    results.append((ok, what))


# 0. harness must have loaded cleanly
qml_errors = [l.rstrip() for l in lines
              if re.search(r"ReferenceError|TypeError|SyntaxError|is not defined"
                           r"|Unable to assign|Binding loop", l)]
record(not qml_errors, "harness log has no QML errors")
for e in qml_errors[:8]:
    results.append((False, "  ERR  " + e))

record(True, "page background = %s" % (BG,))

b = cards["BASE"][0]
h = cards["HOT"][0]

record(b["radius"] == RADIUS and h["radius"] == RADIUS,
       "radius = %g (=%d)" % (b["radius"], RADIUS))
record(b["borderW"] == 1 and h["borderW"] == 1,
       "border width = 1")

exp_fill = to255(b["fill"][:3])
exp_border = to255(b["border"][:3])
exp_ring = to255(h["border"][:3])

bx, by, bw, bh = int(b["x"]), int(b["y"]), int(b["w"]), int(b["h"])
hx, hy, hw, hh = int(h["x"]), int(h["y"]), int(h["w"]), int(h["h"])

# fill matches Theme.cardBackground in the middle
record(close(base.getpixel((bx + bw // 2, by + bh // 2)), exp_fill, TOL),
       "BASE fill %s == cardBackground %s"
       % (base.getpixel((bx + bw // 2, by + bh // 2)), exp_fill))
record(close(hot.getpixel((hx + hw // 4, hy + hh // 2)), exp_fill, TOL),
       "HOT fill %s == cardBackground %s"
       % (hot.getpixel((hx + hw // 4, hy + hh // 2)), exp_fill))

# the 1px border sits on the top edge at centre-x
base_edge = [base.getpixel((bx + bw // 2, by + d)) for d in range(0, 4)]
hot_edge = [hot.getpixel((hx + hw // 2, hy + d)) for d in range(0, 4)]
record(any(close(p, exp_border, TOL) for p in base_edge),
       "BASE top border %s carries border %s" % (base_edge[:2], exp_border))
record(any(close(p, exp_ring, TOL) for p in hot_edge),
       "HOT top border %s carries ring %s" % (hot_edge[:2], exp_ring))
record(exp_border != exp_ring,
       "border %s != ring %s (hover recolours the border)" % (exp_border, exp_ring))
record(b["hovered"] is False and h["hovered"] is True,
       "hovered flips BASE=%s HOT=%s" % (b["hovered"], h["hovered"]))

# both top corners are rounded: the extreme corner pixel is outside the arc
record(close(base.getpixel((bx, by)), BG, 12),
       "BASE top-left corner %s == bg %s (rounded)"
       % (base.getpixel((bx, by)), BG))
record(close(base.getpixel((bx + bw - 1, by)), BG, 12),
       "BASE top-right corner %s == bg %s (rounded)"
       % (base.getpixel((bx + bw - 1, by)), BG))
record(close(hot.getpixel((hx, hy)), BG, 12),
       "HOT top-left corner %s == bg %s (rounded)"
       % (hot.getpixel((hx, hy)), BG))

# fill arc one row down matches the radius. The 1px border is drawn inward, so
# at depth 1 the fill follows the inner rectangle: its top edge spans
# w - 2*(border + (radius - border)) == w - 2*radius.
if bw > 3 * RADIUS and bh > 2 * RADIUS:
    d = 1
    exp_span = bw - 2 * RADIUS
    row = [base.getpixel((bx + i, by + d)) for i in range(bw)]
    span = sum(1 for p in row if close(p, exp_fill, TOL))
    record(exp_span - 8 <= span <= exp_span + 8,
           "BASE fill arc span %d ~= %.1f (radius %d)" % (span, exp_span, RADIUS))

bad = [t for ok, t in results if not ok]
print("checks: %d  passed: %d  failed: %d"
      % (len(results), len(results) - len(bad), len(bad)))
for ok, t in results:
    if not ok or "-v" in sys.argv:
        print(("  OK   " if ok else "  FAIL ") + t)
sys.exit(1 if bad else 0)
