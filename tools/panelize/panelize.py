#!/usr/bin/env python3
"""Panelize WingLeft + WingRight + MainBoard (rotated 90deg) with mouse bites.

Reads the latest EasyEDA Pro exports (Gerber zip + pick-and-place CSV + BOM)
for WingLeft, WingRight and MainBoard from export/, and chains them left to
right -- WingLeft -- WingRight -- MainBoard -- each pair placed with a small
air gap and joined at a couple of thin mouse-bite tabs. Each board keeps its
own real, mostly-continuous routed edge, interrupted only at those tabs,
where the cut is left open and a small cluster of NPTH holes is drilled
through instead, so the tab can be snapped by hand. MainBoard is a
horizontal rectangle and gets rotated 90deg so its long edges are vertical,
matching the wings' own facing-edge orientation.

--vertical N repeats that whole three-board row N times top to bottom,
joining each row to the next -- each of the three boards gets its own
vertical mouse-bite connector, sized for that board's own width, since the
three boards aren't the same width and one connector spanning the whole row
wouldn't fit correctly for all of them.

Pick-and-place data gets EasyEDA's centroid bug fixed inline
(fix_pnp_centroids(), ported from export/fix_pnp.py), and gets filtered to
designators that actually appear in the merged BOM (mounting holes,
hand-soldered headers and silkscreen badges never have a BOM entry, and
JLCPCB's assembly check errors on a placement that references one). Writes
a merged Gerber panel, merged pick-and-place CSV and merged BOM CSV to
tools/panelize/out/.
"""

import argparse
import codecs
import csv
import glob
import math
import re
import shutil
import subprocess
import tempfile
import warnings
import zipfile
from collections import defaultdict
from pathlib import Path

warnings.filterwarnings("ignore")

from gerbonara import LayerStack
from gerbonara.apertures import CircleAperture
from gerbonara.excellon import ExcellonTool
from gerbonara.graphic_objects import MM, Flash, Line, Region

EXPORT_DIR = Path(__file__).resolve().parents[2] / "export"
OUT_DIR = Path(__file__).resolve().parent / "out"

# Dimensions below follow JLCPCB's mouse-bite panelization guide:
# https://jlcpcb.com/blog/mouse-bite-panelization-guide
GAP_MM = 2.0                  # panel spacing; guide: typical 1.6 or 2mm, min 1.2mm
EDGE_MARGIN_MM = 10.0         # keep tabs clear of the chamfered corners
TAB_MAX_SPACING_MM = 60.0     # guide: add a tab set every 50-60mm on longer edges
SEAM_LINE_WIDTH_MM = 0.15     # thin routed edge
BITE_HOLE_DIA_MM = 0.6        # guide: 0.60mm holes
BITE_HOLES_PER_TAB = 6        # guide: 5-8 holes per set
BITE_HOLE_EDGE_GAP_MM = 0.375 # guide: 0.35-0.4mm edge-to-edge spacing, min 0.3mm
BITE_HOLE_PITCH_MM = BITE_HOLE_DIA_MM + BITE_HOLE_EDGE_GAP_MM  # center-to-center
# Un-routed bridge width at each tab: just enough to fit the hole cluster
# plus a small clearance margin so the perforation doesn't touch the
# routed edge.
TAB_CLEARANCE_MM = 0.3
# Smallest safe margin to fall back to when a facing edge is too short for
# the default EDGE_MARGIN_MM (e.g. a board rotated so its connector-cutout
# edge faces the join instead of a clean side).
MIN_TAB_CLEARANCE_MM = 1.0
# If even the minimum margin doesn't fit BITE_HOLES_PER_TAB holes, try
# fewer (down to this floor) before giving up -- a short/irregular edge
# (e.g. a connector cutout) is a physical constraint, not something a
# larger gap between boards can fix.
MIN_BITE_HOLES_PER_TAB = 2
PREVIEW_DPI = 600

PNP_XY_PAIRS = (("Ref X", "Ref Y"), ("Pad X", "Pad Y"), ("Mid X", "Mid Y"))

# Each board has its own independent designator namespace (every board has
# its own C1, U1, R1...); this letter (plus a copy index when panelizing
# vertically more than once) disambiguates them once merged, used
# consistently for both the pick-and-place and the BOM so a designator in
# one file always matches the other.
BOARD_LETTER = {"WingLeft": "L", "WingRight": "R", "MainBoard": "M"}


def designator_prefix(letter, copy_index, vertical_copies):
    return f"{letter}_" if vertical_copies == 1 else f"{letter}{copy_index}_"

# Some EasyEDA exports (MainBoard's drill file, at least) use the single-line
# "X..Y..G85X..Y.." canned-slot shorthand for oblong holes, which gerbonara's
# Excellon parser doesn't understand. Rewrite it into the equivalent
# G00/M15/G01/M17 rout sequence, which it does.
G85_SLOT = re.compile(r"X(-?[0-9.]+)Y(-?[0-9.]+)G85X(-?[0-9.]+)Y(-?[0-9.]+)")


def _fix_g85_slots(text):
    def repl(m):
        x1, y1, x2, y2 = m.groups()
        return f"G00X{x1}Y{y1}\nM15\nG01X{x2}Y{y2}\nM17"

    return G85_SLOT.sub(repl, text)


def latest(pattern):
    matches = sorted(glob.glob(str(EXPORT_DIR / pattern)))
    if not matches:
        raise FileNotFoundError(f"no export matching {pattern}")
    return Path(matches[-1])


def load_board(name):
    zip_path = latest(f"Gerber_{name}_*.zip")
    with tempfile.TemporaryDirectory() as tmp:
        with zipfile.ZipFile(zip_path) as zf:
            zf.extractall(tmp)
        tmp_path = Path(tmp)
        for drl in tmp_path.glob("*.DRL"):
            text = drl.read_text()
            fixed = _fix_g85_slots(text)
            if fixed != text:
                drl.write_text(fixed)
        return LayerStack.open_dir(tmp_path, board_name=name)


