# Magnetic display stand

A 3D-printable stand for the LilyGo T-Display S3 AMOLED board (this
project's display), tilted back for easy reading and held to the espresso
machine's steel body by 4 magnets - no screws into the board, no adhesive
on the machine.

![Stand preview](preview.png)

## What you need

- **4x rod magnets**, 5 mm diameter x 8.47 mm tall, NdFeB N45 (as used when
  this was designed: [supermagnete.ch S-05-08-N](https://www.supermagnete.ch),
  ~1.1 kg pull each - roughly 4.4 kg combined, far more than the stand and
  board weigh). A different magnet size works too - edit `magnet_d` and
  `magnet_h` in `display_stand.scad` and re-render.
- A 3D printer, PLA or PETG. No supports needed if you orient the part with
  the cradle's flat back face down on the bed (rotate it in your slicer -
  the file itself is exported standing up, matching how it's used).

## How it holds the board

The board slides in from the bottom (same edge as the USB-C port and the
two buttons) into a channel that grips it by the edges on three sides -
left, right, and a closed cap at the top - with a ~1.4 mm lip on the front
face holding it in. Back and bottom stay open, so the USB-C cable routes
out behind the stand and nothing covers the screen. The LilyGo board has no
mounting holes, which is why this grips the edges instead of screwing in.

## Dimensions and their source

Board outline (25.49 x 58.78 x 1.1 mm) and the USB-C/button layout came
from LilyGo's own STEP and DXF files
([github.com/Xinyuan-LilyGO/T-Display-S3-AMOLED](https://github.com/Xinyuan-LilyGO/T-Display-S3-AMOLED/tree/main/3D_File)),
not from lilygo.cc (unreachable from the environment this was built in).
Corner radii are approximate (read off the STEP geometry, the DXF disagrees
by a few tenths of a mm). The display's exact active area and bezel
weren't in either source - the cradle is built to hold the board by its
edges rather than frame the screen tightly, specifically so that gap
doesn't matter.

**Check your board against these numbers before printing** if anything
looks off - this was verified against the published CAD files, not a
physical board.

## Customizing

Everything lives in named variables at the top of `display_stand.scad` -
`tilt_angle` (default 60°, measured from horizontal), `base_w`/`base_d`,
`magnet_d`/`magnet_h` for different magnets, etc. Re-render after changes:

```bash
openscad --render -o display_stand.stl display_stand.scad
```

A `assert()` near the top checks that the magnet pockets don't overlap the
strut posts if you change the base size or magnet spacing - it'll fail
loudly with an explanation rather than silently producing a broken part.

## Printing & assembly

1. Print `display_stand.stl` (PLA/PETG, 20%+ infill is plenty - it's not
   load-bearing beyond the board's own weight).
2. Press the 4 magnets into the pockets on the underside of the base
   (friction fit, ~5.3 mm holes for 5 mm magnets). Mind polarity isn't
   relevant here since there's nothing to match it against, just press
   them in flush.
3. Slide the display board in from the bottom until it seats against the
   closed top.
4. Set the stand on the machine's steel body wherever you can read it.
