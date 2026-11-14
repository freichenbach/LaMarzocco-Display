# Magnetic display stand

A 3D-printable stand for the LilyGo T-Display S3 AMOLED display **in its
case**, landscape orientation, tilted back for easy reading and held to the
espresso machine's steel body by 4 magnets - no screws into the case, no
adhesive on the machine.

![Stand preview](preview.png)

## What you need

- **4x rod magnets**, 5 mm diameter x 8.47 mm tall, NdFeB N45 (as used when
  this was designed: [supermagnete.ch S-05-08-N](https://www.supermagnete.ch),
  ~1.1 kg pull each - roughly 4.4 kg combined, far more than the stand and
  case weigh). A different magnet size works too - edit `magnet_d` and
  `magnet_h` in `display_stand.scad` and re-render.
- A 3D printer, PLA or PETG. Print it as exported (base down, matching how
  it's used) - no supports needed. The frame sits on a solid wedge, not a
  thin wall: its cross-section only ever grows as it's printed up from the
  base, so there's no overhang at any height for supports to catch. (An
  earlier version of this design used a thin tilted panel instead - it
  printed, with supports, as a mess of detached, drifted plastic twice in a
  row before being replaced with the wedge. If your slicer still wants to
  add supports under the wedge, it doesn't need them; turning them off is
  safe.)

The wedge spans the full width of the frame's bottom rail (no overhang on
either side) and its slope starts right at the base's own front edge
rather than partway in, so the connection between frame and base is solid
and continuous everywhere, with nothing left unsupported behind it.
`strut_len` (how far it rises before the frame begins) is computed, not
hand-picked, as the shortest rise that still keeps the frame's lowest
corner from dipping below the base. Combined with a base sized to just
clear the magnets and the tilted frame's own footprint, this keeps the
model's overall height and footprint - and so its print time - as small as
the design allows.

If you only look at `preview.png` or OpenSCAD's own live preview, the
wedge can be easy to miss or look disconnected - OpenSCAD's fast preview
mode often renders a `hull()` shape (what the wedge is built from)
washed-out or near-invisible. Press F6 (or pass `--render` on the command
line, as `preview.png` is) for the real, fully-computed geometry before
judging whether something looks off.

The front wedge only holds up the frame's bottom edge, though - tilted
back, the frame's top leans further back and higher still, with nothing
underneath it but its own rigidity. A second wedge (`back_wall()` in
`display_stand.scad`) braces it from the base up to the frame's top, so
the frame is held at both ends rather than cantilevered from the front
alone. Its foot is computed, not placed by eye, from two constraints
that both come from a real test print: the wall's own front face must
lean backward the same way the front wedge does, not jut forward as it
rises (that's the "flying support" / no-bed-contact warning a slicer
gives for a part it can't print without extra support) - and it must
only ever touch the frame's own back edge (`back_wall_attach_w`, a
couple mm, matching the slack already built into `frame_depth` for
this), never reaching across the open channel where the case itself
sits, or it blocks the case from seating at all (also found by printing
it and trying to fit the case in).

**Is there any overhang left at all?** The front wedge and back wall are
both confirmed fully self-supporting (checked by slicing the rendered
model at many heights and verifying neither one's leading edge ever
moves forward as it rises). There is one small, separate spot that isn't:
the retaining lip's own step - solid material only in a narrow band near
the rails, open channel everywhere else - is an unavoidable local
overhang once the whole frame tilts back, confirmed by rendering the
cradle alone, tilted, with no wedge or base at all. It totals under
~250mm² in a few small patches at the closed-cap end - short spans, not
a broad flat shelf, well within what FDM printers bridge without support
and nothing like the tall thin panel this design's own support structure
replaced earlier. An attempt to "fix" it by narrowing the wedges under
the closed cap made things worse (it left that end's own bottom edge
unsupported) and was reverted; it's listed here rather than hidden so
you can judge for yourself or paint supports over just that spot if you'd
rather not rely on the bridge.

**Open end still needed slicer supports in practice**, even though the
model itself has no real overhang there - the open end (no solid cap to
help brace it, unlike the closed end) apparently flexes enough under
print vibration for the slicer to want to prop it up over the whole
height between where the front wedge and back wall reach. `corner_brace()`
adds a dedicated, narrow (1 cm) extra connection from the base at that
one corner instead of relying on supports: a low part repeating
support_wedge()'s own safe attachment to the frame's bottom, and a high
part built exactly like back_wall() (narrow back-edge attachment,
computed self-supporting foot) but reaching only partway up
(`corner_brace_h`) rather than all the way to the top. It can't reach any
higher from a front-anchored foot without cutting across the case (the
case's own back edge rises in Y faster than a front-anchored hull ever
can, checked the same way as back_wall()'s own clearance) - and it can't
be back-anchored like back_wall() over this whole height either, since a
back-edge attachment is only ever exactly flush with the case's own back
surface at its own target height (by construction, matching
`back_wall_attach_w` to the slack in `frame_depth`) - going any higher
than `corner_brace_h` would just move that already-zero margin to a
taller point, not create one.

## How it holds the case

The case (58 x 35 x 10 mm, measured by hand - the height was corrected
from an initial 30 mm that turned out too short for the real case to
seat) slides in from the right-hand
short edge - the same edge the USB-C cable exits - into a channel that
grips it by its two long edges and a closed cap on the left, with a ~1.6 mm
lip on the front face holding it in. Back and the right end stay open, so
the cable routes out sideways and nothing covers the screen.

This grips the case's outer envelope rather than fitting any internal
detail, since the case's own shape beyond its bounding box wasn't known
when this was built - works as long as the case is close to a plain
rectangular box. If your case has a different shape (rounded corners,
a lip, a stepped profile), check the fit before printing.

**Cable on the left instead?** Set `cable_exit_right = false;` near the top
of `display_stand.scad` and re-render - it mirrors the opening to the other
side.

## Dimensions and their source

Case outline: 58 x 35 x 10 mm, measured by hand - not from a CAD file, so
there's no sub-mm precision to rely on. `slot_clearance` (0.4 mm) and
`retain_lip` (1.6 mm) in `display_stand.scad` give it a bit of room; if
your case is a little larger or smaller, adjust `case_l`/`case_h`/`case_t`
at the top of the file and re-render.

## Customizing

Everything lives in named variables at the top of `display_stand.scad` -
`tilt_angle` (default 60°, measured from horizontal), `case_l`/`case_h`/
`case_t`, `cable_exit_right`, `base_w`/`base_d`, `magnet_d`/`magnet_h` for
different magnets, etc. Re-render after changes:

```bash
openscad --render -o display_stand.stl display_stand.scad
```

A few `assert()`s near the top check that the magnet pockets don't overlap
the wedge's foot if you change the base size, wedge position, or magnet
spacing - they'll fail loudly with an explanation rather than silently
producing a broken or unprintable part.

## Printing & assembly

1. Print `display_stand.stl` (PLA/PETG, 20%+ infill is plenty - it's not
   load-bearing beyond the case's own weight).
2. Press the 4 magnets into the pockets on the underside of the base
   (friction fit, ~5.3 mm holes for 5 mm magnets). Mind polarity isn't
   relevant here since there's nothing to match it against, just press
   them in flush. Each pocket has a 2 mm printed floor - the magnets don't
   touch the machine directly, so the finish doesn't get scratched and the
   pull is a bit gentler than bare-metal contact (still comfortably enough
   to hold this stand).
3. Slide the cased display in from the open (cable) side until it seats
   against the closed cap.
4. Set the stand on the machine's steel body wherever you can read it.
