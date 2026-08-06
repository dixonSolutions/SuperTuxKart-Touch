#!/usr/bin/env python3
"""Generate the glass touch HUD textures for SuperTuxTouch.

Every plate shares one geometry and one glass material; only the accent hue and
the state (base / pressed / fill) change. Keeping the art generated rather than
hand-drawn is what makes the cluster stay visually consistent as buttons are
added or retinted -- see docs/TOUCH_UI_DESIGN.md section 4.

Usage:  python3 scripts/gen-touch-textures.py [output_dir]
Default output dir: engine/data/gui/icons/android
"""

from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

# Supersampling factor; all drawing happens at SS times the final size and is
# downscaled with Lanczos, which is cheaper than writing an antialiased
# rasteriser and gives clean circular edges at any output resolution.
SS = 4

# --- Design tokens (fractions of the plate size) --------------------------
PAD = 0.035          # gap between the art and the texture edge
RING_WIDTH = 0.055   # accent ring thickness
GLASS_RGB = (12, 18, 28)
GLASS_ALPHA = 92
RING_ALPHA = 210
SPECULAR_ALPHA = 46

ACCENTS = {
    "btn":    (190, 214, 235),   # neutral plate, also used for pause
    "item":   (53, 200, 255),    # cyan   - powerup / fire
    "nitro":  (92, 232, 106),    # green  - nitro
    "drift":  (255, 176, 32),    # amber  - drift, the cluster primary
    "look":   (159, 179, 200),   # slate  - look back, passive
    "rescue": (255, 107, 91),    # red    - rescue, costly action
}


def _blank(size: int) -> Image.Image:
    return Image.new("RGBA", (size, size), (0, 0, 0, 0))


def _add_specular(img: Image.Image, size: int, radius: float, strength: int) -> None:
    """Single top-left highlight, shared by every plate so they read as one material."""
    spec = _blank(size)
    d = ImageDraw.Draw(spec)
    cx = cy = size / 2
    # A crescent: a bright disc with a slightly offset disc punched out of it.
    d.ellipse(
        [cx - radius, cy - radius, cx + radius, cy + radius],
        fill=(255, 255, 255, strength),
    )
    inner = radius * 0.88
    d.ellipse(
        [
            cx - inner + radius * 0.18,
            cy - inner + radius * 0.22,
            cx + inner + radius * 0.18,
            cy + inner + radius * 0.22,
        ],
        fill=(0, 0, 0, 0),
    )
    spec = spec.filter(ImageFilter.GaussianBlur(radius * 0.10))
    img.alpha_composite(spec)


def make_plate(size: int, accent: tuple[int, int, int], *,
               fill_alpha: int = GLASS_ALPHA,
               ring_alpha: int = RING_ALPHA,
               tint: float = 0.14) -> Image.Image:
    """A circular glass plate with an accent ring."""
    s = size * SS
    img = _blank(s)
    d = ImageDraw.Draw(img)

    pad = PAD * s
    outer = s / 2 - pad
    cx = cy = s / 2

    # Glass body: dark base pulled slightly towards the accent so the fill and
    # the ring belong to the same colour family.
    body = tuple(
        int(GLASS_RGB[i] * (1 - tint) + accent[i] * tint) for i in range(3)
    )
    d.ellipse(
        [cx - outer, cy - outer, cx + outer, cy + outer],
        fill=body + (fill_alpha,),
    )

    ring_w = RING_WIDTH * s
    d.ellipse(
        [
            cx - outer + ring_w / 2,
            cy - outer + ring_w / 2,
            cx + outer - ring_w / 2,
            cy + outer - ring_w / 2,
        ],
        outline=accent + (ring_alpha,),
        width=int(ring_w),
    )

    _add_specular(img, s, outer * 0.94, SPECULAR_ALPHA)
    return img.resize((size, size), Image.LANCZOS)


def make_focus(size: int) -> Image.Image:
    """Pressed-state overlay, drawn on top of a plate.

    It is hue-neutral and sits strictly *inside* the accent ring: pressing
    brightens a button but must never hide which button it is.
    """
    s = size * SS
    img = _blank(s)
    d = ImageDraw.Draw(img)
    pad = PAD * s
    outer = s / 2 - pad - RING_WIDTH * s
    cx = cy = s / 2

    d.ellipse(
        [cx - outer, cy - outer, cx + outer, cy + outer],
        fill=(255, 255, 255, 60),
    )
    ring_w = 0.032 * s
    d.ellipse(
        [
            cx - outer + ring_w / 2,
            cy - outer + ring_w / 2,
            cx + outer - ring_w / 2,
            cy + outer - ring_w / 2,
        ],
        outline=(255, 255, 255, 200),
        width=int(ring_w),
    )
    return img.resize((size, size), Image.LANCZOS)