# A multi-row column's rows are spaced roughly a row pitch apart (tens to
# hundreds of mm), while different edges/tabs belonging to the SAME row
# normally sit within a few mm of each other in the perpendicular
# direction -- comfortably under this. Used to cluster "this row's
# segments" out of a pile that also contains every other row's, before
# picking a side (min/max) within that cluster; without it, a plain
# min/max over ALL rows can land on a different row entirely once rows
# don't share the same facing-edge position (e.g. MainBoard's rotated
# connector-cutout notch needs a real, nonzero, per-row correction).
ROW_CLUSTER_TOL_MM = 30.0
POSITION_TOL_MM = 1.0  # tolerance for "same position" matches on floats carried through several offset() calls


def facing_edge(outline_objects, pick, axis="x", near=None):
    """Return (pos, lo, hi, line): the outline segment `pick` (min or max)
    selects by its position along `axis`, facing an adjacent board.
    axis="x": a vertical segment (x1==x2), selected by its x -- for boards
    placed side by side. axis="y": a horizontal segment (y1==y2), selected
    by its y -- for boards stacked top to bottom.

    A multi-copy column (vertical panelization) has more than one segment
    at the same facing position -- one per row. `near` (a point along the
    segment's own direction: a y for axis="x", an x for axis="y")
    disambiguates by first narrowing to whichever row's segments cluster
    closest to it (within ROW_CLUSTER_TOL_MM), THEN applying `pick` within
    that row alone -- not the other way around, since once rows don't
    share the same facing-edge position, the global min/max across ALL
    rows isn't reliably the row `near` is asking for.

    A single row can *also* have more than one segment at the exact same
    facing position -- e.g. MainBoard's rotated top/bottom edge is split
    into two separate flat stretches by a small notch cut into the middle
    of it. Those ties are broken by picking the widest segment (the one
    with the most along-edge extent), consistently on both sides of a
    join: since a board's top and bottom edges are normally mirror images
    of each other, "widest" picks out the matching pair on each end
    (rather than an arbitrary, possibly mismatched, first-in-list
    segment), which is what keeps a vertical stack of identical boards
    from walking sideways row by row when their top/bottom edges have this
    shape.
    """
    if axis == "x":
        segs = [o for o in outline_objects if isinstance(o, Line) and abs(o.x1 - o.x2) < 1e-6]
        pos_key, mid_key = (lambda l: l.x1), (lambda l: (l.y1 + l.y2) / 2)
        width_key = lambda l: abs(l.y1 - l.y2)
    else:
        segs = [o for o in outline_objects if isinstance(o, Line) and abs(o.y1 - o.y2) < 1e-6]
        pos_key, mid_key = (lambda l: l.y1), (lambda l: (l.x1 + l.x2) / 2)
        width_key = lambda l: abs(l.x1 - l.x2)

    if near is not None:
        row_ref = mid_key(min(segs, key=lambda l: abs(mid_key(l) - near)))
        segs = [s for s in segs if abs(mid_key(s) - row_ref) < ROW_CLUSTER_TOL_MM]
        target_pos = pos_key(pick(segs, key=pos_key))
    else:
        target_pos = pos_key(pick(segs, key=pos_key))

    candidates = [s for s in segs if abs(pos_key(s) - target_pos) < 1e-6]
    line = max(candidates, key=width_key)

    if axis == "x":
        lo, hi = sorted((line.y1, line.y2))
        return line.x1, lo, hi, line
    else:
        lo, hi = sorted((line.x1, line.x2))
        return line.y1, lo, hi, line


def edge_at(outline_objects, axis, pos, near):
    """Find the specific outline segment near a known position `pos` along
    `axis` (within POSITION_TOL_MM, to tolerate float drift accumulated
    across several offset() calls), among possibly several there (one per
    row in a multi-row column), picking the one whose midpoint is closest
    to `near`. Unlike facing_edge(), `pos` is given directly rather than
    picked by min/max -- for locating a specific row's edge within an
    already fully-merged, already-positioned panel. Returns (lo, hi, line).
    """
    if axis == "x":
        segs = [o for o in outline_objects if isinstance(o, Line) and abs(o.x1 - o.x2) < 1e-6 and abs(o.x1 - pos) < POSITION_TOL_MM]
        mid_key = lambda l: (l.y1 + l.y2) / 2
    else:
        segs = [o for o in outline_objects if isinstance(o, Line) and abs(o.y1 - o.y2) < 1e-6 and abs(o.y1 - pos) < POSITION_TOL_MM]
        mid_key = lambda l: (l.x1 + l.x2) / 2

    line = min(segs, key=lambda l: abs(mid_key(l) - near))
    if axis == "x":
        lo, hi = sorted((line.y1, line.y2))
    else:
        lo, hi = sorted((line.x1, line.x2))
    return lo, hi, line


def npth_layer(stack):
    return next(f for f in stack._drill_layers if f.plating_type == "nonplated")


def verify_edge_continuity(outline_objects, expected_gaps):
    """Check that the only breaks in the outline loop are `expected_gaps`.
    Every Line endpoint should be shared by exactly one other Line (closed
    loop) except at those points, which are deliberately open. Raises if a
    corner got left unconnected or an expected gap is missing.
    """
    endpoints = {}
    for o in outline_objects:
        if not isinstance(o, Line):
            continue
        for pt in ((round(o.x1, 4), round(o.y1, 4)), (round(o.x2, 4), round(o.y2, 4))):
            endpoints[pt] = endpoints.get(pt, 0) + 1
    loose = {pt for pt, n in endpoints.items() if n == 1}

    unexpected = loose - expected_gaps
    missing = expected_gaps - loose
    if unexpected or missing:
        raise ValueError(
            f"outline discontinuity: unexpected gap ends {unexpected or 'none'}, "
            f"missing expected gap ends {missing or 'none'}"
        )


