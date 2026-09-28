"""Title and end cards for the trailer (960x544, our own text on a dimmed
still of the game)."""
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
REG = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"


def base(still):
    im = Image.open(still).convert("RGB").filter(ImageFilter.GaussianBlur(6))
    return Image.blend(im, Image.new("RGB", im.size, (8, 12, 30)), 0.72)


def centred(d, y, text, font, fill=(255, 255, 255), stroke=0, stroke_fill=(20, 40, 110)):
    bb = d.textbbox((0, 0), text, font=font, stroke_width=stroke)
    d.text(((960 - (bb[2] - bb[0])) // 2 - bb[0], y), text, font=font, fill=fill, stroke_width=stroke,
           stroke_fill=stroke_fill)


def title(still, out):
    im = base(still)
    d = ImageDraw.Draw(im)
    centred(d, 150, "ICY TOWER", ImageFont.truetype(BOLD, 110), stroke=6)
    centred(d, 290, "for PS Vita", ImageFont.truetype(BOLD, 44), fill=(170, 215, 255))
    centred(d, 380, "The original 1.3.1, frame for frame", ImageFont.truetype(REG, 30), fill=(220, 225, 235))
    im.save(out)


def end(still, out):
    im = base(still)
    d = ImageDraw.Draw(im)
    centred(d, 140, "Get Icy Tower for PS Vita", ImageFont.truetype(BOLD, 56), stroke=4)
    centred(d, 250, "github.com/ibrahim-dogan/vita-icy-tower", ImageFont.truetype(BOLD, 34), fill=(170, 215, 255))
    centred(d, 330, "by İbrahim Doğan", ImageFont.truetype(REG, 34), fill=(230, 232, 240))
    centred(d, 440, "Uses your own copy of the free PC game", ImageFont.truetype(REG, 22), fill=(170, 176, 190))
    im.save(out)


if __name__ == "__main__":
    title(sys.argv[1], sys.argv[2])
    end(sys.argv[1], sys.argv[3])
