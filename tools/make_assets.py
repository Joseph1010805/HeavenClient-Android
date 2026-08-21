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
from nxbuild import Builder

# ffmpeg does the decoding and scaling. Anything on PATH will do; set the
# MAKE_ASSETS_FFMPEG environment variable to point at a specific build.
FFMPEG = os.environ.get('MAKE_ASSETS_FFMPEG', 'ffmpeg')

# Where the source video and the logo live, and where the built data file
# goes. Override with MAKE_ASSETS_SRC and MAKE_ASSETS_OUT.
SRC = os.environ.get('MAKE_ASSETS_SRC', os.path.join(
    os.path.expanduser('~'), 'Downloads'))
OUT = os.environ.get('MAKE_ASSETS_OUT', os.path.join(
    os.path.expanduser('~'), 'maple', 'Map001.nx'))

LOGIN_VIDEO = 'login.mp4'
CHARSEL_VIDEO = 'character selection.mp4'
LOGO_IMAGE = 'LoginIcon.jpg'
BOTTOM_IMAGE = 'bottomscreenbackground.jpg'

# The handheld's lower panel is 1240x1080. The client draws it in a design
# space of half that, which has the same shape, so a layout written once is
# right on any second screen of that proportion.
BOTTOM_W, BOTTOM_H = 620, 540

# Authoring size. Stretched to 800x600 by the client, so this trades sharpness
# against atlas space and the decompressed cache nlnx keeps per bitmap.
W, H = 400, 300


def frames(source, vf, fps, duration=None, start=None):
    """Run a filter chain and return the frames as raw BGRA."""
    cmd = [FFMPEG, '-y', '-v', 'error']

    if start is not None:
        cmd += ['-ss', str(start)]
    if duration is not None:
        cmd += ['-t', str(duration)]

    cmd += ['-i', source,
            '-vf', 'fps=%s,%s,scale=%d:%d' % (fps, vf, W, H),
            '-pix_fmt', 'bgra', '-f', 'rawvideo', '-']

    raw = subprocess.run(cmd, capture_output=True, check=True).stdout

    size = W * H * 4
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


def bottom_bgra(path):
    """The lower panel's backdrop, cropped to the panel's shape.

    The artwork is taller than it is wide and the panel is wider than it is
    tall, so a band is taken out of the middle - which is where the branch and
    the creatures on it sit.
    """
    img = Image.open(path).convert('RGB')
    w, h = img.size

    band = round(w * BOTTOM_H / BOTTOM_W)

    if band < h:
        top = (h - band) // 2
        img = img.crop((0, top, w, top + band))
    else:
        # Wider than the panel instead: take a band out of the middle the
        # other way round.
        band = round(h * BOTTOM_W / BOTTOM_H)
        left = (w - band) // 2
        img = img.crop((left, 0, left + band, h))

    img = img.resize((BOTTOM_W, BOTTOM_H), Image.LANCZOS)

    r, g, b = img.split()
    a = Image.new('L', img.size, 255)

    return Image.merge('RGBA', (b, g, r, a)).tobytes()


def animation(builder, parent, name, data, delay, zigzag=True):
    node = parent.child(name)

    for i, frame in enumerate(data):
        builder.bitmap(node, str(i), frame, W, H, origin=(0, 0), delay=delay)

    if zigzag:
        node.integer('zigzag', 1)

    print('  %-9s %3d frames @ %dms' % (name, len(data), delay))
    return node


def main():
    builder = Builder()
    custom = builder.root.child('Custom')

    print('extracting frames...')

    # The pan upward from the tree to the castle. The top strip carries the
    # generator's watermark, so it is cut before the frame is squared off to
    # 4:3 - cropping rather than squashing, which would narrow the faces.
    login = frames(os.path.join(SRC, LOGIN_VIDEO),
                   'crop=1024:585:0:55,crop=780:585:122:0', fps=4)

    # Near-static: a fixed scene with clouds drifting. Six frames a second is
    # plenty and costs a third of what ten would.
    charsel = frames(os.path.join(SRC, CHARSEL_VIDEO),
                     'crop=1440:1080:240:0', fps=3, start=2, duration=8)

    # Every frame of these sits in the sprite atlas for as long as the screen
    # is up, and they were holding about a quarter of it - which a full-screen
    # world map on the lower panel then tipped over, so the map was drawn with
    # whatever texture landed in the slots it had been using. Fewer frames,
    # each held longer.
    print('building nodes...')
    animation(builder, custom, 'LoginBg', login, delay=250)
    animation(builder, custom, 'CharBg', charsel, delay=333)

    # World select gets a still from the same scene, so the two screens match.
    builder.bitmap(custom, 'WorldBg', charsel[0], W, H, origin=(0, 0))
    print('  %-9s 1 still' % 'WorldBg')

    bottom = bottom_bgra(os.path.join(SRC, BOTTOM_IMAGE))
    builder.bitmap(custom, 'BottomBg', bottom, BOTTOM_W, BOTTOM_H, origin=(0, 0))
    print('  %-9s %dx%d' % ('BottomBg', BOTTOM_W, BOTTOM_H))

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
