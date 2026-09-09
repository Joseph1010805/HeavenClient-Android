"""Build Map001.nx: the login-flow artwork, as animations the client can play.

The client already knows how to play an animation - numbered bitmap children,
each with an origin and a delay - so a video needs no new engine code, only a
data file shaped the way NX expects. Frames are authored small and stretched to
800x600 at draw time, because every frame lives in the texture atlas for as
long as the screen is up and full-size frames would crowd out everything else.

Playback is zigzag (forward then backward) so a loop has no visible cut: the
alternative is matching the last frame to the first, which video from a camera
pan cannot do.
"""
import os
import subprocess
import sys

from PIL import Image, ImageFilter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from nxbuild import Builder, Node

# ffmpeg does the decoding and scaling. Anything on PATH will do; set the
# MAKE_ASSETS_FFMPEG environment variable to point at a specific build.
FFMPEG = os.environ.get('MAKE_ASSETS_FFMPEG', 'ffmpeg')

# Where the source video and the logo live, and where the built data file
# goes. Override with MAKE_ASSETS_SRC and MAKE_ASSETS_OUT.
SRC = os.environ.get('MAKE_ASSETS_SRC', os.path.join(
    os.path.expanduser('~'), 'Downloads'))
OUT = os.environ.get('MAKE_ASSETS_OUT', os.path.join(
    os.path.expanduser('~'), 'maple', 'Map001.nx'))

# ONLY TWO OF THESE ARE OURS.
#
# Everything else this file builds is MapleStory's own artwork, which means
# the whole data file can be rebuilt by anybody who has a client of their own
# - and that matters, because tools/install.sh refuses to distribute Nexon's
# work and would otherwise have nothing to hand a new device.
#
# The videos that used to be here - a login pan, a character-select pan and a
# level-up burst - were made for this project and are gone. Handsome, and the
# only thing standing between a fresh install and a working game.
#
# The wordmarks stay: they are the project's own name and nobody else's.
LOGO_IMAGE = 'LoginIcon.jpg'
DS_LOGO_IMAGE = 'maplestorydslogo.png'

# THE THREE VIDEOS, BACK AGAIN.
#
# They were dropped so that a fresh install could rebuild this whole file from
# a player's own client - the videos are ours and cannot be derived from
# anything Nexon ships, so they were the one thing standing between "run the
# installer" and "have a working game".
#
# That trade is off: they are wanted, and the build simply SKIPS any of them
# whose source is missing. So a machine that has the .mp4 files gets the
# videos, a machine that does not gets a still, and both produce a Map001.nx
# the client is happy with. Nothing is required.
LOGIN_VIDEO = 'login.mp4'
CHARSEL_VIDEO = 'character selection.mp4'
LEVELUP_VIDEO = 'newlevelup.mp4'

# The death card. Same deal as the other three - ours, skipped in silence if
# the file is not here, and the panel simply draws nothing when the node is
# absent.
DEATH_VIDEO = 'Youdied.mp4'

# How much of each to keep. A loop is paid for in sprite-atlas space at
# W*H per frame, so this is the dial that matters - see FPS/DELAY below.
LOGIN_SECONDS = 8
CHARSEL_SECONDS = 8
LEVELUP_SECONDS = 2

# Dying is a beat, not a loop - long enough to land, short enough that it is
# not standing between the player and the revive prompt.
DEATH_SECONDS = 3

# The parchment the login and the panel sit on, both lifted from the game's
# own notice artwork.
TOP_IMAGE = 'icon_topscreen.png'
BOTTOM_IMAGE = 'icon_bottomscreen.png'
# One backdrop per page. Wide 16:9 artwork with the character in the LEFT
# corner, so they are cropped from the left rather than the middle - centring
# them would take the character off the picture entirely.
# THE PAGE BACKGROUNDS ARE GONE.
#
# A bag, a rack, a desk - one full-bleed photograph per page. They were
# dropped from the panel a while ago, because five pages showing five
# different rooms stopped the panel reading as one place, and the frame
# underneath is what made them a set. Nothing has drawn them since; this
# removes the 8MB they were still costing every build.
PAGE_IMAGES = {}

