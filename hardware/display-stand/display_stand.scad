// Magnetic desk stand for the LilyGo T-Display S3 AMOLED
// (La Marzocco shot-timer display project)
//
// Sits on top of the espresso machine, held by 4 rod magnets pressed into
// the base, with the board tilted back for easy reading.
//
// Board dimensions measured from LilyGo's own STEP/DXF files
// (github.com/Xinyuan-LilyGO/T-Display-S3-AMOLED/3D_File, /DXF_file):
//   - PCB outline: 25.49 x 58.78 x 1.1 mm, no mounting holes
//   - USB-C + two buttons on one short edge (the "bottom" edge here)
// The board's exact display/bezel size was not in the source files, so
// this holds the board by its edges (open front+back) rather than hugging
// the screen - safe regardless of the exact active area.
//
// Magnets (as supplied): supermagnete.ch S-05-08-N, NdFeB N45,
//   5 mm dia x 8.47 mm, ~1.1 kg each. 4 used here (one per base corner),
//   ~4.4 kg combined pull - comfortably more than the stand+board weigh.

// ---------- Board ----------
board_w       = 25.49;   // short edge (left-right when mounted)
board_l       = 58.78;   // long edge (bottom-to-top)
board_t       = 1.1;

// ---------- Cradle (holds the board by its edges) ----------
slot_clearance  = 0.3;   // extra width so the board slides in without forcing
retain_lip      = 1.4;   // how far the front lip overlaps onto the board face
frame_margin    = 4;     // solid material around the board on 3 sides -
                          // also sets the strut post width (strut_t below)
frame_depth     = 4.5;   // channel depth (front-to-back)
lip_depth       = 1.6;   // thickness of the narrower front-lip section
bottom_extend   = 6;     // side rails continue this far past the board's
                          // USB edge before the strut starts - keeps the
                          // open mouth clear of the USB-C connector/buttons

// ---------- Magnets ----------
magnet_d      = 5.0;
magnet_h      = 8.47;
magnet_hole_d = 5.3;     // press-fit clearance
magnet_hole_h = 8.7;
magnet_floor  = 0.6;     // plastic skin between magnet and machine surface
base_t        = magnet_hole_h + magnet_floor;   // 9.3 mm
magnet_inset  = 7;       // pocket center distance from each base edge

// ---------- Base & stand ----------
base_w       = 54;       // wide enough that the magnet pockets (below)
base_d       = 54;       // clear the strut posts in both x and y
tilt_angle   = 60;       // degrees from horizontal - adjust to taste
strut_len    = 32;       // length of the angled support, base to cradle
strut_foot_y = 22;       // where the strut leaves the base (measured from
                          // the front edge, y=0)
strut_sink   = 6;        // how far the tilt pivot sits below the base's
                          // top face (0 < strut_sink < base_t) - the tilt
                          // means the foot is no longer a flat, level face,
                          // so it has to overlap solid base material rather
                          // than sit exactly on top of it. Checked by eye
                          // against the rendered geometry (see the repo's
                          // hardware/display-stand/README.md for the render)
                          // - if you change tilt_angle/base_t a lot, re-check
                          // that the foot still lands inside 0..base_t.

$fn = 48;

frame_w   = board_w + 2 * frame_margin;
frame_l   = board_l + frame_margin + bottom_extend;
strut_t   = frame_margin;                  // posts sit directly under the
                                            // cradle's side rails
strut_y_span = frame_depth + 2;            // a little deeper than the
                                            // cradle so the two unions cleanly

// Sanity check (visible in the console on every render): the strut posts
// must clear the magnet pockets in X, since the posts get buried into the
// base right above the rear pocket row.
post_outer_x = frame_w/2;
magnet_x     = base_w/2 - magnet_inset;
assert(magnet_x - magnet_hole_d/2 > post_outer_x,
    "magnet pocket overlaps a strut post - widen base_w or reduce magnet_inset");

module board_cradle() {
    // Built flat: open end (board insertion / USB side) at y=0, closed top
    // at y=frame_l, channel depth along z. Rotated upright by the caller.
    // The two cuts below stop at board_l+bottom_extend, NOT frame_l, so the
    // last frame_margin mm are left solid - a closed cap the board's top
    // edge rests against, instead of the channel staying open all the way
    // through (which would just let the board slide out the top too).
    slot_len = board_l + bottom_extend;
    difference() {
        translate([-frame_w/2, 0, 0])
            cube([frame_w, frame_l, frame_depth]);

        // back pocket: full board footprint + clearance, open at the back
        // and the bottom (y), stops lip_depth short of the front face
        translate([-(board_w + slot_clearance)/2, -1, lip_depth])
            cube([board_w + slot_clearance, slot_len + 1, frame_depth]);

        // front window: narrower by retain_lip on each side, so a lip of
        // plastic overlaps the board's front face and holds it in
        translate([-(board_w - 2*retain_lip)/2, -1, -1])
            cube([board_w - 2*retain_lip, slot_len + 1, lip_depth + 1]);
    }
}

module strut() {
    // two vertical posts, local +z, foot at z=0 so they can be placed
    // directly on the base and the cradle stacked directly on top
    for (side = [-1, 1])
        translate([side * (frame_w/2 - strut_t/2), -strut_y_span/2, 0])
            cube([strut_t, strut_y_span, strut_len]);
}

module upper_assembly() {
    // strut + cradle, built already joined (cradle's open/bottom end sits
    // exactly on the strut's top), both still "standing straight up" -
    // the caller rotates this whole union as one rigid body.
    union() {
        strut();
        // overlaps 2mm down into the strut top, same reasoning as
        // strut_overlap above - an exact touch is not a safe union
        translate([0, 0, strut_len - 2])
            rotate([90, 0, 0])
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
    // strut_sink deep in the base - this guarantees a solid, gap-free
    // join regardless of tilt_angle (see the comment on strut_sink above).
    translate([0, strut_foot_y, base_t - strut_sink])
        rotate([-(90 - tilt_angle), 0, 0])
            upper_assembly();
}

stand();