def make_stick_base(size: int) -> Image.Image:
    """Steering stick base: a wide, low-contrast well so the track reads through it."""
    s = size * SS
    img = _blank(s)
    d = ImageDraw.Draw(img)
    pad = PAD * s
    outer = s / 2 - pad
    cx = cy = s / 2

    d.ellipse(
        [cx - outer, cy - outer, cx + outer, cy + outer],
        fill=GLASS_RGB + (64,),
    )
    ring_w = 0.028 * s
    d.ellipse(
        [
            cx - outer + ring_w / 2,
            cy - outer + ring_w / 2,
            cx + outer - ring_w / 2,
            cy + outer - ring_w / 2,
        ],
        outline=(226, 236, 245, 170),
        width=int(ring_w),
    )
    # Inner guide ring marks the point where steering reaches full lock. The
    # base gets no specular highlight: it is a recessed well, not a raised
    # plate, and a highlight here fights with the knob sitting on top of it.
    guide = outer * 0.55
    d.ellipse(
        [cx - guide, cy - guide, cx + guide, cy + guide],
        outline=(226, 236, 245, 52),
        width=int(0.012 * s),
    )
    return img.resize((size, size), Image.LANCZOS)


def make_stick_knob(size: int) -> Image.Image:
    s = size * SS
    img = _blank(s)
    d = ImageDraw.Draw(img)
    pad = PAD * s
    outer = s / 2 - pad
    cx = cy = s / 2

    d.ellipse(
        [cx - outer, cy - outer, cx + outer, cy + outer],
        fill=(232, 240, 248, 150),
    )
    ring_w = 0.05 * s
    d.ellipse(
        [
            cx - outer + ring_w / 2,
            cy - outer + ring_w / 2,
            cx + outer - ring_w / 2,
            cy + outer - ring_w / 2,
        ],
        outline=(255, 255, 255, 235),
        width=int(ring_w),
    )
    _add_specular(img, s, outer * 0.86, 40)
    return img.resize((size, size), Image.LANCZOS)


def make_stick_arc(size: int, accent: tuple[int, int, int], upper: bool) -> Image.Image:
    """Accelerate / brake indicator drawn over the stick base.

    Upper arc = accelerating, lower arc = braking. Gives the stick's vertical
    axis a visible meaning, which auto-acceleration otherwise hides completely.
    """
    s = size * SS
    img = _blank(s)
    d = ImageDraw.Draw(img)
    pad = PAD * s
    ring_w = 0.055 * s
    outer = s / 2 - pad - ring_w / 2
    cx = cy = s / 2
    # PIL angles run clockwise from 3 o'clock, so the screen's top is 180..360.
    start, end = (203, 337) if upper else (23, 157)
    d.arc(
        [cx - outer, cy - outer, cx + outer, cy + outer],
        start=start,
        end=end,
        fill=accent + (255,),
        width=int(ring_w),
    )
    img = img.filter(ImageFilter.GaussianBlur(s * 0.004))
    return img.resize((size, size), Image.LANCZOS)


def main() -> int:
    root = Path(__file__).resolve().parent.parent
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else \
        root / "engine/data/gui/icons/android"
    out.mkdir(parents=True, exist_ok=True)

    written = []

    def save(name: str, img: Image.Image) -> None:
        path = out / name
        img.save(path)
        written.append(f"{name} {img.width}x{img.height}")

    plate = 256
    save("glass_btn.png", make_plate(plate, ACCENTS["btn"]))
    save("glass_btn_focus.png", make_focus(plate))
    save("glass_btn_item.png", make_plate(plate, ACCENTS["item"]))
    save("glass_btn_drift.png", make_plate(plate, ACCENTS["drift"]))
    save("glass_btn_look.png", make_plate(plate, ACCENTS["look"]))
    save("glass_btn_rescue.png", make_plate(plate, ACCENTS["rescue"]))
    save("glass_btn_nitro.png", make_plate(plate, ACCENTS["nitro"]))
    # Bright twin of the nitro plate; the HUD clips it bottom-up to show the
    # remaining nitro as a rising level inside the button.
    save(
        "glass_btn_nitro_fill.png",
        make_plate(plate, ACCENTS["nitro"], fill_alpha=190, ring_alpha=255, tint=0.5),
    )

    save("glass_stick_base.png", make_stick_base(512))
    save("glass_stick_knob.png", make_stick_knob(256))
    save("glass_stick_accel.png", make_stick_arc(512, ACCENTS["nitro"], upper=True))
    save("glass_stick_brake.png", make_stick_arc(512, ACCENTS["rescue"], upper=False))

    print(f"Wrote {len(written)} textures to {out}")
    for line in written:
        print("  " + line)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