# THE PANEL'S OWN ICONS.
#
# Hand-picked artwork rather than nodes borrowed from the game's own files -
# a map mark of a town is not an inventory, and reading node names to guess
# what a picture looks like got Equipment a picture of Perion.
#
# Kept at their painted size (about 30px) and scaled up by the panel, which
# is what keeps them crisp: these are pixel art and enlarging them in the
# build would only blur them here instead of there.
# NOTE THE icon_ PREFIX. The source names collided with the PAGE backgrounds -
# `equipment.png` was the equipment page's 1280x720 artwork, and copying a 32px
# icon over it destroyed the page. Prefixed, they cannot collide again.
ICON_IMAGES = {
    'IconHome': 'icon_home.png',
    'IconAdventure': 'icon_adventure.png',
    'IconInventory': 'icon_inventory.png',
    'IconEquipment': 'icon_equipment.png',
    'IconStats': 'icon_stats.png',
    'IconQuest': 'icon_quest.png',
    'IconHotkeys': 'icon_key.png',
    'IconSocial': 'icon_social.png',
    'IconMap': 'icon_map.png',
    'IconSettings': 'icon_settings.png',
    'IconTime': 'icon_time.png',
    'IconLuck': 'icon_luck.png',
    'IconMinigame': 'icon_minigame.png',
    'IconScroll': 'icon_scroll.png',

    # The minimap page had been borrowing IconScroll, which is a scroll and
    # not a map. Its own artwork now.
    'IconMinimap': 'icon_minimap.png',
    'IconSave': 'icon_save.png',
    'IconMouse': 'icon_mouse.png',
    'IconHp': 'icon_hp.png',
    'IconMp': 'icon_mp.png',
    'IconShout': 'icon_shout.png',

    # ⚠ THE MEGAPHONE IS NOT THE ENVELOPE. IconShout is a magenta invitation
    # letter, which is what the Megaphone page was showing. This is the game's
    # own Megaphone item icon (5071000), lifted straight out of Item.nx.
    'IconMegaphone': 'icon_megaphone.png',
    'IconRoomMessage': 'icon_roommessage.png',
    # The speech balloon, built by tools/make_chat_icon.py out of the game's
    # own ChatBalloon.img. Run that script if this file is missing.
    'IconChat': 'icon_chat.png',

    # The second batch. Named for what they MEAN on the panel, not for the
    # file they came from - "equipmentininventory" is the EQUIP tab, and a
    # node called that would have to be looked up every time.
    'IconDoll': 'icon_doll.png',
    'IconReport': 'icon_report.png',
    'IconDaily': 'icon_dailyquests.png',
    'IconPvE': 'icon_pve.png',
    'IconPvP': 'icon_pvp.png',
    'IconSkills': 'icon_skills.png',
    'IconCharInfo': 'icon_charinfo.png',
    'IconKeyBindings': 'icon_keybindings.png',
    'IconExit': 'icon_exit.png',
    'IconHelp': 'icon_help.png',
    'IconScreenshot': 'icon_screenshot.png',
    'IconCashShop': 'icon_cashshop.png',
    'IconTrade': 'icon_trade.png',
    'IconPets': 'icon_pets.png',
    'IconFishing': 'icon_fishing.png',
    'IconMonsterBook': 'icon_monstercollection.png',
    'IconStopwatch': 'icon_stopwatch.png',
    'IconCrown': 'icon_crown.png',
    'IconEmotions': 'icon_emotions.png',
    'IconParty': 'icon_party.png',
    'IconHair': 'icon_hair.png',
    'IconBattery': 'icon_battery.png',

    # THE BADGE. 28x28, drawn small and on top of another icon to say that
    # something is waiting behind it - a level-up to spend, an unread
    # message, a daily quest that can be taken. Not a page of its own.
    'IconAlert': 'icon_alert.png',

    # The inventory's four tabs and the equipment window's three, as
    # BUTTONS rather than the game's own tab artwork.
    'IconTabEquip': 'icon_tabequip.png',
    'IconTabUse': 'icon_tabuse.png',
    'IconTabEtc': 'icon_tabetc.png',
    'IconTabCash': 'icon_tabcash.png',
    'IconTabGear': 'icon_tabgear.png',
    'IconTabGearCash': 'icon_tabgearcash.png',
    'IconTabPet': 'icon_tabpet.png',

    # The equipment folder's two halves, drawn for the job rather than
    # borrowed from the inventory tabs: what is ON you, and what is on you
    # only for the look of it.
    'IconWorn': 'icon_worn.png',
    'IconCosmetic': 'icon_cosmetic.png',
}

