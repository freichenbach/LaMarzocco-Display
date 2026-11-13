// Magnetic desk stand for the LilyGo T-Display S3 AMOLED, IN ITS CASE
// (La Marzocco shot-timer display project)
//
// Landscape orientation: the case's long edge runs horizontal, tilted back
// for reading, sitting on top of the espresso machine held by 4 rod
// magnets pressed into the base. The USB-C cable exits a short (now
// vertical) edge, left or right - see cable_exit_right below.
//
// Case dimensions as measured by hand: 58 x 30 x 10 mm (length x height x
// thickness). This holds the case by its outer envelope - the two long
// edges gripped by rails, a closed cap on one short edge, open on the
// other short edge for the cable and for sliding the case in. No
// assumptions are made about the case's exact shape beyond its bounding
// box, so it's robust to not knowing the case's own geometry in detail -
// but DOES assume it's close to a plain rectangular box. Check the fit
// against your actual case before printing if it has, say, a rounded or
// stepped profile.

// ---------- Case (outer envelope, not the bare board) ----------
case_l = 58;    // length - horizontal when mounted
case_h = 30;    // height - vertical when mounted (rail-to-rail span)
case_t = 10;    // thickness (front-to-back)

cable_exit_right = false;  // false swaps the open end to the left

// ---------- Cradle (holds the case by its edges) ----------
slot_clearance  = 0.4;   // extra depth/height so the case slides in freely
retain_lip      = 1.6;   // how far the front lip overlaps onto the case
frame_margin    = 4;     // solid rail/cap material around the case
frame_depth     = case_t + slot_clearance + 2;  // channel depth (front-to-back)
lip_depth       = 2;     // thickness of the narrower front-lip section

// ---------- Magnets ----------
magnet_d      = 5.0;
magnet_h      = 8.47;
magnet_hole_d = 5.3;     // press-fit clearance
magnet_hole_h = 8.7;
// A thin (sub-1mm) floor is also an unreliable print - the first few
// layers over a pocket that size can come out pinholed or uneven. 2mm
// prints solidly and keeps the magnet from directly touching (and
// scratching) the machine, at the cost of a bit of pull - still far more
// than needed for this stand's weight.
magnet_floor  = 2;       // plastic skin between magnet and machine surface
base_t        = magnet_hole_h + magnet_floor;   // 10.7 mm
magnet_inset  = 7;       // pocket center distance from each base edge

// ---------- Base & stand ----------
base_w       = 80;       // wide enough for the now-horizontal frame, with
                          // room either side for the magnets (see the
                          // assert below)
base_d       = 50;
tilt_angle   = 60;       // degrees from horizontal - adjust to taste
strut_len    = 30;       // height of the wedge, base to the frame's bottom
// The wedge holds up the frame's bottom rail, which runs horizontally -
// unlike a portrait layout, support and rail are NOT inline here, so a
// narrow post (or a thin wall, tried and printed twice, both times
// separating partway up - too little bonding area to resist the printer's
// own vibration) would leave both ends of the rail cantilevered in mid-air
// (~19 mm unsupported on each side at frame_len=62, strut_w=24 - well past
// what FDM bridges reliably). strut_w spans almost the full frame width,
// and support_wedge() below fills in a solid ramp underneath instead of a
// panel, so there's no overhang anywhere to sag, snap loose, or need
// print supports for.
strut_w      = 54;       // width of the wedge (leaves 4 mm clear on each
                          // side of the frame, see the assert below)
strut_t      = 8;        // depth of the wedge's foot, where it meets the
                          // base (front-to-back)
strut_foot_y = 20;       // where the wedge's foot sits on the base,
                          // measured from the front edge (y=0)
strut_sink   = 6;        // how far the tilt pivot sits below the base's
                          // top face - keeps the frame's own join to the
                          // wedge solid and gap-free regardless of
                          // tilt_angle, same reasoning as support_wedge()

$fn = 48;

frame_h   = case_h + 2 * frame_margin;
frame_len = case_l + frame_margin;        // + frame_margin = the closed cap;
                                           // the open end needs no extra
                                           // margin, it's open

// Sanity checks (visible in the console on every render): the wedge must
// reach close enough to the frame's own width to avoid a long unsupported
// overhang on the rail (print-only concern - OpenSCAD can't tell you a
// shape is hard to print, so this just catches a config that reintroduces
// the original mistake), and the magnet pockets must clear the wedge's
// foot in both X and Y.
max_rail_overhang = 8;   // mm of unsupported rail on each side considered
                          // safe to print without support
