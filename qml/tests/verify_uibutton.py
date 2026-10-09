#!/usr/bin/env python3
"""PIL verification of the UiButton parity harness renders (Nuxt UI parity).

Runs after `qml/tests/tst_uibutton.qml`; see that file for the qmltestrunner
invocation. The two screenshots land in the runner's working directory:

    python3 qml/tests/verify_uibutton.py uibutton.log uibutton-base.png uibutton-hot.png

Reads RECT BASE / RECT HOT geometry + computed colours from the harness log and
checks the two PNGs against them:

  * background just outside each button (no ring in BASE)
  * alpha-composited fill sample inside each button (BASE and HOT)
  * label pixels near the computed foreground (skipped for icon-only buttons)
  * a solid non-neutral button carries the carbon ink #181818 (independent
    spec check — the other rules only confirm the render matches what the
    component computed, this one pins the choice itself)
  * disabled = same colours through the 0.75 opacity
  * rounded corners: arc span at depth 1 == w - 2*(r - sqrt(2rd - d^2))
  * focus ring in HOT: 3px band *outside* the box at stroke alpha 0.25
  * hover: solid primary fill goes from bg-primary to bg-primary/75
  * BASE vs HOT differ only in the hovered button, the focus ring
    and the animated spinner of the loading row
"""
import math
import re
import sys
from PIL import Image

LOG = sys.argv[1] if len(sys.argv) > 1 else "uibutton.log"
BASE_PNG = sys.argv[2] if len(sys.argv) > 2 else "uibutton-base.png"
HOT_PNG = sys.argv[3] if len(sys.argv) > 3 else "uibutton-hot.png"

RADIUS = 12
RING = 3          # focus-visible:outline-3
TOL = 6           # per-channel tolerance for flat areas
TEXT_TOL = 40     # text is antialiased

rect_re = re.compile(
    r"RECT (BASE|HOT) (\S+) x=([\d.]+) y=([\d.]+) w=([\d.]+) h=([\d.]+) "
    r"fill=([\d.]+),([\d.]+),([\d.]+),([\d.]+) "
    r"fg=([\d.]+),([\d.]+),([\d.]+),([\d.]+) "
    r"stroke=([\d.]+),([\d.]+),([\d.]+),([\d.]+) enabled=(\w+)"
    r"(?: color=(\S+) variant=(\S+))?")

rows = {"BASE": [], "HOT": []}
for line in open(LOG, errors="replace"):
    m = rect_re.search(line)
    if m:
        rows[m.group(1)].append(dict(
            name=m.group(2),
            x=float(m.group(3)), y=float(m.group(4)),
            w=float(m.group(5)), h=float(m.group(6)),
            fill=tuple(float(m.group(i)) for i in range(7, 11)),
            fg=tuple(float(m.group(i)) for i in range(11, 15)),
            stroke=tuple(float(m.group(i)) for i in range(15, 19)),
            enabled=m.group(19) == "true",
            color=m.group(20), variant=m.group(21)))

if not rows["BASE"]:
    sys.exit("no RECT BASE lines in %s" % LOG)

base = Image.open(BASE_PNG).convert("RGB")
hot = Image.open(HOT_PNG).convert("RGB")
BG = base.getpixel((5, 5))
BGF = tuple(c / 255 for c in BG)

results = []


def over(fg, a, bg):
    return tuple(fg[i] * a + bg[i] * (1 - a) for i in range(3))


def to255(t):
    return tuple(max(0, min(255, round(c * 255))) for c in t)


def close(px, exp, tol):
    return all(abs(px[i] - exp[i]) <= tol for i in range(3))


def record(ok, what):
    results.append((ok, what))


def rect_of(r, pad=0):
    x, y = int(round(r["x"])), int(round(r["y"]))
    w, h = int(round(r["w"])), int(round(r["h"]))
    return (x - pad, y - pad, x + w + pad, y + h + pad)


def inside(pt, box):
    return box[0] <= pt[0] <= box[2] and box[1] <= pt[1] <= box[3]


record(True, "page background = %s" % (BG,))