# THE GAUGE CHANNEL, both ways up.
#
# One drawing, 180x20 - a rounded silver rim round a black trough. The EXP bar
# is horizontal and the HP/MP bars are vertical, so the vertical one is made
# HERE by rotating the picture, not at runtime: DrawArgument can rotate, but
# it rotates about a centre and getting that right blind is how the icons
# ended up at 57 degrees. A second bitmap costs 14KB and cannot be got wrong.
BAR_IMAGE = 'barframe.png'

# Icons built from MORE THAN ONE picture, back to front.
#
# Character is the two dolls BACK TO BACK, each looking the other way - the
# pair reads as "your party" where either alone reads as one person. They were
# stacked, one behind the other, which just looked like a queue.
#
# Both source sprites face LEFT, so the right-hand one is flipped: that is what
# turns "both walking the same way" into "standing back to back". They overlap
# by a few pixels so the shoulders touch rather than leaving a gap down the
# middle of the icon.
#
# Listed back-first: (file, offset, flip). The canvas grows to hold the lot.
ICON_COMPOSITES = {
    'IconCharacter': [
        ('icon_characterleft.png', (0, 0), False),
        ('icon_characterrightmirorimage.png', (22, 0), True),
    ],
}

# The handheld's lower panel is 1240x1080. The client draws it in a design
# space of half that, which has the same shape, so a layout written once is
# right on any second screen of that proportion.
BOTTOM_W, BOTTOM_H = 620, 540

# Authoring size. Stretched to 800x600 by the client, so this trades sharpness
# against atlas space and the decompressed cache nlnx keeps per bitmap.
W, H = 400, 300

# How the videos play back.
#
# The client steps every 8ms, so a delay is best kept a multiple of that: 80ms
# is 12.5 frames a second, which is the point where a slow camera pan stops
# reading as a slideshow. Each frame costs W*H pixels of the 8192x8192 sprite
# atlas, so this is the dial to turn if the atlas ever does run short - and
# turn DURATION down before turning this down, because a shorter loop is much
# less noticeable than a juddering one.
FPS = 12
DELAY = 80


def frames(source, vf, fps, duration=None, start=None, w=None, h=None):
    """Run a filter chain and return the frames as raw BGRA."""
    cmd = [FFMPEG, '-y', '-v', 'error']

    if start is not None:
        cmd += ['-ss', str(start)]
    if duration is not None:
        cmd += ['-t', str(duration)]

    cmd += ['-i', source,
            '-vf', 'fps=%s,%s,scale=%d:%d' % (fps, vf, w or W, h or H),
            '-pix_fmt', 'bgra', '-f', 'rawvideo', '-']

    raw = subprocess.run(cmd, capture_output=True, check=True).stdout

    size = (w or W) * (h or H) * 4
    if len(raw) % size:
        raise SystemExit('ragged frame data: %d is not a multiple of %d'
                         % (len(raw), size))

    return [raw[i:i + size] for i in range(0, len(raw), size)]


