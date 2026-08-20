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
