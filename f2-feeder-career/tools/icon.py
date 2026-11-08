"""Draws icon.png, the F2 Revamp logo (512 x 512): a dark tile, a bold "F2" in Formula 2 blue and an
upward chevron in F1 red for the move up. Needs Pillow and the Windows Bahnschrift font."""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

S = 4  # drawn at 4x, then scaled down for smooth edges
W = 512 * S
BG, BLUE, RED, WHITE = (14, 20, 33), (0, 144, 208), (225, 6, 0), (240, 244, 248)
FONT = "C:/Windows/Fonts/bahnschrift.ttf"


def font(size, weight):
    f = ImageFont.truetype(FONT, size * S)
    f.set_variation_by_name(weight)
    return f


img = Image.new("RGB", (W, W), BG)
d = ImageDraw.Draw(img)
# Three speed stripes across the bottom, rising to the right.
for i, colour in enumerate((BLUE, WHITE, RED)):
    y0 = (470 - i * 34) * S
    d.polygon([(0, y0), (W, y0 - 120 * S), (W, y0 - 120 * S + 20 * S), (0, y0 + 20 * S)], fill=colour)
# The "F2".
d.text((52 * S, 30 * S), "F2", font=font(240, b"Bold"), fill=WHITE)
# Two chevrons: up to F1.
cx = 420 * S
for k in range(2):
    y = (78 + k * 62) * S
    d.line([(cx - 50 * S, y + 46 * S), (cx, y), (cx + 50 * S, y + 46 * S)], fill=RED, width=24 * S, joint="curve")
# "REVAMP" under it.
d.text((58 * S, 262 * S), "REVAMP", font=font(84, b"Bold Condensed"), fill=BLUE)
out = img.resize((512, 512), Image.LANCZOS)
out.save(Path(__file__).resolve().parent.parent / "icon.png", optimize=True)