def drop_degenerate_regions(layer):
    """Remove zero-area Region objects (exactly zero width or height while
    the other dimension is non-trivial) from a graphic layer.

    Observed: GerberFile.merge() occasionally corrupts one Region's point
    list into a degenerate sliver spanning almost the whole panel (seen on
    a silkscreen logo badge after merging 3 boards) -- mathematically a
    zero-area polygon, which can't be legitimate design intent on any
    layer (even a large copper pour region always has both width and
    height), so it's safe to drop regardless of which layer it's on.
    """
    kept = []
    for o in layer.objects:
        if isinstance(o, Region):
            (x0, y0), (x1, y1) = o.bounding_box()
            w, h = x1 - x0, y1 - y0
            if (w < 1e-6 and h > 1e-3) or (h < 1e-6 and w > 1e-3):
                print(f"  dropping degenerate region: bbox=(({x0:.3f},{y0:.3f}),({x1:.3f},{y1:.3f}))")
                continue
        kept.append(o)
    layer.objects[:] = kept


def merge_stacks(left, right):
    """Merge `right` into `left` layer by layer.

    LayerStack.merge() runs every board's drill hits through
    normalize_drill_layers(), which collapses PTH and NPTH hits from
    multiple source files into one mislabeled "mixed plating" layer on this
    dataset. Merging each graphic layer and each drill file individually
    sidesteps that and keeps PTH/NPTH separate, as exported.
    """
    for key, layer in left.graphic_layers.items():
        layer.merge(right.graphic_layers[key])
        drop_degenerate_regions(layer)
    for left_drill, right_drill in zip(left._drill_layers, right._drill_layers):
        left_drill.merge(right_drill)


def tab_width_for(hole_count):
    return (hole_count - 1) * BITE_HOLE_PITCH_MM + BITE_HOLE_DIA_MM + 2 * TAB_CLEARANCE_MM


def compute_tab_plan(lo_l, hi_l, lo_r, hi_r):
    """Where to put tabs along a shared edge, and how many holes each gets.
    Tabs are only placed where both sides actually face each other, clear
    of the corners where the facing edges end (chamfers). Tries
    BITE_HOLES_PER_TAB holes per tab first (the guide's 5-8
    recommendation) at the default margin, then the minimum safe margin;
    if the facing edge is still too short even at minimum margin -- e.g.
    MainBoard's connector-cutout notch, whose own along-edge extent is
    fixed regardless of how long the gap it bridges is -- tries fewer
    holes per tab (down to MIN_BITE_HOLES_PER_TAB) before giving up, since
    a short/irregular edge is a physical constraint no margin adjustment
    alone can fix. One tab goes at each end of the usable span, plus
    enough evenly spaced interior tabs to keep consecutive tabs no further
    apart than TAB_MAX_SPACING_MM, so the panel is anchored near both ends
    of the seam and satisfies the guide's "a tab set every 50-60mm" rule
    for longer edges. Returns (tab_centers, hole_count).
    """
    overlap_lo = max(lo_l, lo_r)
    overlap_hi = min(hi_l, hi_r)
    overlap_span = overlap_hi - overlap_lo

    chosen = None
    for hole_count in range(BITE_HOLES_PER_TAB, MIN_BITE_HOLES_PER_TAB - 1, -1):
        tab_width = tab_width_for(hole_count)
        min_margin = tab_width / 2 + MIN_TAB_CLEARANCE_MM
        for margin in (EDGE_MARGIN_MM, min_margin):
            if overlap_span - 2 * margin >= tab_width:
                chosen = (hole_count, tab_width, margin)
                break
        if chosen:
            break

    if chosen is None:
        min_tab_width = tab_width_for(MIN_BITE_HOLES_PER_TAB)
        min_margin = min_tab_width / 2 + MIN_TAB_CLEARANCE_MM
        raise ValueError(
            f"facing edge too short or irregular for a safe mouse-bite tab: overlap span is "
            f"{overlap_span:.2f}mm, need at least {2 * min_margin + min_tab_width:.2f}mm even with "
            f"only {MIN_BITE_HOLES_PER_TAB} holes per tab and the minimum safe margin. This can "
            f"happen when a board's edge along this axis is a connector cutout or other notch "
            f"rather than a clean straight edge."
        )

    hole_count, tab_width, margin = chosen
    if hole_count < BITE_HOLES_PER_TAB or margin != EDGE_MARGIN_MM:
        print(f"  note: facing edge is only {overlap_span:.2f}mm; using {hole_count} holes/tab "
              f"(default {BITE_HOLES_PER_TAB}) and {margin:.2f}mm margin (default {EDGE_MARGIN_MM}mm)")

    tab_lo = overlap_lo + margin
    tab_hi = overlap_hi - margin
    span = tab_hi - tab_lo
    tab_count = max(2, math.ceil(span / TAB_MAX_SPACING_MM) + 1)
    tab_centers = [tab_lo + span * i / (tab_count - 1) for i in range(tab_count)]
    return tab_centers, hole_count


def edge_segments_for(axis, pos, lo, hi, tab_centers, tab_width, aperture):
    """Route the full facing edge at `pos` from its own lo to hi, minus a
    tab_width gap at each tab center."""
    def line_at(a, b):
        if axis == "x":
            return Line(x1=pos, y1=a, x2=pos, y2=b, aperture=aperture, polarity_dark=True, unit=MM)
        return Line(x1=a, y1=pos, x2=b, y2=pos, aperture=aperture, polarity_dark=True, unit=MM)

    cuts, cursor = [], lo
    for center in tab_centers:
        cuts.append((cursor, center - tab_width / 2))
        cursor = center + tab_width / 2
    cuts.append((cursor, hi))
    return [line_at(a, b) for a, b in cuts if b > a]


