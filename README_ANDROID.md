# HeavenClient for Android

An Android port of [HeavenClient](https://github.com/ryantpayton/MapleStory-Client),
the from-scratch MapleStory client, playable on a phone or a handheld against a
v83 server such as [Cosmic](https://github.com/P0nk/Cosmic) or HeavenMS.

Built and tested on an AYN Thor (Adreno, OpenGL ES 3.2), talking to Cosmic over
a LAN.

## No game data is included here, and none can be

This repository contains **code only**. The client needs MapleStory's `.nx`
data files, which are converted from Nexon's `.wz` archives - Nexon's artwork,
music and maps. Those are not mine to distribute and are not here. You will
need to produce them yourself from a client you have, using
[NoLifeWzToNx](https://github.com/ryantpayton/NoLifeWzToNx).

The 15 files the client expects are listed in `Util/NxFiles.h`. Note that
`UI.nx` must come from a *later* client than v83: v83's own `UI.wz` predates the
node layout the UI code resolves (`StatusBar3`, the `button:`/`layer:` naming),
and the client refuses it outright with `WRONG_UI_FILE`. Everything else is v83.

Put them, plus the `fonts` folder, in:

```
/sdcard/Android/data/org.heavenclient.android/files/HeavenClient/
```

## Building

Requires the Android SDK, NDK r27+, and **JDK 17** (Gradle 8.1.1 rejects 21).

```
cd android
./gradlew assembleDebug
```

`android/local.properties` needs `sdk.dir` with **forward slashes** -
Java `.properties` files treat `\U`, `\D` and `\A` as escapes.

## Configuration

`HeavenClient/Settings`, next to the data:

```
ServerIP = 192.168.1.71
ServerPort = 8484
Width = 800
Height = 600
```

`Width`/`Height` are read at startup by this port (upstream only applies them
from the in-game options menu). 800x600 is worth knowing about: every login and
character screen - `UILogin`, `UICharSelect`, `UIExplorerCreation` and the rest -
hardcodes an 800x600 design space and none of them adapt to a taller screen, so
at anything else the character and buttons sit visibly wrong.

## What the port changes

Platform work:

- **SDL2 replaces GLFW** for the window, GL context and input (`IO/Window_Android.cpp`).
  GLFW has no Android backend. Key codes stay GLFW values so existing config
  files and `IO/Keyboard.cpp` work untouched.
- **GLES2 instead of desktop GL** - `GL_QUADS` becomes two triangles,
  `GL_RED` becomes `GL_LUMINANCE`, `GL_BGRA` becomes `GL_BGRA_EXT`, and the
  shaders are rewritten as GLSL ES 1.00.
- **An on-screen keyboard**, tied to whether a text field has focus. Typed
  characters arrive as `SDL_TEXTINPUT` and bypass the keycode path, which would
  otherwise lose capitalisation - passwords are case sensitive.
- **The frame renders to an offscreen target** at the client's own resolution
  and is upscaled to the panel in one filtered blit, rather than every sprite
  being scaled individually with nearest sampling.
- **libnx and mbedtls dependencies removed**; asio is used for sockets as on
  desktop, and `randomGet64()` is replaced with `std::random_device`.

Bugs found while porting, several of which are not Android-specific:

- **`mediump` cannot address the 8192x8192 sprite atlas.** GLES2 mediump gives
  about 1024 steps across a texture coordinate, which lands roughly 8 pixels
  apart on that atlas - large art survived, all text was shredded. The texture
  coordinate and atlas size must be `highp`.
- **Texture uploads did not bind the atlas**, relying on a binding left over
  from init. Uploading into whatever happened to be bound caches an atlas entry
  whose pixels were never written, which draws as a blank rectangle.
- **The mob spawn packet's status block is 16 bytes, not 22.** Reading six
  bytes late put every monster's position and foothold at nonsense values, so
  spawns arrived and were drawn nowhere visible.
- **Dropped connections were invisible.** `receive()` only checked for an error
  when bytes were already waiting, and a dead link has none, so the session
  stayed "connected" indefinitely while the client sent input into nothing.
- **Dying did nothing.** `Char::State::DIED` exists and is read, but nothing in
  the codebase - or in the far newer upstream - ever set it. Reaching 0 HP now
  raises a prompt whose confirmation sends the revive request.
- **`Configuration::load()` truncated every value by one character** on
  LF-terminated files, and `save()` wrote the damage back, so settings decayed
  by a character per run. This affects every non-Windows platform.
- **`GraphicsGL::clear()` can never fire** - it compares a fraction against
  `80.0`, so the atlas is never trimmed and is only ever wiped when full.

Movement and knockback constants are tuned down from upstream's values, which
are that project's approximation of v83 rather than extracted from the game.

## Licence

AGPL-3.0, inherited from HeavenClient. See `LICENSE`. If you distribute a
binary, you must offer the corresponding source under the same terms.
