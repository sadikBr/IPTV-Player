#!/usr/bin/env python3
"""Draws the app icon and writes every size the platforms need.

Run from the repository root:  python3 tools/make_icons.py   (needs Pillow)

The design matches the brand mark in the app's side bar: a warm gradient
tile (accentStrong -> accent2) with a white, slightly rounded play symbol.
"""
import os
from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ACCENT_STRONG = (0xE8, 0x46, 0x2A)
ACCENT = (0xFF, 0x5A, 0x36)
ACCENT2 = (0xFF, 0x8A, 0x5B)
BACKGROUND = (0x0B, 0x0B, 0x0D)

APP_ID = "io.github.sadikbr.iptvplayer"  # also the Linux desktop file name
SS = 4  # supersampling for smooth edges


def gradient(size):
    """Diagonal gradient, top-left accentStrong to bottom-right accent2."""
    small = Image.new("RGB", (256, 256))
    px = small.load()
    for y in range(256):
        for x in range(256):
            t = (x + y) / 510
            if t < 0.5:
                a, b, k = ACCENT_STRONG, ACCENT, t / 0.5
            else:
                a, b, k = ACCENT, ACCENT2, (t - 0.5) / 0.5
            px[x, y] = tuple(round(a[i] + (b[i] - a[i]) * k) for i in range(3))
    return small.resize((size, size), Image.BICUBIC)


def play_mask(size, scale):
    """White play triangle with rounded corners, centred optically."""
    big = size * SS
    mask = Image.new("L", (big, big), 0)
    d = ImageDraw.Draw(mask)
    h = big * scale            # triangle height
    w = h * 0.92               # a bit narrower than tall
    cx = big / 2 + w * 0.08    # optical centre: shift right
    cy = big / 2
    pts = [(cx - w / 2, cy - h / 2), (cx - w / 2, cy + h / 2), (cx + w / 2, cy)]
    r = max(1, int(h * 0.09))
    d.polygon(pts, fill=255)
    # Round the corners: blur then threshold.
    mask = mask.filter(ImageFilter.GaussianBlur(r))
    mask = mask.point(lambda v: 255 if v > 140 else 0)
    return mask.resize((size, size), Image.LANCZOS)


def tile_mask(size, radius_ratio):
    big = size * SS
    m = Image.new("L", (big, big), 0)
    ImageDraw.Draw(m).rounded_rectangle((0, 0, big - 1, big - 1),
                                        radius=int(big * radius_ratio), fill=255)
    return m.resize((size, size), Image.LANCZOS)


def icon(size, radius_ratio=0.225, play_scale=0.42, full_bleed=False):
    """The full icon. full_bleed: square, no transparent corners."""
    img = gradient(size).convert("RGBA")
    # Soft light from the top-left
    glow = Image.new("L", (size, size), 0)
    ImageDraw.Draw(glow).ellipse((-size * 0.3, -size * 0.45, size * 0.9, size * 0.55), fill=28)
    glow = glow.filter(ImageFilter.GaussianBlur(size * 0.12))
    img = Image.composite(Image.new("RGBA", (size, size), (255, 255, 255, 255)), img, glow)
    white = Image.new("RGBA", (size, size), (255, 255, 255, 255))
    img = Image.composite(white, img, play_mask(size, play_scale))
    if not full_bleed:
        img.putalpha(tile_mask(size, radius_ratio))
    return img


def foreground(size):
    """Android adaptive icon foreground: only the play symbol, inside the
    66% safe zone of a 108dp canvas."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    white = Image.new("RGBA", (size, size), (255, 255, 255, 255))
    return Image.composite(white, img, play_mask(size, 0.42 * 0.66))


def save(img, *parts):
    path = os.path.join(ROOT, *parts)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path)
    print("wrote", os.path.relpath(path, ROOT))


def main():
    master = icon(1024)
    save(master, "assets", "icon", "app-icon-1024.png")
    # Window icon (in the app's resources) and Linux desktop icons
    save(icon(256), "assets", "icon", "app-icon-256.png")
    for s in (16, 32, 48, 64, 128, 256, 512):
        save(icon(s), "assets", "icon", "hicolor", f"{s}x{s}", "apps", APP_ID + ".png")
    # Windows .ico with all classic sizes
    ico = os.path.join(ROOT, "assets", "icon", "app.ico")
    icon(256).save(ico, sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64),
                               (128, 128), (256, 256)])
    print("wrote", os.path.relpath(ico, ROOT))
    # Android launcher icons (legacy) and adaptive foreground
    densities = {"mdpi": 1, "hdpi": 1.5, "xhdpi": 2, "xxhdpi": 3, "xxxhdpi": 4}
    for name, k in densities.items():
        save(icon(int(48 * k)), "android", "res", f"mipmap-{name}", "ic_launcher.png")
        round_icon = icon(int(48 * k), radius_ratio=0.5)
        save(round_icon, "android", "res", f"mipmap-{name}", "ic_launcher_round.png")
        save(foreground(int(108 * k)), "android", "res", f"mipmap-{name}",
             "ic_launcher_foreground.png")
    # Background of the adaptive icon (and of the splash screen): the gradient
    save(gradient(432).convert("RGB"), "android", "res", "drawable-nodpi",
         "ic_launcher_background.png")


if __name__ == "__main__":
    main()