def logo_bgra(path, target_width):
    """Load the sign and knock out the background it was drawn on.

    Only what is *not the sign* goes. The paper is cream, so a plain white-key
    would eat it; instead a pixel counts as background when it is pale AND
    near-neutral, which the cream fails on the second test whatever its
    brightness.
    """
    img = Image.open(path).convert('RGB')

    px = img.load()
    w, h = img.size

    # The threshold has to reach down to about 212, not the 238 that "white"
    # suggests: the artist drew a pale grey-green drop shadow down the right
    # side of the sign, roughly (223,230,223), and left opaque it reads as a
    # white outline against the sky.
    def is_background(x, y):
        r, g, b = px[x, y]
        return min(r, g, b) >= 212 and max(r, g, b) - min(r, g, b) <= 18

    # Label every pale-neutral region, then decide which are actually holes.
    seen = bytearray(w * h)
    mask = bytearray(w * h)

    for sy in range(h):
        for sx in range(w):
            if seen[sy * w + sx] or not is_background(sx, sy):
                continue

            stack = [(sx, sy)]
            seen[sy * w + sx] = 1
            pixels = []
            touches_edge = False
            x0 = x1 = sx
            y0 = y1 = sy

            while stack:
                x, y = stack.pop()
                pixels.append(y * w + x)

                if x == 0 or y == 0 or x == w - 1 or y == h - 1:
                    touches_edge = True

                x0 = min(x0, x)
                x1 = max(x1, x)
                y0 = min(y0, y)
                y1 = max(y1, y)

                for a, b in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                    if (0 <= a < w and 0 <= b < h
                            and not seen[b * w + a] and is_background(a, b)):
                        seen[b * w + a] = 1
                        stack.append((a, b))

            # Anything reachable from outside is background. An enclosed region
            # is a hole only when it is wide and thin - that shape means a gap
            # under a horizontal element, here the sky between the hanging pole
            # and the top of the sign. Highlights on the paper are compact, so
            # this leaves them alone rather than punching holes in the artwork.
            wide_and_thin = (x1 - x0) >= 0.35 * w and (y1 - y0) <= 0.12 * h

            if touches_edge or wide_and_thin:
                for i in pixels:
                    mask[i] = 255

    alpha = Image.frombytes('L', (w, h), bytes(mask)).point(lambda v: 255 - v)

    # Soften, then pull the midtones down. Blurring alone leaves a rim of
    # half-transparent pixels - the JPEG's own edge ringing - which shows as a
    # pale halo once it is composited over the sky.
    alpha = alpha.filter(ImageFilter.GaussianBlur(0.8))
    alpha = alpha.point(lambda v: 0 if v < 96 else min(255, int((v - 96) * 1.6)))

    img.putalpha(alpha)

    # Trim to what is actually drawn, so the atlas holds no empty margin.
    box = img.getbbox()
    if box:
        img = img.crop(box)

    ratio = target_width / img.width
    img = img.resize((target_width, max(1, round(img.height * ratio))),
                     Image.LANCZOS)

    r, g, b, a = img.split()
    return Image.merge('RGBA', (b, g, r, a)).tobytes(), img.width, img.height


def bottom_bgra(path, anchor='center'):
    """The lower panel's backdrop, cropped to the panel's shape.

    The artwork is taller than it is wide and the panel is wider than it is
    tall, so a band is taken out of the middle - which is where the branch and
    the creatures on it sit.
    """
    img = Image.open(path).convert('RGB')
    w, h = img.size

    band = round(w * BOTTOM_H / BOTTOM_W)

    if band < h:
        top = 0 if anchor == 'left' else (h - band) // 2
        img = img.crop((0, top, w, top + band))
    else:
        # Wider than the panel instead: take a band out the other way round.
        band = round(h * BOTTOM_W / BOTTOM_H)
        left = 0 if anchor == 'left' else (w - band) // 2
        img = img.crop((left, 0, left + band, h))

    img = img.resize((BOTTOM_W, BOTTOM_H), Image.LANCZOS)

    r, g, b = img.split()
    a = Image.new('L', img.size, 255)

    return Image.merge('RGBA', (b, g, r, a)).tobytes()


