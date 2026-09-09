"""Pull a bitmap out of an NX file. NX stores them as LZ4 blocks of BGRA."""
import struct, lz4.block
HEADER = struct.Struct('<I I Q I Q I Q I Q')

class NxBmp:
    def __init__(self, path):
        with open(path, 'rb') as f:
            self.data = memoryview(f.read())
        (_, self.node_count, self.node_off, self.string_count, self.string_off,
         self.bitmap_count, self.bitmap_off, _, _) = HEADER.unpack_from(self.data, 0)

    def bitmap(self, bid, w, h):
        off = struct.unpack_from('<Q', self.data, self.bitmap_off + bid * 8)[0]
        length = struct.unpack_from('<I', self.data, off)[0]
        raw = lz4.block.decompress(bytes(self.data[off+4:off+4+length]),
                                   uncompressed_size=w*h*4)
        return raw   # BGRA
