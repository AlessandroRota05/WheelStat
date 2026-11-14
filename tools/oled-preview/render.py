"""WheelStat - turns the PBM frames written by preview.cpp into the images
of the documentation: one PNG per screen, a sheet with all the live pages,
and two animations (boot, page navigation).

    python render.py <frames folder> <output folder> <it|en>

The pixels are drawn as a real panel shows them: square dots with a thin
gap, a faint glow, unlit dots barely visible.
"""

import glob
import os
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

W, H = 128, 64
LIT = (232, 244, 255)
UNLIT = (16, 21, 28)
GLASS = (6, 8, 11)
BEZEL = (24, 28, 34)
PAGE_BG = (13, 16, 21)
TEXT = (233, 237, 243)
DIM = (139, 147, 164)

CAPTIONS = {
    "it": {
        "page_0_lean": "0 · Piega",
        "page_1_weather": "1 · Meteo e grip",
        "page_2_gforce": "2 · Forza G",
        "page_3_wheelie": "3 · Impennata / stoppie",
        "page_4_gps": "4 · GPS",
        "page_5_laps": "5 · Giri",
        "page_6_records1": "6 · Record 1/2",
        "page_7_records2": "7 · Record 2/2",
        "page_8_storage": "8 · Memoria",
        "page_9_wifi": "9 · WiFi",
    },
    "en": {
        "page_0_lean": "0 · Lean",
        "page_1_weather": "1 · Weather and grip",
        "page_2_gforce": "2 · G-force",
        "page_3_wheelie": "3 · Wheelie / stoppie",
        "page_4_gps": "4 · GPS",
        "page_5_laps": "5 · Laps",
        "page_6_records1": "6 · Records 1/2",
        "page_7_records2": "7 · Records 2/2",
        "page_8_storage": "8 · Storage",
        "page_9_wifi": "9 · WiFi",
    },
}


def read_pbm(path):
    with open(path, "r", encoding="ascii") as f:
        tokens = f.read().split()
    assert tokens[0] == "P1" and tokens[1:3] == ["128", "64"], path
    rows = tokens[3:]
    return [[c == "1" for c in row] for row in rows]


def panel(bits, scale):
    """The glass of the panel with its dots, glow included."""
    gap = 1 if scale >= 3 else 0
    img = Image.new("RGB", (W * scale, H * scale), GLASS)
    lit = Image.new("L", img.size, 0)
    d = ImageDraw.Draw(img)
    dl = ImageDraw.Draw(lit)
    for y in range(H):
        for x in range(W):
            box = (x * scale, y * scale, x * scale + scale - 1 - gap, y * scale + scale - 1 - gap)
            if bits[y][x]:
                dl.rectangle(box, fill=255)
            else:
                d.rectangle(box, fill=UNLIT)
    # The glow is kept faint: dark text on a lit bar must stay readable.
    glow = lit.filter(ImageFilter.GaussianBlur(scale * 0.7)).point(lambda v: int(v * 0.22))
    img.paste(Image.new("RGB", img.size, LIT), (0, 0), glow)
    img.paste(Image.new("RGB", img.size, LIT), (0, 0), lit)
    return img


def framed(bits, scale):
    """The panel inside a dark bezel, like the module seen from the front."""
    glass = panel(bits, scale)
    pad = scale * 5
    out = Image.new("RGBA", (glass.width + 2 * pad, glass.height + 2 * pad), (0, 0, 0, 0))
    d = ImageDraw.Draw(out)
    d.rounded_rectangle((0, 0, out.width - 1, out.height - 1), radius=pad, fill=BEZEL)
    d.rounded_rectangle((pad // 2, pad // 2, out.width - 1 - pad // 2, out.height - 1 - pad // 2),
                        radius=pad // 2, fill=GLASS)
    out.paste(glass, (pad, pad))
    return out


def font(size, bold=False):
    names = ["segoeuib.ttf", "DejaVuSans-Bold.ttf"] if bold else ["segoeui.ttf", "DejaVuSans.ttf"]
    for name in names:
        for folder in ["C:/Windows/Fonts", "/usr/share/fonts/truetype/dejavu", ""]:
            try:
                return ImageFont.truetype(os.path.join(folder, name), size)
            except OSError:
                pass
    return ImageFont.load_default()


def sheet(frames_dir, names, captions, columns=2, scale=4):
    tiles = [framed(read_pbm(os.path.join(frames_dir, n + ".pbm")), scale) for n in names]
    tw, th = tiles[0].size
    caption_h, gap, margin = 34, 22, 28
    rows = (len(tiles) + columns - 1) // columns
    out = Image.new("RGB", (margin * 2 + columns * tw + (columns - 1) * gap,
                            margin * 2 + rows * (th + caption_h) + (rows - 1) * gap), PAGE_BG)
    d = ImageDraw.Draw(out)
    f = font(19, bold=True)
    for i, (tile, name) in enumerate(zip(tiles, names)):
        x = margin + (i % columns) * (tw + gap)
        y = margin + (i // columns) * (th + caption_h + gap)
        out.paste(tile, (x, y), tile)
        d.text((x + 4, y + th + 7), captions[name], font=f, fill=TEXT)
    return out


def animation(frames_dir, prefix, hold_every, hold_ms, step_ms, first_hold_ms, scale=3):
    paths = sorted(glob.glob(os.path.join(frames_dir, prefix + "_*.pbm")))
    images, durations = [], []
    for i, path in enumerate(paths):
        img = framed(read_pbm(path), scale).convert("RGB")
        images.append(img.quantize(colors=48, method=Image.Quantize.MEDIANCUT))
        if i == 0:
            durations.append(first_hold_ms)
        elif hold_every and i % hold_every == 0:
            durations.append(hold_ms)
        else:
            durations.append(step_ms)
    if not hold_every:
        durations[-1] = hold_ms
    return images, durations


def main():
    frames_dir, out_dir, lang = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(out_dir, exist_ok=True)

    for path in glob.glob(os.path.join(frames_dir, "*.pbm")):
        name = os.path.splitext(os.path.basename(path))[0]
        if name.startswith("anim_") or name == "popup_base":
            continue
        framed(read_pbm(path), 4).save(os.path.join(out_dir, name + ".png"), optimize=True)

    pages = list(CAPTIONS[lang].keys())
    sheet(frames_dir, pages, CAPTIONS[lang]).save(os.path.join(out_dir, "pages.png"), optimize=True)

    # Page navigation: 1 still frame, then 8 per change (7 slide steps and
    # the page at rest).
    images, durations = animation(frames_dir, "anim_pages", 8, 1400, 28, 1400)
    images[0].save(os.path.join(out_dir, "pages.gif"), save_all=True,
                   append_images=images[1:], duration=durations, loop=0, optimize=True)

    images, durations = animation(frames_dir, "anim_boot", 0, 1800, 45, 45)
    images[0].save(os.path.join(out_dir, "boot.gif"), save_all=True,
                   append_images=images[1:], duration=durations, loop=0, optimize=True)

    print("written", out_dir)


if __name__ == "__main__":
    main()
