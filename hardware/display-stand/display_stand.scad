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

cable_exit_right = true;  // false swaps the open end to the left

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
magnet_floor  = 0.6;     // plastic skin between magnet and machine surface
base_t        = magnet_hole_h + magnet_floor;   // 9.3 mm
magnet_inset  = 7;       // pocket center distance from each base edge

// ---------- Base & stand ----------
base_w       = 74;       // wide enough for the now-horizontal frame
base_d       = 50;
tilt_angle   = 60;       // degrees from horizontal - adjust to taste
strut_len    = 30;       // length of the angled support, base to cradle
strut_w      = 24;       // width of the (single, centered) support pedestal
strut_t      = 6;        // its thickness (front-to-back)
strut_foot_y = 20;       // where the strut leaves the base, measured from
                          // the front edge (y=0)
strut_sink   = 6;        // how far the tilt pivot sits below the base's
                          // top face - see the matching comment in the
                          // portrait version this was adapted from

$fn = 48;

frame_h   = case_h + 2 * frame_margin;
frame_len = case_l + frame_margin;        // + frame_margin = the closed cap;
                                           // the open end needs no extra
                                           // margin, it's open

// Sanity check (visible in the console on every render): the magnet
// pockets must clear the strut pedestal in Y, since the pedestal is buried
// into the base right between/near the front and back pocket rows.
pedestal_front_y = strut_foot_y - strut_t/2;
pedestal_back_y  = strut_foot_y + strut_t/2;
assert(pedestal_front_y > magnet_inset + magnet_hole_d/2,
    "strut pedestal overlaps the front magnet row - move strut_foot_y back or shrink strut_t");
assert(pedestal_back_y < base_d - magnet_inset - magnet_hole_d/2,
    "strut pedestal overlaps the back magnet row - move strut_foot_y forward or shrink strut_t");

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

module strut() {
    // single centered pedestal, local +z, foot at z=0
    translate([-strut_w/2, -strut_t/2, 0])
        cube([strut_w, strut_t, strut_len]);
}

module upper_assembly() {
    union() {
        strut();
        // cradle's bottom rail (local z=-frame_h/2) lands on the
        // pedestal's top (z=strut_len)
        translate([0, 0, strut_len + frame_h/2])
            board_cradle();
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

    // Tilt the whole upper assembly backward about a point buried
    // strut_sink deep in the base - guarantees a solid, gap-free join
    // regardless of tilt_angle (the pedestal foot is no longer a flat,
    // level face once rotated, so it has to overlap solid base material
    // rather than sit exactly on top of it).
    translate([0, strut_foot_y, base_t - strut_sink])
        rotate([-(90 - tilt_angle), 0, 0])
            upper_assembly();
}

stand();