def keep_alpha_bgra(path, target_width):
    """An image that already has its own transparency, scaled to a width.

    Unlike the sign, nothing has to be keyed out here - the artwork arrives
    with an alpha channel, and all that would do is damage it.
    """
    img = Image.open(path).convert('RGBA')

    ratio = target_width / img.width
    img = img.resize((target_width, max(1, round(img.height * ratio))),
                     Image.LANCZOS)

    r, g, b, a = img.split()
    return Image.merge('RGBA', (b, g, r, a)).tobytes(), img.width, img.height


# THE PLAIN PART OF THE PARCHMENT.
#
# icon_topscreen.png is a photograph of one of the game's own notice windows,
# and it came with the window's furniture: a soft rounded border on all four
# sides, an ornamental band down the right, and a mushroom sitting in the
# bottom-right corner - all three sliced off mid-stroke by the original crop.
#
# Stretched across the login screen those became a second border inside the
# game's border and half a mushroom hanging off the edge. Cropping to the
# plain middle throws them away and keeps the thing that was actually wanted,
# which is the paper.
#
# Measured against the 464x302 source. If that image is ever replaced this
# needs re-measuring - it is a crop of a specific picture, not a rule.
TOP_PLAIN = (12, 12, 398, 236)

# The same treatment for the lower screen's parchment, which is a crop of the
# same window and came with the same half a mushroom. Measured against the
# 384x222 source.
BOTTOM_PLAIN = (10, 10, 320, 180)


# WHERE THE GAME'S OWN ART LIVES.
#
# Set MAKE_ASSETS_WZ if the client data is somewhere else. Reading the login
# backdrop straight out of Map.nx rather than shipping a PNG keeps the whole
# file rebuildable from a player's OWN client, which is the same rule the
# videos are held to - nothing here redistributes Nexon's work.
WZ = os.environ.get('MAKE_ASSETS_WZ',
                    os.path.join(os.path.expanduser('~'), 'maple', 'wz-v83'))

# The login backdrop: the biggest tile in this set, a wide cloudscape.
LOGIN_BACKDROP = ('timeTemple.img', 700, 400)

# WHICH ROWS OF THAT TILE ARE ACTUALLY SKY.
#
# A map backdrop is not a picture, it is a layer: the top corners and the
# bottom third are TRANSPARENT, because the map draws ground over them. Taken
# whole and flattened, those holes come out black - a stepped notch across the
# top and a black blob along the bottom, which is what the first attempt
# looked like.
#
# Measured off the tile's own alpha: rows 0-480 average ~220, rows 560+ drop
# to 21. So the sky is the band between.
LOGIN_SKY = (40, 490)


def scenery(setname, min_w, min_h):
    """The largest back tile in a Map.nx set, as a PIL image. None if absent."""
    import struct

    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

    from nxdump import Nx
    from nxbmp import NxBmp

    path = os.path.join(WZ, 'Map.nx')

    if not os.path.exists(path):
        return None

    mp, bm = Nx(path), NxBmp(path)

    node = mp.resolve('Back/%s/back' % setname)

    if node is None:
        return None

    _, first, num, _, _ = mp.node(node)
    best = None

    for m in range(first, first + num):
        _, _, _, t, pl = mp.node(m)

        if t != 5:
            continue

        bid, w, h = struct.unpack_from('<IHH', pl)

        if w >= min_w and h >= min_h and (best is None or w * h > best[1] * best[2]):
            best = (bid, w, h)

    if best is None:
        return None

    bid, w, h = best
    raw = bm.bitmap(bid, w, h)

    # NX keeps pixels BGRA; PIL wants RGBA.
    im = Image.frombytes('RGBA', (w, h), bytes(raw))
    b, g, r, a = im.split()

    return Image.merge('RGBA', (r, g, b, a))


