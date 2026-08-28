# Regenerates the game's icon from the MENU's own backdrop and the Alagard "DF".
#
#   python tools/make_icon.py
#
# Ported from the mobile game's tools/make_icon.py - the two projects share the
# font, the palette and this sky, and they are meant to be recognisably one thing.
# The one real difference is the tint: this game's menu multiplies that sky into
# red (Config::MenuSkyTint) because the dungeon sits under a red cubemap, and an
# icon taken from the untinted source would be a cold blue badge for a red game.
#
# Writes both files the build wants:
#   assets/icon/dungeon_foray.png   the picture, and what a store listing wants
#   assets/icon/dungeon_foray.ico   what Windows puts on the .exe, all sizes in one
from PIL import Image, ImageFilter, ImageDraw

SIZE = 512
SKY = r'assets/textures/menu/sky_night.png'
FONT = r'assets/fonts/alagard.png'
OUT_PNG = r'assets/icon/dungeon_foray.png'
OUT_ICO = r'assets/icon/dungeon_foray.ico'

INITIALS = 'DF'

# Config::MenuSkyTint. raylib's tint is a multiply, so this is the same arithmetic
# DrawTexturePro does - keep the two in step by hand, there is no build that reads
# the header from here.
TINT = (255, 58, 42)

# The sizes Windows actually picks between: 16 in a menu, 32 on the desktop, 48 in
# the shell, 256 in the large-icon view. One file holds them all.
ICO_SIZES = [(16, 16), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]


def tinted_crop():
    """The menu's picture, square, at icon size."""
    sky = Image.open(SKY).convert('RGBA')
    sw, sh = sky.size

    # Centred on both axes, which is the crop MenuBackdrop::Draw actually does -
    # an icon cut from a different part of the sky than the menu shows would be
    # an icon of a place the player never sees.
    side = min(sw, sh)
    left = (sw - side) // 2
    top = (sh - side) // 2

    crop = sky.crop((left, top, left + side, top + side))
    crop = crop.resize((SIZE, SIZE), Image.Resampling.LANCZOS)

    # The tint, as a multiply per channel
    px = crop.load()
    for y in range(SIZE):
        for x in range(SIZE):
            r, g, b, a = px[x, y]
            px[x, y] = (r*TINT[0]//255, g*TINT[1]//255, b*TINT[2]//255, a)

    return crop


def vignette(image):
    """Dark at the edges, so the letters have something to sit against."""
    overlay = Image.new('RGBA', (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)
    centre = SIZE // 2

    # Drawn as rings from the outside in, alpha falling off toward the middle.
    # Crude next to a real gradient and indistinguishable from one at this size.
    for r in range(SIZE // 2, 0, -2):
        t = 1.0 - (r/(SIZE/2))
        a = int(115*(t**1.8))
        draw.ellipse([centre - r, centre - r, centre + r, centre + r], fill=(6, 8, 16, a))

    return Image.alpha_composite(image, overlay)


def glyph_boxes(atlas, px):
    """Every glyph in the Alagard atlas, in order, as boxes.

    The atlas is a grid separated by its own key colour (the pixel at 0,0), so the
    glyphs are found by scanning for rows and then columns that are not entirely
    key - which is what makes this survive the atlas being re-exported at another
    size.
    """
    width, height = atlas.size
    key = px[0, 0][:3]

    def is_key(x, y):
        return px[x, y][:3] == key

    boxes = []
    y = 0

    while y < height:
        if not any(not is_key(x, y) for x in range(width)):
            y += 1
            continue

        y0 = y
        while y < height and any(not is_key(x, y) for x in range(width)):
            y += 1
        y1 = y

        x = 0
        while x < width:
            if not any(not is_key(x, yy) for yy in range(y0, y1)):
                x += 1
                continue

            x0 = x
            while x < width and any(not is_key(x, yy) for yy in range(y0, y1)):
                x += 1
            boxes.append((x0, y0, x, y1))

    return boxes


def glyph(atlas, px, boxes, ch, scale, colour=(255, 248, 235, 255)):
    """One letter, keyed out and recoloured, at `scale`.

    The atlas is opaque with a key colour rather than an alpha channel, so the
    alpha is taken from the pixel's own brightness: the anti-aliased edge of a
    letter is dimmer than its middle, and reading that as alpha is what keeps the
    edge smooth instead of stair-stepped.
    """
    box = boxes[ord(ch) - 32]
    out = atlas.crop(box).copy()
    op = out.load()
    key = px[0, 0][:3]

    for y in range(out.height):
        for x in range(out.width):
            r, g, b, a = op[x, y]

            if (r, g, b) == key:
                op[x, y] = (0, 0, 0, 0)
            else:
                op[x, y] = (colour[0], colour[1], colour[2], max(r, g, b))

    # NEAREST on purpose. Alagard is a pixel face and the menu draws it the same
    # way; smoothing it here would be the one place in the project it is soft.
    return out.resize((max(1, int(out.width*scale)), max(1, int(out.height*scale))),
                      Image.Resampling.NEAREST)


def main():
    picture = vignette(tinted_crop())

    atlas = Image.open(FONT).convert('RGBA')
    px = atlas.load()
    boxes = glyph_boxes(atlas, px)

    height = int(SIZE*0.42)
    scale = height/glyph(atlas, px, boxes, INITIALS[0], 1).height

    letters = [glyph(atlas, px, boxes, ch, scale) for ch in INITIALS]

    gap = max(4, int(SIZE*0.045))
    total = sum(letter.width for letter in letters) + gap*(len(letters) - 1)

    ox = (SIZE - total)//2
    oy = (SIZE - max(letter.height for letter in letters))//2

    # A soft drop shadow, so the letters read against whichever part of the sky
    # they happen to land on rather than only against the dark half
    shadow = Image.new('RGBA', (SIZE, SIZE), (0, 0, 0, 0))
    offset = max(2, SIZE//54)
    x = ox

    for letter in letters:
        shadow.paste((0, 0, 0, 185),
                     (x + offset, oy + offset, x + offset + letter.width, oy + offset + letter.height),
                     letter)
        x += letter.width + gap

    shadow = shadow.filter(ImageFilter.GaussianBlur(radius=max(1, SIZE//80)))

    out = Image.alpha_composite(picture, shadow)
    x = ox

    for letter in letters:
        out.paste(letter, (x, oy), letter)
        x += letter.width + gap

    out.save(OUT_PNG)
    out.save(OUT_ICO, sizes=ICO_SIZES)

    print('wrote', OUT_PNG, out.size)
    print('wrote', OUT_ICO, ICO_SIZES)


main()
