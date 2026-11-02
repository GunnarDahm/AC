# Android port (test build)

The game runs on Android through SDL2, with [gl4es](https://github.com/ptitSeb/gl4es)
translating its desktop OpenGL calls to OpenGL ES 2 and [OpenAL Soft](https://github.com/kcat/openal-soft)
playing sound through OpenSL ES. USB and Bluetooth game controllers work through SDL's
GameController support.

## Getting a build

The latest build of `master` is always at the
[android-latest release](https://github.com/GunnarDahm/AC/releases/tag/android-latest):
open it on the phone and tap `assaultcube-android.apk` to download and install.

Every push (including pull requests) also builds an APK on GitHub Actions; it is attached
to the run as the `assaultcube-android` artifact (a zip, downloadable from a browser).

To build locally you need JDK 17, the Android SDK (platform 34) and NDK 27.2.12479018:

```sh
android/fetch-deps.sh
cd android && ./gradlew assembleRelease
```

`fetch-deps.sh` downloads pinned versions of SDL2, SDL2_image, gl4es, OpenAL Soft, libogg and
libvorbis into `android/deps/`, and applies the fixes in `android/patches/<dependency>/`.

The APK lands in `android/app/build/outputs/apk/release/`. It is signed with the
debug key, so it can be sideloaded but not uploaded to the Play Store.

## Installing and testing

Requires Android 7.0 (API 24) or newer.

```sh
adb install -r app-release.apk
adb logcat -s assaultcube SDL GL4ES openal AndroidRuntime
```

On first launch the app unpacks the game data (`config/`, `packages/`, `bot/`, `mods/`, about
60 MB) into its storage, which takes a few seconds. Later launches go straight into the game.
Settings, demos and screenshots are kept in a separate profile directory (`home/` next to the
unpacked data), so updating the app keeps them.

### Controller

Pair the controller in Android's Bluetooth settings first; it can also be connected
or disconnected while the game is running. The same controller support works in the desktop
builds.

| Control | In game | In menus and the console |
|---|---|---|
| Left stick | move | navigate |
| Right stick | look | |
| RT | attack | |
| LT | special action (sniper: scope, others: throw grenade) | |
| A | jump | select |
| B | crouch | back |
| X | reload | |
| Y | previous weapon | |
| RB / LB | next / previous weapon | page down / up |
| D-pad | up: primary, left: pistol, right: grenades, down: drop flag | navigate (left/right change sliders) |
| Right stick click | knife attack | |
| Left stick click | toggle map | |
| Back | scoreboard (hold) | |
| Start | main menu | back |

Buttons are bindable keys named `PAD_A`, `PAD_B`, `PAD_X`, `PAD_Y`, `PAD_LB`, `PAD_RB`,
`PAD_LT`, `PAD_RT`, `PAD_LSTICK`, `PAD_RSTICK`, `PAD_UP`, `PAD_DOWN`, `PAD_LEFT`, `PAD_RIGHT`,
`PAD_BACK`, `PAD_START` and `PAD_GUIDE` (see `config/keymap.cfg`; the defaults are in
`config/resetbinds.cfg`), so they can be rebound in the key binding menu or with `bind`.
Tunables: `paddeadzone`, `padyawspeed`, `padpitchspeed`, `padlookcurve`, `padinvert`,
`padtriggerthreshold`, `padrepeatdelay`, `padrepeatrate`, and `padmenubuttons 0` to use your
own binds in menus too. Controllers without a built-in SDL mapping can be added to
`config/gamecontrollerdb.txt` (the format of [SDL_GameControllerDB](https://github.com/mdqinc/SDL_GameControllerDB)).

### Touch

Tap a menu item to select it, drag up or down in a menu to scroll. In game, dragging a finger
turns the view (speed: `touchlookspeed`, in percent of the mouse sensitivity), a tap fires once
or respawns, and putting a second finger down while the first one is on the screen holds fire.
There are no on-screen move buttons yet, so playing needs a controller or keyboard.
The Android back button acts as Escape (opens and closes menus). While the console is open, a
tap brings the on-screen keyboard back.

### Known limitations

- OpenGL ES can't read back the depth buffer, so the crosshair target is found by tracing the
  view direction against the map geometry (walls, floors, ceilings) instead. Mapmodels such as
  crates and barrels don't stop that trace, so a shot aimed at one carries on to whatever is
  behind it, where on desktop it would end at the model.
- The application asks for no storage permission, so maps and mods can only be added through
  the in-game download (or `adb push` into the profile directory).
