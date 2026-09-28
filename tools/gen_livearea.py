"""Draws the LiveArea images (our own art, nothing from the original game)."""
from PIL import Image, ImageDraw, ImageFont
import random, os

FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"

def tower(w, h, seed=3, title=True, credit=None):
    random.seed(seed)
    s = 4
    im = Image.new("RGB", (w * s, h * s))
    d = ImageDraw.Draw(im)
    for y in range(h * s):  # night sky gradient
        t = y / (h * s)
        d.line([(0, y), (w * s, y)], fill=(int(20 + 30 * t), int(24 + 40 * t), int(70 + 90 * t)))
    # stone wall
    bw, bh = 16 * s * max(1, w // 128), 8 * s * max(1, w // 128)
    for row, y in enumerate(range(0, h * s, bh)):
        off = (row % 2) * bw // 2
        for x in range(-bw, w * s, bw):
            c = random.randint(70, 95)
            d.rectangle([x + off + s, y + s, x + off + bw - s, y + bh - s], fill=(c - 10, c - 8, c + 25))
    # icy platforms
    pw = w * s // 3
    for i, y in enumerate(range(h * s - bh * 2, 0, -bh * 5)):
        x = random.randint(0, w * s - pw)
        d.rounded_rectangle([x, y, x + pw, y + bh], radius=bh // 2, fill=(215, 235, 250), outline=(120, 160, 200), width=s)
    if title:
        size = int(w * s / 5.2)
        f = ImageFont.truetype(FONT, size)
        text = "ICY\nTOWER"
        bb = d.multiline_textbbox((0, 0), text, font=f, align="center", spacing=size // 8)
        tx = (w * s - (bb[2] - bb[0])) // 2 - bb[0]
        ty = (h * s - (bb[3] - bb[1])) // 2 - bb[1]
        d.multiline_text((tx + s * 3, ty + s * 3), text, font=f, fill=(10, 10, 30), align="center", spacing=size // 8)
        d.multiline_text((tx, ty), text, font=f, fill=(255, 255, 255), align="center", spacing=size // 8,
                         stroke_width=s * 2, stroke_fill=(30, 60, 140))
    if credit:
        f = ImageFont.truetype(FONT, int(h * s / 22))
        bb = d.textbbox((0, 0), credit, font=f)
        tx, ty = w * s - (bb[2] - bb[0]) - 12 * s, h * s - (bb[3] - bb[1]) - 14 * s
        d.text((tx, ty), credit, font=f, fill=(255, 255, 255), stroke_width=s * 2, stroke_fill=(20, 30, 70))
    return im.resize((w, h), Image.LANCZOS)

def save(im, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    im.convert("RGB").quantize(256).save(path)

save(tower(128, 128), "sce_sys/icon0.png")
save(tower(960, 544, title=True, credit="PS Vita port by İbrahim Doğan"), "sce_sys/pic0.png")
save(tower(840, 500, title=False, credit="PS Vita port by İbrahim Doğan"), "sce_sys/livearea/contents/bg.png")
save(tower(280, 158), "sce_sys/livearea/contents/startup.png")