# 0. the harness must have loaded cleanly. A broken Theme binding only shows up
#    as scattered colour failures (or not at all), so fail loudly instead: this
#    is what caught the harness losing its Theme import when it moved out of
#    the qml/ directory it used to get implicitly.
qml_errors = [l.rstrip() for l in open(LOG, errors="replace")
              if re.search(r"ReferenceError|TypeError|SyntaxError|is not defined"
                           r"|Unable to assign|Binding loop", l)]
record(not qml_errors, "harness log has no QML errors")
for e in qml_errors[:8]:
    results.append((False, "  ERR  " + e))

for tag in ("BASE", "HOT"):
    for r in rows[tag]:
        n, x, y, w, h = r["name"], r["x"], r["y"], r["w"], r["h"]
        x, y, w, h = int(round(x)), int(round(y)), int(round(w)), int(round(h))
        icon_only = n.startswith("q-")

        fill_c = over(r["fill"][:3], r["fill"][3], BGF)
        op = 1.0 if r["enabled"] else 0.75
        exp_fill = tuple(BGF[i] + op * (fill_c[i] - BGF[i]) for i in range(3))
        exp_fill255 = to255(exp_fill)

        # 1. background just outside the left edge (only valid in BASE, where
        #    nothing is hovered or focused)
        if tag == "BASE":
            out = base.getpixel((x - 3, y + h // 2))
            record(close(out, BG, TOL),
                   "%s/%s: outside-left %s == bg %s" % (tag, n, out, BG))

        # 2. flat fill, past the 1px inset ring and clear of the leading icon
        img = base if tag == "BASE" else hot
        inside_px = img.getpixel((x + 3, y + h // 2))
        record(close(inside_px, exp_fill255, TOL),
               "%s/%s: fill %s == expected %s" % (tag, n, inside_px, exp_fill255))

        # 3. label pixels near the computed foreground (icon-only rows have none)
        if not icon_only:
            fg_fill = over(r["fg"][:3], r["fg"][3], exp_fill)
            fg_op = tuple(exp_fill[i] + op * (fg_fill[i] - exp_fill[i])
                          for i in range(3))
            exp_fg = to255(fg_op)
            box = img.crop((x + int(w * 0.2), y + int(h * 0.25),
                            x + int(w * 0.85), y + int(h * 0.8)))
            hits = sum(1 for p in box.getdata() if close(p, exp_fg, TEXT_TOL))
            record(hits >= 8,
                   "%s/%s: label pixels near fg %s = %d" % (tag, n, exp_fg, hits))

        # 3b. independent spec check: a solid non-neutral button carries the
        #     carbon ink #181818, NOT Nuxt's text-inverted. Checks 2 and 3 only
        #     confirm the render matches what the component computed; this one
        #     pins the choice itself. text-inverted is white in light mode,
        #     i.e. 1.83–3.65:1 on every accent — all fail WCAG AA (4.5:1) —
        #     while the token is #181818 in both modes (4.66–11.3:1), so the
        #     assertion holds for light and dark runs alike.
        if r["variant"] == "solid" and r["color"] not in (None, "neutral"):
            want = (24 / 255, 24 / 255, 24 / 255, 1.0)
            ink_ok = all(abs(r["fg"][i] - want[i]) < 1e-3 for i in range(4))
            record(ink_ok,
                   "%s/%s: solid %s fg %s == carbon ink #181818"
                   % (tag, n, r["color"], tuple(round(c, 4) for c in r["fg"])))

        # 4. rounded corners: fill span one row below the top edge must match
        #    the circular arc inset r - sqrt(2*r*d - d*d)
        if r["fill"][3] == 1.0 and w > 3 * RADIUS and h > 2 * RADIUS:
            d = 1
            inset = RADIUS - math.sqrt(2 * RADIUS * d - d * d)
            exp_span = w - 2 * inset
            row = [img.getpixel((x + i, y + d)) for i in range(w)]
            span = sum(1 for p in row if close(p, exp_fill255, TOL))
            lo, hi = exp_span - 5, exp_span + 5
            record(lo <= span <= hi,
                   "%s/%s: arc span %d in [%.1f,%.1f] (radius %d)"
                   % (tag, n, span, lo, hi, RADIUS))

# 5. disabled opacity really differs from enabled
on = [r for r in rows["BASE"] if r["name"] == "d-on"]
off = [r for r in rows["BASE"] if r["name"] == "d-off"]
if on and off:
    a = base.getpixel((int(on[0]["x"]) + 3, int(on[0]["y"]) + 16))
    b = base.getpixel((int(off[0]["x"]) + 3, int(off[0]["y"]) + 16))
    record(a != b, "disabled fill %s != enabled fill %s" % (b, a))

# 6. hover: solid primary goes from bg-primary (alpha 1) to bg-primary/75
hv_b = [r for r in rows["BASE"] if r["name"] == "hover"][0]
hv_h = [r for r in rows["HOT"] if r["name"] == "hover"][0]
record(abs(hv_b["fill"][3] - 1.0) < 1e-6,
       "hover BASE fill alpha = %.3f (bg-primary)" % hv_b["fill"][3])
record(abs(hv_h["fill"][3] - 0.75) < 1e-6,
       "hover HOT fill alpha = %.3f (hover:bg-primary/75)" % hv_h["fill"][3])
hx, hy = int(hv_h["x"]) + 3, int(hv_h["y"]) + int(hv_h["h"]) // 2
exp75 = to255(over(hv_h["fill"][:3], 0.75, BGF))
got75 = hot.getpixel((hx, hy))
record(close(got75, exp75, TOL),
       "hover pixel %s == bg-primary/75 over bg %s" % (got75, exp75))

# 7. focus ring: 3px band *outside* the border box, stroke colour at 0.25
fb = [r for r in rows["BASE"] if r["name"] == "focus"][0]
fx, fy, fw, fh = int(fb["x"]), int(fb["y"]), int(fb["w"]), int(fb["h"])
ring255 = to255(over(fb["stroke"][:3], 0.25, BGF))
in_base = base.getpixel((fx - 2, fy + fh // 2))
in_hot = hot.getpixel((fx - 2, fy + fh // 2))
just_out = hot.getpixel((fx - RING - 1, fy + fh // 2))
record(close(in_base, BG, TOL),
       "focus ring absent in BASE at (fx-2): %s" % (in_base,))
record(close(in_hot, ring255, 12),
       "focus ring in HOT at (fx-2): %s == %s" % (in_hot, ring255))
record(close(just_out, BG, TOL),
       "ring stops at %dpx: (fx-%d) = %s back to bg" % (RING, RING + 1, just_out))
# corner: the ring follows the enlarged radius, so the point diagonally outside
# the box stays background while the 45-degree band carries the ring colour
corner = hot.getpixel((fx - 1, fy - 1))
diag = hot.getpixel((fx + 2, fy + 2))
record(close(corner, BG, TOL),
       "rounded ring: (fx-1,fy-1) outside the band = bg %s" % (corner,))
record(close(diag, ring255, 12),
       "ring follows the radius along the diagonal: %s == %s" % (diag, ring255))

# 8. BASE vs HOT may differ only in: hovered button, focus ring, spinner rows
allowed = [rect_of(hv_b, 4), rect_of(fb, RING + 4)]
for r in rows["BASE"]:
    if r["name"].startswith("load-"):
        allowed.append(rect_of(r, 2))

bp, hp = base.load(), hot.load()
strays = []
n_diff = 0
for yy in range(base.size[1]):
    for xx in range(base.size[0]):
        if bp[xx, yy] != hp[xx, yy]:
            n_diff += 1
            if not any(inside((xx, yy), b) for b in allowed):
                strays.append((xx, yy))
                if len(strays) > 5:
                    break
    if len(strays) > 5:
        break
record(not strays,
       "BASE/HOT differ only in hover/focus/spinner: %d px, strays=%s"
       % (n_diff, strays[:5]))

bad = [t for ok, t in results if not ok]
print("checks: %d  passed: %d  failed: %d"
      % (len(results), len(results) - len(bad), len(bad)))
for ok, t in results:
    if not ok or "-v" in sys.argv:
        print(("  OK   " if ok else "  FAIL ") + t)
sys.exit(1 if bad else 0)