assert(frame_len/2 - strut_w/2 <= max_rail_overhang,
    "strut_w too narrow for frame_len - the bottom rail would overhang more than max_rail_overhang unsupported on each side");

wedge_foot_front_y = strut_foot_y - strut_t/2;
wedge_foot_back_y  = strut_foot_y + strut_t/2;
assert(wedge_foot_front_y > magnet_inset + magnet_hole_d/2,
    "wedge foot overlaps the front magnet row in Y - move strut_foot_y back or shrink strut_t");
assert(wedge_foot_back_y < base_d - magnet_inset - magnet_hole_d/2,
    "wedge foot overlaps the back magnet row in Y - move strut_foot_y forward or shrink strut_t");
assert(base_w/2 - magnet_inset - magnet_hole_d/2 > strut_w/2,
    "wedge foot overlaps a magnet pocket in X - widen base_w, shrink strut_w, or increase magnet_inset");

module board_cradle_right() {
    // Built directly in final mounted orientation (pre-tilt): x =
    // horizontal, open end at +x, closed cap at -x; z = vertical (rail
    // span); y = depth (channel). Centered on x=0 and z=0 so it drops
    // straight onto the strut. mirror()'d at the call site for the other
    // cable side - see cable_exit_right.
    cap_x = -frame_len/2 + frame_margin;   // where the closed cap ends and
                                            // the open channel begins
    difference() {
        translate([-frame_len/2, 0, -frame_h/2])
            cube([frame_len, frame_depth, frame_h]);

        // back pocket: open at the back and the +x end (overshoots past
        // the frame on purpose), stops lip_depth short of the front face
        translate([cap_x, lip_depth, -(case_h + slot_clearance)/2])
            cube([frame_len, frame_depth, case_h + slot_clearance]);

        // front window: narrower by retain_lip on each side, so a lip of
        // plastic overlaps the case's front face and holds it in
        translate([cap_x, -1, -(case_h - 2*retain_lip)/2])
            cube([frame_len, lip_depth + 1, case_h - 2*retain_lip]);
    }
}

module board_cradle() {
    if (cable_exit_right)
        board_cradle_right();
    else
        mirror([1, 0, 0]) board_cradle_right();
}

// A thin, tall, tilted, unsupported wall (the original design here) turned
// out to be too weak to print reliably even with slicer supports - it
// doesn't have enough bonding area low down to resist the printer's own
// vibration, and once it flexes even a little the layers above drift
// further and further (two failed prints, both separating partway up).
//
// A solid wedge replaces it: one continuous ramp of material from the
// base up to the frame, with no overhang at any height, since its own
// cross-section only ever grows as it's built up - there's nothing to
// sag, snap loose, or need print supports for, and it is mechanically
// far stronger than a thin panel ever was.
module frame_bottom_proxy() {
    // A thin slice standing in for the frame's own bottom-rail footprint,
    // used only to tell hull() where the wedge needs to reach - not part
    // of the visible model (board_cradle() provides the real geometry).
    translate([-strut_w/2, 0, 0])
        cube([strut_w, frame_depth, 1]);
}

module support_wedge() {
    hull() {
        // foot, flat on the base
        translate([-strut_w/2, strut_foot_y - strut_t/2, 0])
            cube([strut_w, strut_t, 1]);
        // up to the frame's own bottom edge, through the same tilt used
        // to place the frame itself (see stand() below) - kept identical
        // on purpose, so the wedge's top and the frame's bottom coincide
        translate([0, strut_foot_y, base_t - strut_sink])
            rotate([-(90 - tilt_angle), 0, 0])
                translate([0, 0, strut_len])
                    frame_bottom_proxy();
    }
}

module magnet_pockets() {
    xs = [-(base_w/2 - magnet_inset), (base_w/2 - magnet_inset)];
    ys = [magnet_inset, base_d - magnet_inset];
    for (x = xs)
        for (y = ys)
            translate([x, y, base_t - magnet_hole_h])
                cylinder(d = magnet_hole_d, h = magnet_hole_h + 0.5);
}

module base() {
    difference() {
        translate([-base_w/2, 0, 0])
            cube([base_w, base_d, base_t]);
        magnet_pockets();
    }
}

module stand() {
    base();
    support_wedge();

    // Tilt the frame backward about a point buried strut_sink deep in the
    // base - guarantees a solid, gap-free join with the wedge regardless
    // of tilt_angle (same reasoning as support_wedge()'s own hull target:
    // the frame's bottom is no longer a flat, level face once rotated).
    translate([0, strut_foot_y, base_t - strut_sink])
        rotate([-(90 - tilt_angle), 0, 0])
            translate([0, 0, strut_len + frame_h/2])
                board_cradle();
}

stand();
