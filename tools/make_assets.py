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
    """Load the sign and knock out the white it was drawn on.

    Only the white *around* the sign goes: the paper is cream and would be
    caught by any plain white-key, so the transparent region is grown inward
    from the edges and stops at the ink outline.
    """
    img = Image.open(path).convert('RGB')

    # Near-white AND near-neutral. The paper (roughly 250,243,225) is white
    # enough to pass the first test on its own, and is excluded by the second.
    px = img.load()
    w, h = img.size

    def is_background(x, y):
        r, g, b = px[x, y]
        return min(r, g, b) >= 238 and max(r, g, b) - min(r, g, b) <= 12

    # Flood inward from every edge pixel, so enclosed white (inside a letter,
    # say) is left opaque.
    mask = bytearray(w * h)
    stack = []

    for x in range(w):
        stack.append((x, 0))
        stack.append((x, h - 1))
    for y in range(h):
        stack.append((0, y))
        stack.append((w - 1, y))

    while stack:
        x, y = stack.pop()

        if x < 0 or y < 0 or x >= w or y >= h or mask[y * w + x]:
            continue
        if not is_background(x, y):
            continue

        mask[y * w + x] = 255
        stack += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]

    alpha = Image.frombytes('L', (w, h), bytes(mask)).point(lambda v: 255 - v)

    # Soften, then pull the midtones down. Blurring alone leaves a rim of
    # half-transparent white - the JPEG's own edge ringing - which shows as a
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
                   'crop=1024:585:0:55,crop=780:585:122:0', fps=10)

    # Near-static: a fixed scene with clouds drifting. Six frames a second is
    # plenty and costs a third of what ten would.
    charsel = frames(os.path.join(SRC, CHARSEL_VIDEO),
                     'crop=1440:1080:240:0', fps=6, start=2, duration=8)

    print('building nodes...')
    animation(builder, custom, 'LoginBg', login, delay=100)
    animation(builder, custom, 'CharBg', charsel, delay=167)

    # World select gets a still from the same scene, so the two screens match.
    builder.bitmap(custom, 'WorldBg', charsel[0], W, H, origin=(0, 0))
    print('  %-9s 1 still' % 'WorldBg')

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