def notch_lines_for(axis, pos_l, pos_r, tab_centers, tab_width, aperture):
    """Close each tab into a real routed notch: a cut across its two other
    sides, from pos_l to pos_r (however far apart that is -- this is the
    same mechanism whether it's a normal small gap or a much longer one
    bridging a row-alignment height difference). Without these, a fab's
    DFM check sees only an unexplained gap in an otherwise-continuous edge
    and doesn't recognize it as an intentional tab -- JLCPCB's checker
    flagged exactly this. The tab's near/far sides (along `axis`) are
    deliberately left un-routed (that gap IS the bridge); only the two
    sides along the tab centers get cut, turning the bridge into a proper
    3-sided-cut notch like a standard castellated tab."""
    lines = []
    for center in tab_centers:
        for level in (center - tab_width / 2, center + tab_width / 2):
            if axis == "x":
                lines.append(Line(x1=pos_l, y1=level, x2=pos_r, y2=level, aperture=aperture, polarity_dark=True, unit=MM))
            else:
                lines.append(Line(x1=level, y1=pos_l, x2=level, y2=pos_r, aperture=aperture, polarity_dark=True, unit=MM))
    return lines


def drill_bite_holes(npth, axis, pos_l, pos_r, tab_centers, hole_count):
    """Perforate each tab with two rows of NPTH holes -- one centered on
    each board's own true edge, so each row straddles that board's edge
    (half in board material, half in the gap, however far apart pos_l and
    pos_r are), per the guide's "centerline of the board frame or
    extending into the board" placement rule applied to both boards."""
    bite_tool = ExcellonTool(diameter=BITE_HOLE_DIA_MM, plated=False, unit=MM)
    hole_offsets = [(i - (hole_count - 1) / 2) * BITE_HOLE_PITCH_MM for i in range(hole_count)]
    for center in tab_centers:
        for offset in hole_offsets:
            for pos in (pos_l, pos_r):
                x, y = (pos, center + offset) if axis == "x" else (center + offset, pos)
                npth.objects.append(Flash(x=x, y=y, aperture=bite_tool, polarity_dark=True, unit=MM))


def bite_join(
    left, right, axis="x", known_gaps=frozenset(), near_l=None, near_r=None,
    primary_override=None, perpendicular_override=None,
):
    """Merge `right` into `left`, positioned GAP_MM beyond `left`'s facing
    edge along `axis` ("x": right goes to the right of left; "y": right
    goes below left) and centered on it along the other axis, joined by a
    castellated mouse-bite tab. Mutates `left` into the combined panel.
    `known_gaps` are gaps from a previous join already present in `left`'s
    outline. `near_l`/`near_r` disambiguate which facing-edge segment to
    use on each side when a multi-row column has more than one at the same
    position (see facing_edge()) -- relevant when this is the first of
    several row-by-row joins between two multi-row columns (see
    reconnect_row() for the rest). They're separate because `left` and
    `right` can have different per-row spacing, so the same hint doesn't
    necessarily land closest to the right row on both sides.
    `primary_override`, if given, replaces the normal GAP_MM-based spacing
    along `axis` -- used to force several boards onto the same shared row
    pitch (see panelize_gerbers()) even though their own natural spacing
    differs; the resulting gap for a shorter board ends up bigger than
    GAP_MM, bridged by a correspondingly longer tab -- the tab/notch/hole
    logic below doesn't care how far apart pos_l and pos_r are, so this
    falls out for free. `perpendicular_override` replaces the normal
    auto-centering on the other axis -- used when stacking identical
    copies of the same board (build_column()'s vertical joins): centering
    on the facing edge's own extent isn't safe there when that edge is a
    notch off to one side rather than symmetric across the board (as seen
    with MainBoard's rotated connector cutout), since it would shift row i
    sideways relative to row 0 and break the horizontal joins downstream,
    which assume a column's X position is constant across its rows.

    Returns (dx, dy, pos_l, pos_r, tab_centers, all_gaps): the offset
    applied to `right` (for shifting its pick-and-place data), the joint's
    geometry, and the updated gap set to pass into the next join.
    """
    if axis == "x":
        pos_l, lo_l, hi_l, line_l = facing_edge(left.outline.objects, max, axis, near_l)
        pos_r, lo_r, hi_r, line_r = facing_edge(right.outline.objects, min, axis, near_r)
        primary = primary_override if primary_override is not None else (pos_l + GAP_MM) - pos_r
    else:
        pos_l, lo_l, hi_l, line_l = facing_edge(left.outline.objects, min, axis, near_l)
        pos_r, lo_r, hi_r, line_r = facing_edge(right.outline.objects, max, axis, near_r)
        primary = primary_override if primary_override is not None else (pos_l - GAP_MM) - pos_r
    perpendicular = (
        perpendicular_override if perpendicular_override is not None
        else (lo_l + hi_l) / 2 - (lo_r + hi_r) / 2
    )

    dx, dy = (primary, perpendicular) if axis == "x" else (perpendicular, primary)
    right.offset(x=dx, y=dy)
    pos_r += primary
    lo_r += perpendicular
    hi_r += perpendicular

    # Replace each board's facing edge with an interrupted version of
    # itself. Each replacement spans the edge's OWN native endpoints
    # (lo_l/hi_l, lo_r/hi_r) -- not the overlap between the two boards --
    # so it reconnects exactly where the original line met the untouched
    # chamfer corners on either side.
    left.outline.objects.remove(line_l)
    right.outline.objects.remove(line_r)

    tab_centers, hole_count = compute_tab_plan(lo_l, hi_l, lo_r, hi_r)
    tab_width = tab_width_for(hole_count)
    edge_aperture = CircleAperture(diameter=SEAM_LINE_WIDTH_MM, unit=MM)
    left.outline.objects.extend(edge_segments_for(axis, pos_l, lo_l, hi_l, tab_centers, tab_width, edge_aperture))
    right.outline.objects.extend(edge_segments_for(axis, pos_r, lo_r, hi_r, tab_centers, tab_width, edge_aperture))
    left.outline.objects.extend(notch_lines_for(axis, pos_l, pos_r, tab_centers, tab_width, edge_aperture))

    # Continuity can only be checked once both sides' edges AND the closers
    # (which reference points on both) are in the same object list, so
    # this waits until after the merge below.
    left_outline_count = len(left.outline.objects)
    right_outline_count = len(right.outline.objects)

    merge_stacks(left, right)
    objects = left.graphic_layers[("mechanical", "outline")].objects
    if len(objects) != left_outline_count + right_outline_count:
        raise ValueError(
            f"merge changed outline object count: expected {left_outline_count + right_outline_count}, "
            f"got {len(objects)}"
        )
    verify_edge_continuity(objects, known_gaps)

    drill_bite_holes(npth_layer(left), axis, pos_l, pos_r, tab_centers, hole_count)

    return dx, dy, pos_l, pos_r, tab_centers, known_gaps


