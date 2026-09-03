"""Build icon_chat.png - the Chat page's icon - out of the game's own balloon.

    python tools/make_chat_icon.py [path/to/UI.nx] [out.png]

Every other icon on the lower panel is a MapleStory item picture that was
saved out by hand. There is no item that means "say something": the closest
are the chat-balloon cash items, and those are a megaphone with a balloon
hanging off it, which is the icon the Voice page wants and would make the two
read as the same thing.

So this assembles the balloon the game itself draws over a player's head when
they talk. `ChatBalloon.img/0` is a nine-slice - four corners, four edges, a
centre tile and the little arrow - and stretching it to two tiles wide gives
a plain speech bubble at exactly icon size. It is the game's art, drawn the
way the game draws it, and it cannot be confused with the envelope on
Messages.

Run this once; make_assets.py picks the file up from the source folder with
all the others. It is here rather than inside make_assets.py because that
script only ever reads PNGs, and teaching it to read NX for one icon is more
machinery than one icon is worth.
"""
import os
import struct
import sys

import lz4.block
from PIL import Image

HEADER = struct.Struct('<I I Q I Q I Q I Q')
NODE = struct.Struct('<I I H H 8s')

# The nine-slice, plus the tail.
PIECES = ('nw', 'n', 'ne', 'w', 'c', 'e', 'sw', 's', 'se', 'arrow')

# Two tiles across, one down. Wider reads as a banner and taller reads as a
# thought bubble; this is the shape people draw when they draw speech.
TILES_X = 2
TILES_Y = 1


class Nx:
    """Just enough of the format to pull one bitmap out. See tools/nxdump.py."""

    def __init__(self, path):
        with open(path, 'rb') as f:
            self.data = memoryview(f.read())

        if bytes(self.data[:4]) != b'PKG4':
            raise SystemExit('not an NX file: ' + path)

        (_, self.nodecount, self.nodeoff,
         self.stringcount, self.stringoff,
         self.bitmapcount, self.bitmapoff,
         self.audiocount, self.audiooff) = HEADER.unpack_from(self.data, 0)

    def node(self, i):
        return NODE.unpack_from(self.data, self.nodeoff + i * 20)

    def string(self, i):
        off = struct.unpack_from('<Q', self.data, self.stringoff + i * 8)[0]
        n = struct.unpack_from('<H', self.data, off)[0]
        return bytes(self.data[off + 2:off + 2 + n]).decode('utf-8', 'replace')

    def find(self, path):
        i = 0

        for part in [p for p in path.split('/') if p]:
            _, child, count, _, _ = self.node(i)

            for k in range(count):
                if self.string(self.node(child + k)[0]) == part:
                    i = child + k
                    break
            else:
                raise SystemExit('no such node: ' + path)

        return i

    def bitmap(self, path):
        _, _, _, kind, blob = self.node(self.find(path))

        if kind != 5:
            raise SystemExit('not a bitmap: ' + path)

        index, w, h = struct.unpack_from('<I H H', blob, 0)
        off = struct.unpack_from('<Q', self.data, self.bitmapoff + index * 8)[0]
        length = struct.unpack_from('<I', self.data, off)[0]

        raw = lz4.block.decompress(
            bytes(self.data[off + 4:off + 4 + length]),
            uncompressed_size=w * h * 4)

        # NX bitmaps are BGRA. Splitting and re-merging in the other order is
        # a real swap; renaming the variables is not, which is how every icon
        # in this project came out blue once already.
        b, g, r, a = Image.frombytes('RGBA', (w, h), raw).split()

        return Image.merge('RGBA', (r, g, b, a))


def main():
    ui = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.expanduser('~'), 'maple', 'wz-v178', 'UI.nx')

    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(
        os.path.expanduser('~'), 'Downloads', 'icon_chat.png')

    nx = Nx(ui)
    part = {p: nx.bitmap('ChatBalloon.img/0/' + p) for p in PIECES}

    cw, ch = part['c'].size
    ew = part['w'].width
    eh = part['n'].height

    w = ew * 2 + cw * TILES_X
    h = eh * 2 + ch * TILES_Y

    # Room under the balloon for the tail, which hangs below the body.
    icon = Image.new('RGBA', (w, h + part['arrow'].height - 4), (0, 0, 0, 0))

    def put(img, x, y):
        icon.paste(img, (x, y), img)

    put(part['nw'], 0, 0)
    put(part['ne'], w - ew, 0)
    put(part['sw'], 0, h - eh)
    put(part['se'], w - ew, h - eh)

    for i in range(TILES_X):
        put(part['n'], ew + i * cw, 0)
        put(part['s'], ew + i * cw, h - eh)

    for j in range(TILES_Y):
        put(part['w'], 0, eh + j * ch)
        put(part['e'], w - ew, eh + j * ch)

        for i in range(TILES_X):
            put(part['c'], ew + i * cw, eh + j * ch)

    # Just in from the left corner, the way the game hangs it under a head.
    put(part['arrow'], ew + 2, h - 4)

    icon.save(out)
    print('%s  %dx%d' % (out, icon.width, icon.height))


if __name__ == '__main__':
    main()
