#!/usr/bin/env python3
"""Generate the multi-resolution Windows icon from the PressureOS visual language."""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "deploy" / "windows" / "PressureOS.ico"
SIZE = 1024


def rounded_mask(size: int, radius: int) -> Image.Image:
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, size - 1, size - 1), radius=radius, fill=255)
    return mask


def render_icon() -> Image.Image:
    image = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))

    shadow = Image.new("RGBA", image.size, (0, 0, 0, 0))
    shadow_draw = ImageDraw.Draw(shadow)
    shadow_draw.rounded_rectangle((89, 105, 935, 951), radius=220, fill=(5, 48, 108, 115))
    shadow = shadow.filter(ImageFilter.GaussianBlur(38))
    image.alpha_composite(shadow)

    background = Image.new("RGBA", (840, 840), (0, 0, 0, 0))
    pixels = background.load()
    for y in range(840):
        for x in range(840):
            t = min(1.0, max(0.0, (x * 0.35 + y * 0.65) / 839.0))
            r = round(68 * (1 - t) + 7 * t)
            g = round(183 * (1 - t) + 89 * t)
            b = round(255 * (1 - t) + 200 * t)
            pixels[x, y] = (r, g, b, 255)
    background.putalpha(rounded_mask(840, 220))
    image.alpha_composite(background, (92, 82))

    gloss = Image.new("RGBA", image.size, (0, 0, 0, 0))
    gloss_draw = ImageDraw.Draw(gloss)
    gloss_draw.ellipse((105, 25, 930, 570), fill=(255, 255, 255, 37))
    icon_mask = Image.new("L", image.size, 0)
    icon_mask.paste(rounded_mask(840, 220), (92, 82))
    gloss.putalpha(ImageChops.multiply(gloss.getchannel("A"), icon_mask))
    image.alpha_composite(gloss)

    draw = ImageDraw.Draw(image)
    draw.ellipse((220, 206, 804, 790), fill=(245, 252, 255, 248), outline=(255, 255, 255, 255), width=20)
    draw.arc((294, 312, 730, 748), start=197, end=343, fill=(27, 88, 149, 255), width=38)

    for angle, point in (
        (0, (360, 505)),
        (1, (425, 418)),
        (2, (512, 386)),
        (3, (599, 418)),
        (4, (664, 505)),
    ):
        del angle
        x, y = point
        draw.ellipse((x - 15, y - 15, x + 15, y + 15), fill=(79, 183, 242, 255))

    draw.line((512, 535, 674, 398), fill=(10, 126, 241, 255), width=43)
    draw.ellipse((464, 487, 560, 583), fill=(10, 126, 241, 255), outline=(255, 255, 255, 255), width=19)
    draw.line((350, 666, 674, 666), fill=(27, 88, 149, 255), width=38)

    badge = Image.new("RGBA", image.size, (0, 0, 0, 0))
    badge_draw = ImageDraw.Draw(badge)
    badge_draw.rounded_rectangle((672, 666, 870, 864), radius=60, fill=(24, 198, 217, 255))
    badge_draw.line((716, 767, 762, 813, 836, 724), fill=(255, 255, 255, 255), width=29, joint="curve")
    image.alpha_composite(badge)
    return image


def main() -> None:
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    image = render_icon()
    image.save(
        OUTPUT,
        format="ICO",
        sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)],
    )
    print(OUTPUT)


if __name__ == "__main__":
    main()