def reconnect_row(panel, axis, pos_l, pos_r, near_l, near_r, known_gaps):
    """Cut one more castellated mouse-bite tab within an already-merged
    `panel`, connecting the facing edges at fixed positions `pos_l`/`pos_r`
    (both sides already correctly placed -- this is for row 2+ of a
    row-by-row join between two multi-row columns, after bite_join()
    already merged and positioned them using row 0). `near_l`/`near_r` pick
    out this row's specific edge segment on each side among the several
    already at pos_l/pos_r (one per row) -- separate because the two
    original columns can have different per-row spacing. Returns
    (tab_centers, all_gaps).
    """
    objects = panel.graphic_layers[("mechanical", "outline")].objects
    lo_l, hi_l, line_l = edge_at(objects, axis, pos_l, near_l)
    lo_r, hi_r, line_r = edge_at(objects, axis, pos_r, near_r)
    objects.remove(line_l)
    objects.remove(line_r)

    tab_centers, hole_count = compute_tab_plan(lo_l, hi_l, lo_r, hi_r)
    tab_width = tab_width_for(hole_count)
    edge_aperture = CircleAperture(diameter=SEAM_LINE_WIDTH_MM, unit=MM)
    objects.extend(edge_segments_for(axis, pos_l, lo_l, hi_l, tab_centers, tab_width, edge_aperture))
    objects.extend(edge_segments_for(axis, pos_r, lo_r, hi_r, tab_centers, tab_width, edge_aperture))
    objects.extend(notch_lines_for(axis, pos_l, pos_r, tab_centers, tab_width, edge_aperture))

    verify_edge_continuity(objects, known_gaps)
    drill_bite_holes(npth_layer(panel), axis, pos_l, pos_r, tab_centers, hole_count)

    return tab_centers, known_gaps


def board_height(name, rotate_rad=None):
    """Load one fresh copy of board `name` (optionally rotated) just to
    measure its overall bounding-box height, then discard it. Used to work
    out the shared row pitch before building any column for real."""
    b = load_board(name)
    if rotate_rad:
        b.rotate(rotate_rad)
    (_, y0), (_, y1) = b.board_bounds()
    return y1 - y0


def board_edge_ref(name, axis, pick, rotate_rad=None):
    """Load one fresh, never-shifted copy of board `name` and return its
    native facing-edge reference midpoint along `axis`. Used as an
    unambiguous "this is row 0's edge" reference in join_columns(): once a
    multi-row column's own vertical join needs a real nonzero
    perpendicular correction (asymmetric top/bottom edges, e.g. MainBoard's
    rotated connector-cutout notch), row 0 and row 1 no longer share the
    same facing-edge position, so picking "the min/max one" without a hint
    can land on the wrong row -- a fresh single board sidesteps that
    entirely, since there's only one row to find."""
    b = load_board(name)
    if rotate_rad:
        b.rotate(rotate_rad)
    _, lo, hi, _ = facing_edge(b.outline.objects, pick, axis)
    return (lo + hi) / 2


def build_column(name, vertical_copies, row_pitch, rotate_rad=None):
    """Load `vertical_copies` independent instances of board `name` and
    stack them vertically, copy 0 on top down to copy N-1 at the bottom,
    joined the same way as the horizontal joins (mouse-bite tabs,
    castellated notches). `row_pitch` (a shared value common to every
    column being built for this panel -- see panelize_gerbers()) is forced
    as the spacing between copies via bite_join()'s primary_override,
    instead of each column defaulting to its own natural GAP_MM-based
    spacing -- so that row i lands at the same height in every column
    regardless of that column's own board height, keeping rows aligned
    across the whole panel. A column shorter than row_pitch just ends up
    with a bigger-than-GAP_MM gap between its own copies, bridged by a
    correspondingly longer tab (still with mouse bites at both ends).

    Returns (column_stack, per_copy_offsets): per_copy_offsets[i] = (dx, dy)
    to apply to copy i's own raw pick-and-place/BOM data (in its native,
    optionally-rotated coordinate frame) to place it within the column;
    copy 0 is always (0, 0) since it's the fixed reference the others join
    onto.
    """
    boards = [load_board(name) for _ in range(vertical_copies)]
    if rotate_rad:
        for b in boards:
            b.rotate(rotate_rad)

    column = boards[0]
    offsets = [(0.0, 0.0)]
    known_gaps = frozenset()
    for i in range(1, vertical_copies):
        # No perpendicular_override here: some boards' top/bottom edges
        # (e.g. MainBoard rotated -- its connector-cutout notch) aren't
        # symmetric, don't naturally line up with zero shift, and need
        # their own real perpendicular correction to overlap at all. That
        # correction can (and for MainBoard does) end up nonzero, which
        # join_columns() accounts for rather than assuming a column's X
        # position is constant across rows.
        dx, dy, _, _, _, known_gaps = bite_join(
            column, boards[i], axis="y", known_gaps=known_gaps, primary_override=-row_pitch,
        )
        offsets.append((dx, dy))
    return column, offsets


