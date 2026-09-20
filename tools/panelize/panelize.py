#!/usr/bin/env python3
"""Panelize WingLeft + WingRight + MainBoard (rotated 90deg) with mouse bites.

Reads the latest EasyEDA Pro exports (Gerber zip + pick-and-place CSV) for
WingLeft, WingRight and MainBoard from export/, and chains them left to
right -- WingLeft -- WingRight -- MainBoard -- each pair placed with a small
air gap and joined at a couple of thin mouse-bite tabs. Each board keeps its
own real, mostly-continuous routed edge, interrupted only at those tabs,
where the cut is left open and a small cluster of NPTH holes is drilled
through instead, so the tab can be snapped by hand. MainBoard is a
horizontal rectangle and gets rotated 90deg so its long edges are vertical,
matching the wings' own facing-edge orientation. Pick-and-place data also
gets EasyEDA's centroid bug fixed inline (see fix_pnp_centroids(), ported
from export/fix_pnp.py) before merging. Writes a merged Gerber panel and
merged pick-and-place CSV to tools/panelize/out/.
"""

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
TAB_WIDTH_MM = (BITE_HOLES_PER_TAB - 1) * BITE_HOLE_PITCH_MM + BITE_HOLE_DIA_MM + 2 * TAB_CLEARANCE_MM
PREVIEW_DPI = 600

PNP_XY_PAIRS = (("Ref X", "Ref Y"), ("Pad X", "Pad Y"), ("Mid X", "Mid Y"))

# Each board has its own independent designator namespace (every board has
# its own C1, U1, R1...); this prefix disambiguates them once merged, used
# consistently for both the pick-and-place and the BOM so a designator in
# one file always matches the other.
BOARD_PREFIX = {"WingLeft": "L_", "WingRight": "R_", "MainBoard": "M_"}

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


def facing_edge(outline_objects, pick):
    """Return (x, y_lo, y_hi, line) of the vertical outline segment `pick`
    (min or max) selects by its x position -- i.e. the edge facing the other
    board."""
    verticals = [o for o in outline_objects if isinstance(o, Line) and abs(o.x1 - o.x2) < 1e-6]
    line = pick(verticals, key=lambda l: l.x1)
    y_lo, y_hi = sorted((line.y1, line.y2))
    return line.x1, y_lo, y_hi, line


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


def bite_join(left, right, known_gaps=frozenset()):
    """Merge `right` into `left`, positioned GAP_MM to the right of `left`'s
    facing edge and vertically centered on it, joined by routed edges that
    are interrupted at a few thin mouse-bite tabs. Mutates `left` into the
    combined panel. `known_gaps` are tab gaps from a previous join already
    present in `left`'s outline (e.g. chaining a third board onto an
    already-joined panel) -- without them the continuity check would flag
    that earlier joint's still-open gaps as unexpected. Returns
    (dx, dy, lx, rx, tab_centers, all_gaps): the offset applied to `right`
    (for shifting its pick-and-place data), the joint's geometry (for the
    reference layer), and the updated gap set to pass into the next join.
    """
    lx, ly_lo, ly_hi, l_line = facing_edge(left.outline.objects, max)
    rx, ry_lo, ry_hi, r_line = facing_edge(right.outline.objects, min)

    dx = (lx + GAP_MM) - rx
    dy = (ly_lo + ly_hi) / 2 - (ry_lo + ry_hi) / 2
    right.offset(x=dx, y=dy)
    rx += dx
    ry_lo += dy
    ry_hi += dy

    # Replace each board's facing edge with an interrupted version of
    # itself: routed almost the full length, with a short open gap at each
    # tab. The rail between lx and rx stays attached to both boards only at
    # those tabs. Each replacement spans the edge's OWN native endpoints
    # (ly_lo/ly_hi, ry_lo/ry_hi) -- not the overlap between the two boards --
    # so it reconnects exactly where the original line met the untouched
    # chamfer corners on either side.
    left.outline.objects.remove(l_line)
    right.outline.objects.remove(r_line)

    # Tabs are only placed where both edges actually face each other, and
    # clear of the corners where the facing edges end (chamfers).
    overlap_lo = max(ly_lo, ry_lo)
    overlap_hi = min(ly_hi, ry_hi)
    tab_lo = overlap_lo + EDGE_MARGIN_MM
    tab_hi = overlap_hi - EDGE_MARGIN_MM
    if tab_hi <= tab_lo:
        raise ValueError("boards don't overlap enough for the requested tab margin")

    # One tab at each end of the usable span, plus enough evenly spaced
    # interior tabs to keep consecutive tabs no further apart than
    # TAB_MAX_SPACING_MM, so the panel is anchored near the top and bottom
    # of the seam (not just somewhere in the middle) and satisfies the
    # guide's "a tab set every 50-60mm" rule for longer edges.
    span = tab_hi - tab_lo
    tab_count = max(2, math.ceil(span / TAB_MAX_SPACING_MM) + 1)
    tab_centers = [tab_lo + span * i / (tab_count - 1) for i in range(tab_count)]

    def edge_segments(x, y_lo, y_hi, aperture):
        """Route the full facing edge at `x` from its own y_lo to y_hi,
        minus a TAB_WIDTH_MM gap at each tab."""
        cuts, cursor = [], y_lo
        for center in tab_centers:
            cuts.append((cursor, center - TAB_WIDTH_MM / 2))
            cursor = center + TAB_WIDTH_MM / 2
        cuts.append((cursor, y_hi))
        return [
            Line(x1=x, y1=y0, x2=x, y2=y1, aperture=aperture, polarity_dark=True, unit=MM)
            for y0, y1 in cuts
            if y1 > y0
        ]

    edge_aperture = CircleAperture(diameter=SEAM_LINE_WIDTH_MM, unit=MM)
    left.outline.objects.extend(edge_segments(lx, ly_lo, ly_hi, edge_aperture))
    right.outline.objects.extend(edge_segments(rx, ry_lo, ry_hi, edge_aperture))

    # Close each tab into a real routed notch: a cut across its top and its
    # bottom, from lx to rx. Without these, a fab's DFM check sees only an
    # unexplained gap in an otherwise-continuous edge and doesn't recognize
    # it as an intentional tab -- JLCPCB's checker flagged exactly this.
    # The tab's left/right sides are deliberately left un-routed (that gap
    # IS the bridge); only top/bottom get cut, turning the bridge into a
    # proper 3-sided-cut notch like a standard castellated tab.
    for center in tab_centers:
        for y in (center - TAB_WIDTH_MM / 2, center + TAB_WIDTH_MM / 2):
            left.outline.objects.append(
                Line(x1=lx, y1=y, x2=rx, y2=y, aperture=edge_aperture, polarity_dark=True, unit=MM)
            )

    # Continuity can only be checked once both sides' vertical edges AND the
    # horizontal closers (which reference points on both) are in the same
    # object list, so this waits until after the merge below.
    left_outline_count = len(left.outline.objects)
    right_outline_count = len(right.outline.objects)

    merge_stacks(left, right)
    objects = left.graphic_layers[("mechanical", "outline")].objects
    if len(objects) != left_outline_count + right_outline_count:
        raise ValueError(
            f"merge changed outline object count: expected {left_outline_count + right_outline_count}, "
            f"got {len(objects)}"
        )
    # Every tab is now a fully closed notch (top/bottom cut, left/right
    # deliberately open but bridged by the un-routed gap itself -- not a
    # loose outline endpoint), so nothing should be left unconnected beyond
    # gaps inherited from an earlier joint.
    verify_edge_continuity(objects, known_gaps)
    all_gaps = known_gaps

    # Perforate each tab with two rows of NPTH holes -- one centered on
    # each board's own true edge (lx, rx), so each row straddles that
    # board's edge (half in board material, half in the gap), per the
    # guide's "centerline of the board frame or extending into the board"
    # placement rule applied to both boards.
    bite_tool = ExcellonTool(diameter=BITE_HOLE_DIA_MM, plated=False, unit=MM)
    npth = npth_layer(left)
    hole_offsets = [(i - (BITE_HOLES_PER_TAB - 1) / 2) * BITE_HOLE_PITCH_MM for i in range(BITE_HOLES_PER_TAB)]
    for center in tab_centers:
        for offset in hole_offsets:
            for row_x in (lx, rx):
                npth.objects.append(
                    Flash(x=row_x, y=center + offset, aperture=bite_tool, polarity_dark=True, unit=MM)
                )

    return dx, dy, lx, rx, tab_centers, all_gaps


