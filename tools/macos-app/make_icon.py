#!/usr/bin/env python3
"""Draw the OpenTS.app icon.

A C&C Tiberian Sun flavoured mark rather than a generic "game" glyph: the
oblique chrome-ish chevron over a dark field, with the series' signature
amber. Drawn at every size iconutil wants, from one supersampled master, so
the small sizes stay legible instead of turning to mush.

    make_icon.py <output.icns>
"""

import os
import struct
import subprocess
import sys
import tempfile

from PIL import Image, ImageDraw, ImageFilter

# The sizes iconutil's iconset requires, and the factor each is drawn at.
ICON_SIZES = [
    (16, 2), (32, 2), (128, 2), (256, 2), (512, 2),
]

# Drawn once at 1024 and downsampled, so edges are antialiased before they
# are small rather than after.
MASTER = 1024

BACKGROUND_TOP = (26, 30, 38)
BACKGROUND_BOTTOM = (10, 11, 15)
AMBER = (255, 176, 32)
AMBER_DIM = (206, 132, 18)
STEEL = (150, 158, 172)


def rounded_mask(size, radius):
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, size - 1, size - 1], radius=radius, fill=255)
    return mask


def draw_master():
    image = Image.new("RGBA", (MASTER, MASTER), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)

    # Background: a soft vertical gradient, the way the game's menu chrome is lit.
    for y in range(MASTER):
        t = y / (MASTER - 1)
        colour = tuple(
            int(BACKGROUND_TOP[i] + (BACKGROUND_BOTTOM[i] - BACKGROUND_TOP[i]) * t)
            for i in range(3)
        )
        draw.line([(0, y), (MASTER, y)], fill=colour + (255,))

    # A faint horizon line, the way the sky sits above the ground in the game's art.
    draw.rectangle([0, int(MASTER * 0.66), MASTER, MASTER], fill=(6, 7, 10, 255))
    draw.line([(0, int(MASTER * 0.66)), (MASTER, int(MASTER * 0.66))], fill=(48, 54, 66, 255))

    # The chevron: two strokes meeting at a point, drawn as polygons so the
    # mitre stays sharp at every size instead of blurring into a blob.
    apex = (MASTER * 0.5, MASTER * 0.20)
    left = (MASTER * 0.17, MASTER * 0.56)
    right = (MASTER * 0.83, MASTER * 0.56)
    thickness = MASTER * 0.115

    def stroke(p_from, p_to):
        # A quad from p_from to p_to, offset perpendicular by the stroke width.
        dx, dy = p_to[0] - p_from[0], p_to[1] - p_from[1]
        length = (dx * dx + dy * dy) ** 0.5
        nx, ny = -dy / length * thickness / 2, dx / length * thickness / 2
        return [
            (p_from[0] + nx, p_from[1] + ny),
            (p_to[0] + nx, p_to[1] + ny),
            (p_to[0] - nx, p_to[1] - ny),
            (p_from[0] - nx, p_from[1] - ny),
        ]

    draw.polygon(stroke(left, apex), fill=AMBER + (255,))
    draw.polygon(stroke(right, apex), fill=AMBER_DIM + (255,))

    # Steel underline beneath the mark, tying the two strokes together.
    draw.rectangle(
        [MASTER * 0.235, MASTER * 0.70, MASTER * 0.765, MASTER * 0.70 + thickness * 0.40],
        fill=STEEL + (170,),
    )

    # A soft glow under the apex, which is what makes it read as "lit" rather
    # than "drawn" at small sizes.
    glow = Image.new("RGBA", (MASTER, MASTER), (0, 0, 0, 0))
    ImageDraw.Draw(glow).ellipse(
        [MASTER * 0.30, MASTER * 0.12, MASTER * 0.70, MASTER * 0.52],
        fill=AMBER + (70,),
    )
    image = Image.alpha_composite(image, glow.filter(ImageFilter.GaussianBlur(MASTER * 0.055)))

    # macOS rounded-rectangle silhouette, with the shadow the icon needs to
    # sit properly in the Dock and the Finder.
    mask = rounded_mask(MASTER, int(MASTER * 0.2237))
    image.putalpha(mask)

    shadow = Image.new("RGBA", (MASTER, MASTER), (0, 0, 0, 0))
    shadow.putalpha(mask.filter(ImageFilter.GaussianBlur(MASTER * 0.03)))
    shadow = Image.new("RGBA", (MASTER, MASTER), (0, 0, 0, 0))
    shadow_mask = mask.filter(ImageFilter.GaussianBlur(MASTER * 0.028))
    shadow.paste((0, 0, 0, 150), (0, 0), shadow_mask)

    canvas = Image.new("RGBA", (MASTER, MASTER), (0, 0, 0, 0))
    canvas = Image.alpha_composite(canvas, shadow)
    canvas.paste(image, (0, 0), image)
    return canvas


def main():
    if len(sys.argv) != 2:
        print(f"usage: {os.path.basename(sys.argv[0])} <output.icns>", file=sys.stderr)
        return 1

    output = sys.argv[1]
    master = draw_master()

    with tempfile.TemporaryDirectory() as work:
        iconset = os.path.join(work, "OpenTS.iconset")
        os.makedirs(iconset)

        for size, scale in ICON_SIZES:
            resized = master.resize((size, size), Image.LANCZOS)
            resized.save(os.path.join(iconset, f"icon_{size}x{size}.png"))
            if scale != 1:
                resized.save(
                    os.path.join(iconset, f"icon_{size * scale}x{size * scale}@2x.png")
                )

        # 1024 is the one size with no 1x entry; its @2x is the 512@2x above.
        master.resize((1024, 1024), Image.LANCZOS).save(
            os.path.join(iconset, "icon_512x512@2x.png")
        )

        subprocess.run(
            ["iconutil", "-c", "icns", iconset, "-o", output],
            check=True,
        )

    print(f"wrote {output} ({os.path.getsize(output)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