def join_columns(left, right, axis, vertical_copies, left_offsets, right_offsets, left_ref0, right_ref0, known_gaps=frozenset()):
    """Horizontally join two (possibly multi-row) columns, creating one
    independent mouse-bite connector per row rather than a single
    connector spanning the whole column -- each row's facing edge is its
    own separate segment (build_column()'s vertical joins only touch the
    top/bottom edges, leaving every row's left/right edge intact), so one
    connector per row is what "all connectors get created" requires; there
    is no meaningful single "center" to align multiple rows on -- rows
    aren't forced into any shared vertical alignment, each is joined on
    its own terms. Row 0 is joined (and positions `right`) with
    bite_join(); every later row reuses that same position and just gets
    its own connector cut via reconnect_row(). `left_offsets`/
    `right_offsets` are each column's own per-row (dx, dy) offsets from
    build_column(), used both to identify each row's edge on its
    respective side (they can have different per-row spacing) and to
    track each row's own facing-edge position -- a column's rows aren't
    necessarily at the same position along `axis` as row 0: a board whose
    two joined edges (this one and the one build_column() used) aren't
    symmetric, like MainBoard's rotated connector-cutout notch, needs a
    real nonzero correction to make its own rows overlap at all, and that
    correction carries through to every row's position here too.
    `left_ref0`/`right_ref0` (from board_edge_ref(), a fresh single-board
    lookup) are each column's row-0 native reference: needed even for row 0
    itself, not just later rows -- once a column's rows don't share the
    same facing-edge position, facing_edge()'s plain min/max (no hint)
    deterministically picks whichever row is more extreme, which is only
    row 0 by coincidence (true for the wings, false for MainBoard).

    Returns (row0_result, known_gaps): row0_result is bite_join()'s row-0
    return tuple, used for shifting `right`'s pick-and-place data -- the
    offset it applied moves `right` as a whole, so it's the same for every
    row.
    """
    perp_idx = 1 if axis == "x" else 0  # which of (dx, dy) is the perpendicular-to-axis component
    pos_idx = 1 - perp_idx  # which of (dx, dy) shifts a facing position along axis
    l_ref0, r_ref0 = left_ref0, right_ref0

    row0 = bite_join(left, right, axis=axis, known_gaps=known_gaps, near_l=l_ref0, near_r=r_ref0)
    known_gaps = row0[5]
    pos_l0, pos_r0 = row0[2], row0[3]
    perp_applied = row0[perp_idx]  # the dx or dy bite_join actually applied to `right`

    for i in range(1, vertical_copies):
        near_l = l_ref0 + left_offsets[i][perp_idx]
        near_r = r_ref0 + perp_applied + right_offsets[i][perp_idx]
        pos_l = pos_l0 + left_offsets[i][pos_idx]
        pos_r = pos_r0 + right_offsets[i][pos_idx]
        _, known_gaps = reconnect_row(left, axis, pos_l, pos_r, near_l, near_r, known_gaps)
    return row0, known_gaps