def flatten(img, band):
    """Take the opaque band of a map layer and fill what holes remain.

    The fill is the mean of the SOLID pixels, so the small gaps left inside
    the clouds blend into their own artwork instead of punching through to
    black.
    """
    w, h = img.size
    top, bottom = band

    img = img.crop((0, top, w, min(bottom, h)))

    px = img.load()
    total = [0, 0, 0]
    count = 0

    for y in range(0, img.height, 3):
        for x in range(0, w, 5):
            r, g, b, a = px[x, y]

            if a > 240:
                total[0] += r
                total[1] += g
                total[2] += b
                count += 1

    fill = tuple(c // count for c in total) if count else (0, 0, 0)

    flat = Image.new('RGB', img.size, fill)
    flat.paste(img, (0, 0), img)

    return flat


def fit_image(img, width, height):
    """Crop an already-loaded image to a shape and scale to it."""
    w, h = img.size

    band = round(w * height / width)

    if band < h:
        top = (h - band) // 2
        img = img.crop((0, top, w, top + band))
    else:
        band = round(h * width / height)
        left = (w - band) // 2
        img = img.crop((left, 0, left + band, h))

    img = img.resize((width, height), Image.LANCZOS)

    r, g, b = img.split()
    a = Image.new('L', img.size, 255)

    return Image.merge('RGBA', (b, g, r, a)).tobytes()


def fit_bgra(path, width, height, crop=None):
    """Crop to a shape and scale to it, keeping the middle."""
    img = Image.open(path).convert('RGB')

    if crop:
        img = img.crop(crop)

    w, h = img.size

    band = round(w * height / width)

    if band < h:
        top = (h - band) // 2
        img = img.crop((0, top, w, top + band))
    else:
        band = round(h * width / height)
        left = (w - band) // 2
        img = img.crop((left, 0, left + band, h))

    img = img.resize((width, height), Image.LANCZOS)

    r, g, b = img.split()
    a = Image.new('L', img.size, 255)

    return Image.merge('RGBA', (b, g, r, a)).tobytes()


def animation(builder, parent, name, data, delay, zigzag=True, w=None, h=None):
    node = parent.child(name)

    for i, frame in enumerate(data):
        builder.bitmap(node, str(i), frame, w or W, h or H, origin=(0, 0), delay=delay)

    if zigzag:
        node.integer('zigzag', 1)

    print('  %-9s %3d frames @ %dms' % (name, len(data), delay))
    return node


def main():
    builder = Builder()
    custom = builder.root.child('Custom')

    print('building nodes...')

    # ONE PICTURE, THREE PLACES.
    #
    # Login, character select and world select all showed a different moving
    # backdrop. They are the same parchment now - the game's own - which is
    # both honest about what this build ships and quieter to look at than a
    # video loop behind a form.
    top = fit_bgra(os.path.join(SRC, TOP_IMAGE), W, H, crop=TOP_PLAIN)

    # A MOVING BACKDROP WHERE THERE IS A VIDEO FOR IT, A STILL WHERE THERE IS
    # NOT.
    #
    # An NX animation is just a node whose children are numbered frames, each
    # carrying a `delay` - which is why a video needs no engine code at all,
    # only frames on disk. The client already treats these three nodes as
    # animations if they have numbered children and as a still if they do not,
    # so both shapes work with the code exactly as it stands.
    #
    # WorldBg keeps the parchment on purpose: the world list is a short stop
    # between two moving screens, and three pans in a row is seasickness.
    def moving_backdrop(name, filename, seconds):
        path = os.path.join(SRC, filename)

        if not os.path.exists(path):
            builder.bitmap(custom, name, top, W, H, origin=(0, 0))
            print('  %-9s %dx%d  %s (no %s - still)' % (name, W, H, TOP_IMAGE, filename))
            return

        shots = frames(path, 'crop=iw:ih', FPS, duration=seconds)

        node = custom.add(Node(name))

        for i, shot in enumerate(shots):
            builder.bitmap(node, str(i), shot, W, H, origin=(0, 0), delay=DELAY)

        print('  %-9s %dx%d  %s, %d frames @ %dms' % (name, W, H, filename,
                                                     len(shots), DELAY))

    # THE LOGIN IS A PAINTING, THE CHARACTER SCREEN IS A PAN.
    #
    # Deliberately different: the login is the first thing anyone sees and a
    # still cloudscape reads as a title card, where a second moving pan right
    # before the character one just makes the whole opening restless. It also
    # gives back a third of the sprite atlas the videos cost.
    #
    # Falls back to the parchment if Map.nx is not where WZ says.
    art = scenery(*LOGIN_BACKDROP)

    if art is not None:
        art = flatten(art, LOGIN_SKY)

        builder.bitmap(custom, 'LoginBg', fit_image(art, W, H), W, H, origin=(0, 0))
        print('  %-9s %dx%d  Map.nx Back/%s' % ('LoginBg', W, H, LOGIN_BACKDROP[0]))
    else:
        builder.bitmap(custom, 'LoginBg', top, W, H, origin=(0, 0))
        print('  %-9s %dx%d  %s (no Map.nx - parchment)' % ('LoginBg', W, H, TOP_IMAGE))

    moving_backdrop('CharBg', CHARSEL_VIDEO, CHARSEL_SECONDS)

    builder.bitmap(custom, 'WorldBg', top, W, H, origin=(0, 0))
    print('  %-9s %dx%d  %s' % ('WorldBg', W, H, TOP_IMAGE))

    # The wordmark the lower panel shows while the game is loading, in place
    # of a line of text. Its own transparency is kept as it arrives.
    ds, dw, dh = keep_alpha_bgra(os.path.join(SRC, DS_LOGO_IMAGE), 240)
    builder.bitmap(custom, 'DsLogo', ds, dw, dh, origin=(0, 0))
    print('  %-9s %dx%d' % ('DsLogo', dw, dh))

    bottom = fit_bgra(os.path.join(SRC, BOTTOM_IMAGE), BOTTOM_W, BOTTOM_H,
                      crop=BOTTOM_PLAIN)
    builder.bitmap(custom, 'BottomBg', bottom, BOTTOM_W, BOTTOM_H, origin=(0, 0))
    print('  %-9s %dx%d  %s' % ('BottomBg', BOTTOM_W, BOTTOM_H, BOTTOM_IMAGE))

    # THE LEVEL-UP BURST, ON THE LOWER PANEL.
    #
    # Distinct from the game's own effect, which plays on the character up on
    # the top screen. This one is ours and it plays down here, so the panel
    # marks the moment too rather than sitting there showing a bag.
    #
    # Skipped in silence when the source is absent: SecondScreenPanel asks for
    # 'LevelUp', gets an invalid texture and draws nothing, which is exactly
    # how it behaved for the whole time the node was missing.
    levelup_path = os.path.join(SRC, LEVELUP_VIDEO)

    if os.path.exists(levelup_path):
        shots = frames(levelup_path, 'crop=iw:ih', FPS,
                       duration=LEVELUP_SECONDS,
                       w=BOTTOM_W, h=BOTTOM_H)

        node = custom.add(Node('LevelUp'))

        for i, shot in enumerate(shots):
            builder.bitmap(node, str(i), shot, BOTTOM_W, BOTTOM_H,
                           origin=(0, 0), delay=DELAY)

        print('  %-9s %dx%d  %s, %d frames @ %dms'
              % ('LevelUp', BOTTOM_W, BOTTOM_H, LEVELUP_VIDEO, len(shots), DELAY))
    else:
        print('  %-9s skipped - no %s' % ('LevelUp', LEVELUP_VIDEO))

    # THE DEATH CARD, ON THE LOWER PANEL.
    #
    # Built exactly like the level-up burst above, and for the same reason:
    # the moment is marked down here as well as up on the map, so the panel is
    # part of what happened rather than a bag someone forgot to close.
    #
    # The words "You Died" are NOT burnt into these frames. They are drawn by
    # SecondScreenPanel over the top, so they stay crisp at whatever size the
    # panel happens to be and can be changed without re-encoding a video.
    death_path = os.path.join(SRC, DEATH_VIDEO)

    if os.path.exists(death_path):
        shots = frames(death_path, 'crop=iw:ih', FPS,
                       duration=DEATH_SECONDS,
                       w=BOTTOM_W, h=BOTTOM_H)

        node = custom.add(Node('Died'))

        for i, shot in enumerate(shots):
            builder.bitmap(node, str(i), shot, BOTTOM_W, BOTTOM_H,
                           origin=(0, 0), delay=DELAY)

        print('  %-9s %dx%d  %s, %d frames @ %dms'
              % ('Died', BOTTOM_W, BOTTOM_H, DEATH_VIDEO, len(shots), DELAY))
    else:
        print('  %-9s skipped - no %s' % ('Died', DEATH_VIDEO))

    # A backdrop for every page that has one. Cropped from the left so the
    # character in the corner survives the change of shape.
    for node_name, filename in sorted(PAGE_IMAGES.items()):
        art = bottom_bgra(os.path.join(SRC, filename), anchor='left')
        builder.bitmap(custom, node_name, art, BOTTOM_W, BOTTOM_H, origin=(0, 0))
        print('  %-9s %dx%d  %s' % (node_name, BOTTOM_W, BOTTOM_H, filename))

    # The panel's icons, at the size they were drawn.
    def icon_sources():
        for node_name, filename in sorted(ICON_IMAGES.items()):
            path = os.path.join(SRC, filename)

            if not os.path.exists(path):
                print('  %-14s MISSING %s' % (node_name, filename))
                continue

            yield node_name, Image.open(path).convert('RGBA')

        for node_name, parts in sorted(ICON_COMPOSITES.items()):
            layers = []

            for filename, at, flip in parts:
                path = os.path.join(SRC, filename)

                if not os.path.exists(path):
                    print('  %-14s MISSING %s' % (node_name, filename))
                    layers = []
                    break

                layer = Image.open(path).convert('RGBA')

                if flip:
                    layer = layer.transpose(Image.FLIP_LEFT_RIGHT)

                layers.append((layer, at))

            if not layers:
                continue

            w = max(im.width + at[0] for im, at in layers)
            h = max(im.height + at[1] for im, at in layers)

            canvas = Image.new('RGBA', (w, h), (0, 0, 0, 0))

            for im, at in layers:
                canvas.alpha_composite(im, at)

            yield node_name, canvas

    for node_name, src in icon_sources():

        # BGRA, and the swap has to be REAL. Writing `b, g, r, a = split()`
        # only renames the channels - split() still hands back R, G, B, A - so
        # merging them back in that order changes nothing and every icon
        # arrives with red and blue exchanged. A brown book comes out blue.
        r, g, b, a = src.split()
        data = Image.merge('RGBA', (b, g, r, a)).tobytes()

        builder.bitmap(custom, node_name, data, src.width, src.height,
                       origin=(0, 0))
        # No filename here: a composite has several, and `filename` left over
        # from the loop above named the wrong file for every icon printed.
        print('  %-14s %dx%d' % (node_name, src.width, src.height))

    bar_path = os.path.join(SRC, BAR_IMAGE)

    if os.path.exists(bar_path):
        flat = Image.open(bar_path).convert('RGBA')

        for node_name, im in (('BarH', flat),
                              ('BarV', flat.transpose(Image.ROTATE_90))):
            r, g, b, a = im.split()

            builder.bitmap(custom, node_name,
                           Image.merge('RGBA', (b, g, r, a)).tobytes(),
                           im.width, im.height, origin=(0, 0))

            print('  %-14s %dx%d' % (node_name, im.width, im.height))
    else:
        print('  BarH/BarV      MISSING %s' % BAR_IMAGE)

    logo, lw, lh = logo_bgra(os.path.join(SRC, LOGO_IMAGE), 240)
    builder.bitmap(custom, 'Logo', logo, lw, lh, origin=(0, 0))
    print('  %-9s %dx%d' % ('Logo', lw, lh))

    os.makedirs(os.path.dirname(OUT), exist_ok=True)

    print('writing %s ...' % OUT)
    nodes, strings, bitmaps = builder.write(OUT)

    print('nodes %d  strings %d  bitmaps %d  size %.1f MB'
          % (nodes, strings, bitmaps, os.path.getsize(OUT) / 1048576))


if __name__ == '__main__':
    main()
