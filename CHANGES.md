# What changed in the Android port

## Making it run on Android

GLFW has no Android backend, so SDL2 handles the window, the GL context and
input instead (`IO/Window_Android.cpp`). Key codes are still GLFW values, so
existing config files and `IO/Keyboard.cpp` work untouched.

Desktop OpenGL became GLES2: quads are drawn as pairs of triangles, `GL_RED`
became `GL_LUMINANCE`, `GL_BGRA` became `GL_BGRA_EXT`, and the shaders were
rewritten as GLSL ES 1.00.

There's an on-screen keyboard now, which appears when a text field has focus.
Typed characters come in as `SDL_TEXTINPUT` and go straight to the field rather
than through the key-code path, which would lose capitals - passwords care.

The frame renders to an offscreen buffer at the game's own resolution and is
scaled up to the screen in one go, instead of every sprite being scaled
separately.

libnx and mbedtls are gone. Sockets use asio like the desktop build, and
`randomGet64()` became `std::random_device`.

## Bugs fixed

Some of these aren't Android-specific and affect the desktop client too.

**All text was unreadable.** The sprite atlas is 8192x8192, and the fragment
shader ran at `mediump` precision - roughly 1024 steps across a texture
coordinate, which works out to about 8 pixels on an atlas that size. Big artwork
survived; every letter was shredded. The texture coordinate and atlas size need
to be `highp`.

**Sprites drew as blank rectangles.** Texture uploads never bound the atlas -
they relied on it still being bound from startup. Uploading into whatever
happened to be bound leaves an atlas entry that looks fine but was never
written.

**Monsters never appeared.** The client skipped 22 bytes of the spawn packet
before reading position; Cosmic writes 16. Six bytes out meant every monster got
a nonsense position and foothold, so they spawned somewhere off the map.

**Losing connection looked like the game breaking.** The socket only checked for
errors when data was already waiting, and a dead connection has none - so the
client carried on drawing the world and sending input into nothing. Attacks
that never land, monsters that deal no damage.

**Dying did nothing at all.** `Char::State::DIED` exists and is read in two
places, but nothing anywhere ever set it - not here, not in the current
upstream. Hitting 0 HP now prompts you and sends the revive request.

**Settings lost a character per run.** `Configuration::load()` trimmed the last
character off every value on files with Unix line endings, and `save()` wrote
the damage back. `1280` became `128`, then `12`. This affects every platform
except Windows.

**`GraphicsGL::clear()` can never run.** It compares a percentage against `80.0`
when the value is a fraction between 0 and 1, so the atlas is never tidied and
only ever wiped completely when it fills.

## Tuning

Walking speed and monster knockback are lower than upstream. Those numbers are
HeavenClient's approximation of how v83 felt rather than anything extracted from
the game, so there's no correct value to go back to - these are just what felt
right on a handheld.

## Custom artwork for the login screens

The login, character select and character creation screens all read their
backgrounds out of `Map001.wz`, which only exists in clients from v209 onward.
Against v83 data those lookups find nothing, and against later data they find
the wrong thing - v83's `back/13` is a mushroom house, v202's is a single pixel.
Every version obtainable is wrong in some way, so there is nothing to fall back
to. World select had the opposite problem: it drew two stacked city backdrops
from content years newer than the rest of the client.

So these four screens now use artwork of our own, in a small `Map001.nx` built
by `tools/make_assets.py`. Nothing in the client had to learn a new trick: a
background here is an ordinary NX animation - numbered bitmap children, each
with an `origin` and a `delay` - which is exactly how a video is expressible.

    Custom/LoginBg   80 frames, 10/sec   the login screen
    Custom/CharBg    48 frames, 6/sec    character select and creation
    Custom/WorldBg   one still           world select
    Custom/Logo      one still           the sign on the login screen

To rebuild it, with `login.mp4`, `character selection.mp4` and `LoginIcon.jpg`
in `~/Downloads` and ffmpeg on PATH:

    python tools/make_assets.py

then put the result next to the other `.nx` files. `MAKE_ASSETS_SRC`,
`MAKE_ASSETS_OUT` and `MAKE_ASSETS_FFMPEG` override those paths.

Three things about the format are worth knowing before changing `nxbuild.py`,
because each one fails quietly rather than loudly:

**Children must be sorted by name.** Reading a child is a binary search over the
parent's run of children, comparing raw bytes and then length. An unsorted run
does not merely slow a lookup down - it returns the wrong node or none at all,
while the parent still reports the right child count. That looks exactly like
missing artwork.

**A bitmap is a `uint32` compressed length followed by a raw LZ4 block** that
inflates to exactly `4 * width * height` bytes of BGRA. The reader takes the
decompressed size from the node's own width and height, so the stored length is
only used to find the next blob.

**The reader over-reads.** It asks for `4 * width * height` bytes from a blob
that is usually much smaller, so the file ends with padding - without it the
last (best-compressed) bitmap reads past the end of the file.

Frames are authored at 400x300 and stretched to 800x600 when drawn. Every frame
of a screen's animation ends up in the texture atlas for as long as that screen
is up, and at full size they would crowd out the rest of the UI. Playback is
`zigzag` - forward then backward - so the loop has no visible cut, which matters
for the login video because it is a camera pan and its last frame looks nothing
like its first.