def panelize_gerbers(vertical_copies=1):
    # A row pitch shared by all three columns, so row i lands at the same
    # height everywhere -- otherwise each column would default to its own
    # natural (GAP_MM-based) spacing and drift apart row by row, since
    # WingLeft, WingRight and MainBoard (rotated) aren't the same height.
    # The tallest board sets the pitch; shorter ones just get a
    # bigger-than-GAP_MM gap between their own copies, which build_column()
    # bridges with a correspondingly longer tab.
    row_pitch = max(
        board_height("WingLeft") + GAP_MM,
        board_height("WingRight") + GAP_MM,
        board_height("MainBoard", math.radians(90)) + GAP_MM,
    )

    wing_left, wl_offsets = build_column("WingLeft", vertical_copies, row_pitch)
    wing_right, wr_offsets = build_column("WingRight", vertical_copies, row_pitch)
    # MainBoard is a horizontal rectangle; rotate it 90deg so its long
    # edges are vertical, matching the wings' own facing-edge orientation.
    main_board, mb_offsets = build_column("MainBoard", vertical_copies, row_pitch, rotate_rad=math.radians(90))

    # Row-0 references, each from a fresh single board -- see join_columns()
    # for why these are needed even for row 0 itself. The panel's "left"
    # side of the second join is WingRight's own outer edge (WingLeft
    # merged into it during the first join), so that join uses WingRight's
    # own per-row offsets too, not WingLeft's.
    wl_ref0 = board_edge_ref("WingLeft", "x", max)
    wr_ref0_inner = board_edge_ref("WingRight", "x", min)
    wr_ref0_outer = board_edge_ref("WingRight", "x", max)
    mb_ref0 = board_edge_ref("MainBoard", "x", min, math.radians(90))

    offset1, known_gaps = join_columns(
        wing_left, wing_right, "x", vertical_copies, wl_offsets, wr_offsets, wl_ref0, wr_ref0_inner
    )
    offset2, known_gaps = join_columns(
        wing_left, main_board, "x", vertical_copies, wr_offsets, mb_offsets, wr_ref0_outer, mb_ref0, known_gaps
    )
    panel = wing_left

    OUT_DIR.mkdir(exist_ok=True)
    gerber_dir = OUT_DIR / "gerbers"
    panel.save_to_directory(gerber_dir)

    out_path = OUT_DIR / "Gerber_Panel.zip"
    with zipfile.ZipFile(out_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for p in sorted(gerber_dir.iterdir()):
            if p.is_file():
                zf.write(p, p.name)
    print(f"panel bounds: {panel.board_bounds()}")
    print(f"wrote {out_path}")

    write_preview(gerber_dir)

    return offset1, offset2, wl_offsets, wr_offsets, mb_offsets


def write_preview(gerber_dir):
    """Render the panel with gerbv, a real Gerber CAM viewer that treats
    outline/drill data as router toolpaths rather than a fill boundary --
    unlike gerbonara's own lightweight SVG renderer, it correctly shows the
    rail of PCB material between the two boards since it isn't relying on
    the (now discontinuous) outline forming a closed polygon."""
    gerbv = shutil.which("gerbv")
    if not gerbv:
        print("gerbv not found on PATH -- skipping preview (sudo apt-get install gerbv)")
        return

    layer_files = sorted(p for p in gerber_dir.iterdir() if p.is_file())

    png_path = OUT_DIR / "Gerber_Panel.png"
    subprocess.run(
        [gerbv, "-a", "-D", str(PREVIEW_DPI), "-x", "png", "-o", str(png_path),
         *[str(p) for p in layer_files]],
        check=True, capture_output=True,
    )
    print(f"wrote {png_path}")


def read_pnp(path):
    with codecs.open(path, "r", encoding="utf-16") as f:
        reader = csv.reader(f, delimiter="\t")
        header = next(reader)
        rows = [row for row in reader if row]
    return header, rows


def write_pnp(path, header, rows):
    def fmt(fields):
        return "\t".join(f'"{v}"' if v else "" for v in fields) + "\r\n"

    with codecs.open(path, "w", encoding="utf-16") as f:
        f.write("\t".join(header) + "\r\n")
        for row in rows:
            f.write(fmt(row))


def parse_mm(value):
    return float(value.strip().removesuffix("mm"))


def format_mm(value):
    return f"{value:.4f}mm"


# EasyEDA computes an incorrect Mid X/Y for components with non-90-degree
# rotations. Ported from export/fix_pnp.py: overwrite Mid X/Y with Ref X/Y
# for those, and treat a 90-degree-multiple rotation whose Mid X/Y already
# disagrees with Ref X/Y as a hard error (would mean something other than
# this known bug is going on).
PNP_EPSILON = 1e-4
PNP_NOT_IN_BOM = {"STDC14_Connector"}  # through-hole/hand-placed, absent from the SMD BOM


def is_multiple_of_90(rotation):
    return abs(rotation % 90) < PNP_EPSILON or abs(rotation % 90 - 90) < PNP_EPSILON


def fix_pnp_centroids(header, rows, source_name):
    idx = {name: i for i, name in enumerate(header)}
    i_des, i_dev = idx["Designator"], idx["Device"]
    i_rx, i_ry, i_rot = idx["Ref X"], idx["Ref Y"], idx["Rotation"]
    i_mx, i_my = idx["Mid X"], idx["Mid Y"]

    errors, fixed = [], []
    out_rows = []
    for row in rows:
        row = list(row)
        rotation = parse_mm(row[i_rot])
        ref_x, ref_y = parse_mm(row[i_rx]), parse_mm(row[i_ry])
        mid_x, mid_y = parse_mm(row[i_mx]), parse_mm(row[i_my])
        off = abs(mid_x - ref_x) > PNP_EPSILON or abs(mid_y - ref_y) > PNP_EPSILON

        if is_multiple_of_90(rotation):
            if off and row[i_dev] not in PNP_NOT_IN_BOM:
                errors.append((row[i_des], row[i_dev], rotation))
        elif off:
            row[i_mx], row[i_my] = row[i_rx], row[i_ry]
            fixed.append((row[i_des], row[i_dev], rotation))

        out_rows.append(row)

    if errors:
        detail = ", ".join(f"{d}({dev}, {r:.0f}deg)" for d, dev, r in errors)
        raise ValueError(
            f"{source_name}: Mid X/Y disagrees with Ref X/Y for {len(errors)} component(s) "
            f"at a 90deg-multiple rotation -- not the known EasyEDA bug, needs a look: {detail}"
        )
    if fixed:
        print(f"  {source_name}: fixed {len(fixed)} centroid(s): " + ", ".join(d for d, _, _ in fixed))
    return out_rows


def shift_row(row, idx, dx, dy):
    """Translate every X/Y coordinate pair in a pick-and-place row by
    (dx, dy) -- same offset bite_join() applied to that board's Gerbers."""
    row = list(row)
    for xcol, ycol in PNP_XY_PAIRS:
        xi, yi = idx[xcol], idx[ycol]
        row[xi] = format_mm(parse_mm(row[xi]) + dx)
        row[yi] = format_mm(parse_mm(row[yi]) + dy)
    return row


def rotate_row(row, idx, angle_rad):
    """Rotate every X/Y coordinate pair (and the Rotation column) in a
    pick-and-place row about the board's own origin, matching
    LayerStack.rotate()'s effect on that board's Gerbers."""
    # Empirically, gerbonara's rotate(angle) with a positive angle turns
    # the board clockwise: (x, y) -> (x*cos+y*sin, y*cos-x*sin). Verified
    # against a known pad's position before/after ls.rotate(radians(90)).
    c, s = math.cos(angle_rad), math.sin(angle_rad)
    row = list(row)
    for xcol, ycol in PNP_XY_PAIRS:
        xi, yi = idx[xcol], idx[ycol]
        x, y = parse_mm(row[xi]), parse_mm(row[yi])
        row[xi] = format_mm(x * c + y * s)
        row[yi] = format_mm(y * c - x * s)
    rot_i = idx.get("Rotation")
    if rot_i is not None:
        row[rot_i] = f"{(float(row[rot_i]) - math.degrees(angle_rad)) % 360:.0f}"
    return row


def load_pnp(name):
    header, rows = read_pnp(latest(f"PickAndPlace_{name}_*.csv"))
    return header, fix_pnp_centroids(header, rows, name)


def merge_pnp(offset1, offset2, wl_offsets, wr_offsets, mb_offsets, bom_designators):
    """`bom_designators` is the set of (prefixed) designators present in
    the merged BOM. Pick-and-place naturally also includes things that are
    never in a BOM -- mounting holes, hand-soldered headers, decorative
    silkscreen badges -- and JLCPCB's assembly check hard-errors if a
    placement's designator has no BOM entry ("won't be assembled due to
    data missing"). Rather than hardcoding which devices those are, rows
    get filtered against the BOM's own designator set, which is the actual
    invariant JLCPCB is checking.

    `wl_offsets`/`wr_offsets`/`mb_offsets` are each board's per-vertical-copy
    offset from build_column() (copy i's own placement within its column);
    `offset1`/`offset2` are the horizontal offsets applied to the whole
    WingRight/MainBoard column when joining columns. A given copy's total
    transform is its within-column offset plus its column's horizontal
    offset (both are plain translations composed after the one rotation
    MainBoard's copies share).
    """
    vertical_copies = len(wl_offsets)
    # (board name, letter, per-copy within-column offsets, rotation, column's horizontal offset)
    specs = [
        ("WingLeft", BOARD_LETTER["WingLeft"], wl_offsets, None, (0.0, 0.0)),
        ("WingRight", BOARD_LETTER["WingRight"], wr_offsets, None, (offset1[0], offset1[1])),
        ("MainBoard", BOARD_LETTER["MainBoard"], mb_offsets, math.radians(90), (offset2[0], offset2[1])),
    ]

    header = idx = des_idx = dev_idx = None
    all_rows = []
    for name, letter, v_offsets, rotation, (extra_dx, extra_dy) in specs:
        h, rows = load_pnp(name)
        if header is None:
            header = h
            idx = {n: j for j, n in enumerate(h)}
            des_idx, dev_idx = idx["Designator"], idx["Device"]
        for i in range(vertical_copies):
            vdx, vdy = v_offsets[i]
            prefix = designator_prefix(letter, i, vertical_copies)
            for row in rows:
                r = rotate_row(row, idx, rotation) if rotation else row
                r = shift_row(r, idx, vdx + extra_dx, vdy + extra_dy)
                r[des_idx] = prefix + r[des_idx]
                all_rows.append(r)

    out_rows = [row for row in all_rows if row[des_idx] in bom_designators]
    skipped = [row for row in all_rows if row[des_idx] not in bom_designators]
    if skipped:
        by_device = defaultdict(list)
        for row in skipped:
            by_device[row[dev_idx]].append(row[des_idx])
        print(f"  dropped {len(skipped)} placement(s) with no BOM entry (mounting holes, hand-soldered headers, silkscreen badges, etc.):")
        for dev in sorted(by_device):
            print(f"    [{dev}]  {', '.join(sorted(by_device[dev]))}")

    out_path = OUT_DIR / "PickAndPlace_Panel.csv"
    write_pnp(out_path, header, out_rows)
    print(f"wrote {out_path} ({len(out_rows)} placements)")


def read_bom(path):
    # Tab-delimited, UTF-16, LF line endings, no field quoting (BOM fields
    # never contain tabs, only commas in the Designator/Unique ID lists).
    with codecs.open(path, "r", encoding="utf-16") as f:
        lines = [line for line in f.read().split("\n") if line]
    return lines[0].split("\t"), [line.split("\t") for line in lines[1:]]


def write_bom(path, header, rows):
    with codecs.open(path, "w", encoding="utf-16") as f:
        f.write("\t".join(header) + "\n")
        for row in rows:
            f.write("\t".join(row) + "\n")


def merge_bom(vertical_copies=1):
    """Merge the three boards' BOMs into one, summing quantities and
    combining designator lists for identical parts instead of listing the
    same part three times. Designators get the same letter(+copy index)
    prefix as the merged pick-and-place file, so the two stay
    cross-referenceable. With vertical_copies > 1, each board's BOM rows
    get counted once per vertical copy, since panelizing N-high really
    does need N times the parts.

    Rows are matched by (Manufacturer Part, Footprint, Value, Comment) --
    Supplier Part (the LCSC number) alone isn't reliable: it's sometimes
    blank (generic parts with no LCSC listing, matched via Manufacturer
    Part instead) and sometimes shared by two rows EasyEDA itself kept
    separate on the same board because their Comment differs (seen with
    the hall-effect sensor: 32 instances as "SC4015SO-N-TR" and 1 as
    "G#4/G4", same Supplier Part). Matching on all four preserves that
    distinction within a board while still merging true duplicates across
    boards.
    """
    header, left_rows = read_bom(latest("BOM_WingLeft_*.csv"))
    _, right_rows = read_bom(latest("BOM_WingRight_*.csv"))
    _, main_rows = read_bom(latest("BOM_MainBoard_*.csv"))
    idx = {name: i for i, name in enumerate(header)}
    i_no, i_qty, i_comment = idx["No."], idx["Quantity"], idx["Comment"]
    i_des, i_footprint, i_value = idx["Designator"], idx["Footprint"], idx["Value"]
    i_mfrpart, i_uid = idx["Manufacturer Part"], idx["Unique ID"]

    groups = {}
    order = []
    boards = (("WingLeft", left_rows), ("WingRight", right_rows), ("MainBoard", main_rows))
    for name, rows in boards:
        for copy_i in range(vertical_copies):
            prefix = designator_prefix(BOARD_LETTER[name], copy_i, vertical_copies)
            for row in rows:
                row = list(row)
                row[i_des] = ",".join(prefix + d for d in row[i_des].split(","))
                key = (row[i_mfrpart], row[i_footprint], row[i_value], row[i_comment])
                if key in groups:
                    existing = groups[key]
                    existing[i_qty] = str(int(existing[i_qty]) + int(row[i_qty]))
                    existing[i_des] += "," + row[i_des]
                    existing[i_uid] += "," + row[i_uid]
                else:
                    groups[key] = row
                    order.append(key)

    out_rows = []
    for i, key in enumerate(order, start=1):
        row = groups[key]
        row[i_no] = str(i)
        out_rows.append(row)

    out_path = OUT_DIR / "BOM_Panel.csv"
    write_bom(out_path, header, out_rows)
    total_parts = sum(int(r[i_qty]) for r in out_rows)
    print(f"wrote {out_path} ({len(out_rows)} line items, {total_parts} total parts)")

    bom_designators = {d for row in out_rows for d in row[i_des].split(",")}
    return bom_designators


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument(
        "--vertical", type=int, default=1, metavar="N",
        help="repeat the panel N times vertically, each copy joined to the next with its own "
             "mouse-bite tabs (default: 1, i.e. no vertical panelization)",
    )
    args = parser.parse_args()
    if args.vertical < 1:
        parser.error("--vertical must be >= 1")
    return args


def main():
    args = parse_args()
    offset1, offset2, wl_offsets, wr_offsets, mb_offsets = panelize_gerbers(args.vertical)
    bom_designators = merge_bom(args.vertical)
    merge_pnp(offset1, offset2, wl_offsets, wr_offsets, mb_offsets, bom_designators)


if __name__ == "__main__":
    main()
