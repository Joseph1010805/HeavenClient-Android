"""Write an NX (PKG4) file from scratch.

nxtrim.py copies a branch out of an existing archive; this builds one that
never existed, so custom artwork can be handed to the client through the same
door as Nexon's own data. The client reads backgrounds as ordinary animation
nodes, which means a video is expressible as NX: one bitmap child per frame,
each carrying an origin and a delay.

The payload contract is set by the client, not by the format: nlnx reads
4*width*height bytes from the bitmap offset and hands them to
LZ4_decompress_fast, so every blob is a uint32 compressed length followed by a
raw LZ4 block that inflates to exactly that many bytes of BGRA.
"""
import struct

HEADER = struct.Struct('<I I Q I Q I Q I Q')
NODE = struct.Struct('<I I H H 8s')

NONE, INT, REAL, STRING, VECTOR, BITMAP, AUDIO = range(7)


class Node:
    """One entry in the tree, before it is given an id."""

    def __init__(self, name, ntype=NONE, data=b'\0' * 8):
        self.name = name
        self.ntype = ntype
        self.data = data
        self.children = []

    def add(self, child):
        self.children.append(child)
        return child

    # The helpers below cover the node kinds this project actually writes.
    def child(self, name):
        return self.add(Node(name))

    def integer(self, name, value):
        return self.add(Node(name, INT, struct.pack('<q', value)))

    def vector(self, name, x, y):
        return self.add(Node(name, VECTOR, struct.pack('<ii', x, y)))


class Builder:
    def __init__(self):
        self.root = Node('')
        self.bitmaps = []

    def bitmap(self, parent, name, bgra, width, height, origin=None, delay=None):
        """Attach a bitmap node, storing the pixels compressed.

        `bgra` is raw BGRA8888, top row first, exactly 4*width*height bytes -
        the layout the client uploads straight to GL as GL_BGRA_EXT.
        """
        import lz4.block

        expected = 4 * width * height
        if len(bgra) != expected:
            raise ValueError('%s: got %d bytes, want %d for %dx%d'
                             % (name, len(bgra), expected, width, height))

        # store_size=False keeps it a raw LZ4 block. The length that
        # LZ4_decompress_fast needs is the DECOMPRESSED one, which it derives
        # from width and height, so the uint32 written ahead of the block is
        # the compressed length and is only used to find the next blob.
        packed = lz4.block.compress(bgra, mode='high_compression',
                                    compression=9, store_size=False)

        index = len(self.bitmaps)
        self.bitmaps.append(struct.pack('<I', len(packed)) + packed)

        node = parent.add(Node(name, BITMAP,
                               struct.pack('<IHH', index, width, height)))

        # A texture with no origin draws from its top-left; the client reads
        # this child and subtracts it from the draw position.
        node.vector('origin', *(origin or (0, 0)))

        if delay is not None:
            node.integer('delay', delay)

        return node

    def write(self, path):
        # Breadth-first, so every parent's children land in one contiguous run
        # - which is the only way the format can address them.
        out = [None]
        queue = [(self.root, 0)]
        strings = {}

        def string_id(text):
            raw = text.encode()
            if raw not in strings:
                strings[raw] = len(strings)
            return strings[raw]

        head = 0
        while head < len(queue):
            node, idx = queue[head]
            head += 1

            # Children MUST be sorted by name. Reading a child is a binary
            # search over this run, comparing raw bytes and then length, so an
            # unsorted run does not merely slow a lookup down - it silently
            # returns the wrong node or none at all, and the parent still
            # reports the right child count while every name under it resolves
            # to nothing.
            node.children.sort(key=lambda c: c.name.encode())

            first = len(out) if node.children else 0
            for child in node.children:
                queue.append((child, len(out)))
                out.append(None)

            out[idx] = NODE.pack(string_id(node.name), first,
                                 len(node.children), node.ntype, node.data)

        blobs = [struct.pack('<H', len(raw)) + raw
                 for raw, _ in sorted(strings.items(), key=lambda kv: kv[1])]

        with open(path, 'wb') as f:
            f.write(b'\0' * HEADER.size)

            node_off = f.tell()
            for packed in out:
                f.write(packed)

            def align():
                while f.tell() % 8:
                    f.write(b'\0')

            align()
            string_positions = []
            for blob in blobs:
                string_positions.append(f.tell())
                f.write(blob)

            align()
            string_table = f.tell()
            for pos in string_positions:
                f.write(struct.pack('<Q', pos))

            bitmap_positions = []
            for blob in self.bitmaps:
                align()
                bitmap_positions.append(f.tell())
                f.write(blob)

            align()
            bitmap_table = f.tell()
            for pos in bitmap_positions:
                f.write(struct.pack('<Q', pos))

            align()
            audio_table = f.tell()

            # The client reads 4*w*h bytes per bitmap regardless of how much
            # was actually stored, so the last (well-compressed) blob would be
            # read past the end of the file. The data it needs is all present,
            # but the over-read has to land somewhere.
            f.write(b'\0' * (4 << 20))

            f.seek(0)
            f.write(HEADER.pack(0x34474B50,
                                len(out), node_off,
                                len(blobs), string_table,
                                len(bitmap_positions), bitmap_table,
                                0, audio_table))

        return len(out), len(blobs), len(bitmap_positions)
