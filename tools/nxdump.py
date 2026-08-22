"""Print the child names under a path in an NX archive.

The counterpart to nxbuild.py: that one writes, this one looks. Written
because the reactor sounds were being looked up under names nobody had
actually checked existed - the client asks for `hit` and `break` under the
reactor's id, finds nothing, and plays nothing, which is indistinguishable
from having no sound at all.

    python tools/nxdump.py Sound.nx Reactor.img/2001

Prints each child's name, type and child count. With no path, prints the root.
"""
import struct
import sys

HEADER = struct.Struct('<I I Q I Q I Q I Q')
NODE = struct.Struct('<I I H H 8s')

TYPES = {0: 'none', 1: 'int', 2: 'real', 3: 'string',
         4: 'vector', 5: 'bitmap', 6: 'audio'}


class Nx:
    def __init__(self, path):
        with open(path, 'rb') as f:
            self.data = memoryview(f.read())

        magic = bytes(self.data[:4])
        if magic != b'PKG4':
            raise SystemExit(f'not an NX file (magic {magic!r})')

        (_, self.node_count, self.node_off,
         self.string_count, self.string_off,
         _, _, _, _) = HEADER.unpack_from(self.data, 0)

    def node(self, index):
        name, children, num, ntype, payload = NODE.unpack_from(
            self.data, self.node_off + index * NODE.size)
        return name, children, num, ntype, payload

    def string(self, index):
        off = struct.unpack_from('<Q', self.data,
                                 self.string_off + index * 8)[0]
        length = struct.unpack_from('<H', self.data, off)[0]
        return bytes(self.data[off + 2:off + 2 + length]).decode(
            'utf-8', 'replace')

    def child(self, index, want):
        """Linear scan, not a binary search - the point here is to see what is
        really stored, including anything mis-sorted enough that the client's
        own binary search would miss it."""
        _, first, num, _, _ = self.node(index)
        for i in range(first, first + num):
            if self.string(self.node(i)[0]) == want:
                return i
        return None

    def resolve(self, path):
        index = 0
        for part in [p for p in path.split('/') if p]:
            index = self.child(index, part)
            if index is None:
                return None
        return index


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)

    nx = Nx(sys.argv[1])
    path = sys.argv[2] if len(sys.argv) > 2 else ''

    index = nx.resolve(path)
    if index is None:
        raise SystemExit(f'no such node: {path!r}')

    _, first, num, ntype, _ = nx.node(index)
    print(f'{path or "/"}  type={TYPES.get(ntype, ntype)}  children={num}')

    for i in range(first, first + num):
        cname, _, cnum, ctype, _ = nx.node(i)
        print(f'  {nx.string(cname):<24} {TYPES.get(ctype, ctype):<8} '
              f'children={cnum}')


if __name__ == '__main__':
    main()