def panelize_gerbers():
    wing_left = load_board("WingLeft")
    wing_right = load_board("WingRight")
    main_board = load_board("MainBoard")

    # MainBoard is a horizontal rectangle; rotate it 90deg so its long
    # edges are vertical, matching the wings' own facing-edge orientation.
    main_board.rotate(math.radians(90))

    offset1 = bite_join(wing_left, wing_right)                      # WingLeft -- WingRight
    offset2 = bite_join(wing_left, main_board, known_gaps=offset1[5])  # panel -- MainBoard
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

    return offset1, offset2


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


def merge_pnp(offset1, offset2, bom_designators):
    """`bom_designators` is the set of (prefixed) designators present in
    the merged BOM. Pick-and-place naturally also includes things that are
    never in a BOM -- mounting holes, hand-soldered headers, decorative
    silkscreen badges -- and JLCPCB's assembly check hard-errors if a
    placement's designator has no BOM entry ("won't be assembled due to
    data missing"). Rather than hardcoding which devices those are, rows
    get filtered against the BOM's own designator set, which is the actual
    invariant JLCPCB is checking.
    """
    dx1, dy1, *_ = offset1
    dx2, dy2, *_ = offset2

    header, left_rows = load_pnp("WingLeft")
    _, right_rows = load_pnp("WingRight")
    _, main_rows = load_pnp("MainBoard")
    idx = {name: i for i, name in enumerate(header)}
    des_idx, dev_idx = idx["Designator"], idx["Device"]

    all_rows = []
    for row in left_rows:
        row = list(row)
        row[des_idx] = BOARD_PREFIX["WingLeft"] + row[des_idx]
        all_rows.append(row)
    for row in right_rows:
        row = shift_row(row, idx, dx1, dy1)
        row[des_idx] = BOARD_PREFIX["WingRight"] + row[des_idx]
        all_rows.append(row)
    for row in main_rows:
        row = rotate_row(row, idx, math.radians(90))
        row = shift_row(row, idx, dx2, dy2)
        row[des_idx] = BOARD_PREFIX["MainBoard"] + row[des_idx]
        all_rows.append(row)

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


def merge_bom():
    """Merge the three boards' BOMs into one, summing quantities and
    combining designator lists for identical parts instead of listing the
    same part three times. Designators get the same L_/R_/M_ prefix as the
    merged pick-and-place file, so the two stay cross-referenceable.

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
    for name, rows in (("WingLeft", left_rows), ("WingRight", right_rows), ("MainBoard", main_rows)):
        prefix = BOARD_PREFIX[name]
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


def main():
    offset1, offset2 = panelize_gerbers()
    bom_designators = merge_bom()
    merge_pnp(offset1, offset2, bom_designators)


if __name__ == "__main__":
    main()
