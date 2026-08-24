"""Ask the network who is hosting a game, the same way another handheld would.

Proves the announcement leaves the device - an in-app log only proves the app
told Android about it.
"""
import socket, struct, sys, time

NAME = "_maplestory._tcp.local"

def encode(name):
    out = b""
    for part in name.split("."):
        out += bytes([len(part)]) + part.encode()
    return out + b"\x00"

q = struct.pack("!HHHHHH", 0, 0, 1, 0, 0, 0) + encode(NAME) + struct.pack("!HH", 12, 1)

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)
s.bind(("", 5353))
mreq = struct.pack("4sl", socket.inet_aton("224.0.0.251"), socket.INADDR_ANY)
s.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
s.settimeout(1.0)
s.sendto(q, ("224.0.0.251", 5353))

seen = set()
end = time.time() + 8
while time.time() < end:
    try:
        data, addr = s.recvfrom(4096)
    except socket.timeout:
        continue
    if b"maplestory" in data:
        # Pull the readable labels out rather than writing a DNS parser.
        txt = b"".join(bytes([c]) if 32 <= c < 127 else b" " for c in data)
        seen.add((addr[0], txt.decode().strip()))

if not seen:
    print("nothing answered")
    sys.exit(1)

for ip, txt in seen:
    print("REPLY from", ip)
    print("   ", " ".join(txt.split())[:160])
