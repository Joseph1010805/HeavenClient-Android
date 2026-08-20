# HeavenClient for Android

An Android port of [HeavenClient](https://github.com/ryantpayton/MapleStory-Client),
the from-scratch MapleStory client, playable on a phone or a handheld against a
v83 server such as [Cosmic](https://github.com/P0nk/Cosmic) or HeavenMS.

Built and tested on an AYN Thor (Adreno, OpenGL ES 3.2) against Cosmic over a
LAN. The Switch port this descends from is
[lain3d/HeavenClientNX](https://github.com/lain3d/HeavenClientNX); its README is
kept here as `README_SWITCH.md`.

---

## No game data is included here, and none can be

This repository contains **code only**. The client needs MapleStory's `.nx`
data files, converted from Nexon's `.wz` archives - Nexon's artwork, music and
maps. Those are not mine to distribute and are not here. You must produce them
yourself, from a client you have.

Everything below assumes you understand that and are supplying your own data.

---

# Build tutorial

## 1. What you need

| | |
|---|---|
| Android Studio | for the SDK, or the command line tools |
| Android NDK | **r27** (`27.3.13750724`) - set in `android/app/build.gradle` |
| **JDK 17** | Gradle 8.1.1 **rejects JDK 21**. This bites people |
| Git | with submodule support |
| A device | arm64, Android 7.0+ (minSdk 24), OpenGL ES 2.0+ |

## 2. Clone with submodules

SDL2, FreeType, asio and OpenAL are submodules. A plain `git clone` gives you
empty directories and a confusing pile of CMake errors:

```bash
git clone --recursive https://github.com/Joseph1010805/HeavenClient-Android.git
cd HeavenClient-Android
```

Already cloned without `--recursive`?

```bash
git submodule update --init --recursive
```

## 3. Point Gradle at your SDK

Create `android/local.properties`:

```properties
sdk.dir=C:/Users/YourName/AppData/Local/Android/Sdk
```

**Use forward slashes even on Windows.** `.properties` files treat backslashes
as escapes, so `C:\Users\...` silently becomes nonsense - `\U`, `\D` and `\A`
are eaten - and Gradle reports a missing SDK at a path you can plainly see
exists.

On macOS or Linux it is usually:

```properties
sdk.dir=/home/yourname/Android/Sdk
```

## 4. Build

```bash
cd android
./gradlew assembleDebug
```

The APK lands at `android/app/build/outputs/apk/debug/app-debug.apk`.

If Gradle complains about the Java version, point it at 17 explicitly:

```bash
JAVA_HOME=/path/to/jdk-17 ./gradlew assembleDebug
```

## 5. Install

```bash
adb install -r android/app/build/outputs/apk/debug/app-debug.apk
```

Launch it once and let it fail on the missing data - that first run creates the
directory the data goes into, with the right ownership. This matters; see below.

---

# Supplying the game data

## 6. Convert your WZ files to NX

Use [NoLifeWzToNx](https://github.com/ryantpayton/NoLifeWzToNx). It is a Visual
Studio project that predates current toolchains, so expect three fixes:

1. `namespace sys = std::experimental::filesystem;` - modern MSVC only ships
   `std::filesystem`. Switch it, and set the project to C++17.
2. `<codecvt>` is deprecated in C++17 and the project treats warnings as
   errors. Define `_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS`.
3. The bundled `libsquish.lib` is a link-time-code-generation binary from a much
   older compiler and will not link (`C1047`). The sources ship alongside it -
   add `includes/libsquish/*.cpp` to the project and drop the `.lib`.

Then, per file:

```bash
NoLifeWzToNx.exe -c Character.wz
```

`-c` (client mode) matters - the server variant produces files this client
cannot use.

## 7. Which versions to convert

This is the part that is easy to get wrong.

| File | Version |
|---|---|
| Everything except `UI.nx` | **v83** |
| `UI.nx` | **a later client** (this build uses v178) |

v83's own `UI.wz` predates the node layout the UI code resolves - `StatusBar3`,
and the `button:` / `layer:` naming - and the client rejects it outright with
`WRONG_UI_FILE`. `Base.nx` is also required even though it is tiny: `load_all()`
checks for it before loading anything else.

`Map001.nx` is **not** required by this build. Upstream wants it for the login
backdrop; here `nx.cpp` falls back to `Map.nx`, which has its own
`Back/login.img`.

See `Util/NxFiles.h` for the definitive list.

## 8. Get the data onto the device

`adb push` cannot write into the app's data directory directly - it tries to
`fchown` the file and is not permitted to. Stage the files elsewhere on the same
volume and move them, which is a rename rather than a copy:

```bash
DIR=/storage/emulated/0/Android/data/org.heavenclient.android/files/HeavenClient
STAGE=/storage/emulated/0/Download/nxstage

adb shell "mkdir -p $STAGE"
for f in *.nx; do
    adb push "$f" "$STAGE/"
    adb shell "mv $STAGE/$(basename $f) $DIR/ && chmod 644 $DIR/$(basename $f)"
done
adb push fonts "$STAGE/" && adb shell "mv $STAGE/fonts $DIR/ && chmod -R a+rX $DIR/fonts"
```

The `chmod` is required: a file moved in this way is owned by `shell`, and
without it the app - which is "other" on that file - cannot read it.

**Two failure modes worth knowing**, because both look like something else:

- `adb push` will report small files as copied into that directory when nothing
  actually lands. Verify with `ls`, not with adb's output.
- If the app has never been launched, the directory may not exist, or may exist
  without group write. The app creates it `0775` on first run.

Don't forget the `fonts` folder - `FT_New_Face` fails silently, so missing fonts
present as text simply not appearing.

## 9. Settings

`HeavenClient/Settings`, alongside the data:

```
ServerIP = 192.168.1.71
ServerPort = 8484
Width = 800
Height = 600
```

`Width`/`Height` are read at startup by this port; upstream only applies them
from the in-game options menu. **800x600 is strongly recommended**: every login
and character screen - `UILogin`, `UICharSelect`, `UIExplorerCreation` and the
rest - hardcodes an 800x600 design space and none of them adapt, so at any other
resolution the character and buttons sit visibly wrong. The frame is upscaled to
your panel regardless.

## 10. Server notes

Against Cosmic, two settings need changing or nothing works from a phone:

```yaml
HOST: 192.168.1.71        # not 127.0.0.1
LANHOST: 192.168.1.71     # not 127.0.0.1
```

These are the addresses handed to the client when it selects a character. Left
at loopback, the phone tries to connect to *itself* and the Start button appears
to do nothing at all.

Also worth knowing: passwords must be **5+ characters**. `UILogin` rejects
anything shorter locally and never sends a packet, so a short password looks
exactly like a rejected login.

---

# What the port changes

Platform work:

- **SDL2 replaces GLFW** for the window, GL context and input
  (`IO/Window_Android.cpp`). GLFW has no Android backend. Key codes stay GLFW
  values so existing config files and `IO/Keyboard.cpp` work untouched.
- **GLES2 instead of desktop GL** - `GL_QUADS` becomes two triangles, `GL_RED`
  becomes `GL_LUMINANCE`, `GL_BGRA` becomes `GL_BGRA_EXT`, and the shaders are
  rewritten as GLSL ES 1.00.
- **An on-screen keyboard**, tied to whether a text field has focus. Typed
  characters arrive as `SDL_TEXTINPUT` and bypass the keycode path, which would
  otherwise lose capitalisation - passwords are case sensitive.
- **The frame renders to an offscreen target** at the client's own resolution
  and is upscaled in one filtered blit, rather than every sprite being scaled
  individually with nearest sampling.
- **libnx and mbedtls dependencies removed**; asio is used for sockets as on
  desktop, and `randomGet64()` is replaced with `std::random_device`.

Bugs found while porting. Several are not Android-specific and affect the
desktop client too:

- **`mediump` cannot address the 8192x8192 sprite atlas.** GLES2 mediump gives
  about 1024 steps across a texture coordinate, landing roughly 8 pixels apart
  on that atlas - large art survived, all text was shredded. The texture
  coordinate and atlas size must be `highp`.
- **Texture uploads did not bind the atlas**, relying on a binding left from
  init. Uploading into whatever happened to be bound caches an atlas entry whose
  pixels were never written, which draws as a blank rectangle.
- **The mob spawn packet's status block is 16 bytes, not 22.** Reading six bytes
  late put every monster's position and foothold at nonsense values, so spawns
  arrived and were drawn nowhere visible.
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

Movement and knockback constants are tuned down from upstream's, which are that
project's approximation of v83 rather than values extracted from the game.

---

# Licence

AGPL-3.0, inherited from HeavenClient. See `LICENSE`. If you distribute a
binary, you must offer the corresponding source under the same terms.
