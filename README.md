# Wireless Sofle (PandaKB) with ZMK + DYA Studio

🇬🇧 English · 🇧🇷 [Português](README.pt-BR.md)

Firmware for my **wireless Sofle RGB MX** (PandaKB board, nRF52840 controllers), built on
[ZMK](https://zmk.dev) and editable live from the browser with
[DYA Studio](https://studio.dya.cormoran.works/).

What this repository does differently from a plain `zmk-config`:

- it runs on **cormoran's ZMK fork (`main+dya`, Zephyr 4.1)**, which enables DYA Studio
  (keymap, macros, combos and rotary encoders editable in the browser, no rebuild needed);
- it ships a **custom status screen for the vertically mounted OLED** (Bluetooth, charging bolt,
  battery icon with level, active layer and modifiers), written for LVGL 9 because
  `zmk-nice-oled` does not build on Zephyr 4.1;
- it keeps, in one place, a record of **everything that went wrong while building the keyboard**
  and how it was fixed (see [Hardware pitfalls](#hardware-pitfalls)).

> Personal project, tested only on my own keyboard. The ZMK `main+dya` fork is experimental and
> `west.yml` points to branches, not pinned commits, so a new build may pull different versions.
> **Always keep the `.uf2` files that work.**

## Contents

- [Quick start](#quick-start)
- [How it works](#how-it-works)
- [Customizing](#customizing)
- [Troubleshooting](#troubleshooting)
- [Hardware](#hardware)
- [Branches](#branches)
- [Keymap layers](#keymap-layers)
- [Flashing](#flashing)
- [DYA Studio](#dya-studio)
- [OLED screen](#oled-screen)
- [Building](#building)
- [Hardware pitfalls](#hardware-pitfalls)
- [Zephyr 4.1 migration notes](#zephyr-41-migration-notes)
- [Known limitations](#known-limitations)
- [Credits](#credits)

## Quick start

Same keyboard as mine (PandaKB Sofle RGB MX with nRF52840 controllers)?

1. **Get the firmware.** On GitHub open *Actions*, pick the latest green run of the `dya-tela`
   branch and download the `firmware` artifact. It holds three files: `settings_reset_dya.uf2`,
   `Sofle_left_oled_dya.uf2` and `Sofle_right_oled_dya.uf2`.
2. **Flash both halves** following [Flashing](#flashing) (always `settings_reset` first).
3. **Pair** the keyboard with your computer over Bluetooth, or just plug the left half in by USB.
4. **Customize** it live at <https://studio.dya.cormoran.works/> (see [DYA Studio](#dya-studio)).

Different hardware? Read [How it works](#how-it-works) and [Customizing](#customizing) first: the
pin mapping lives in the shield files and will need adjusting.

## How it works

```
 computer ⇄ USB or Bluetooth ⇄ LEFT half (central) ⇄ Bluetooth ⇄ RIGHT half (peripheral)
                                  │  keymap, layers, macros, combos, DYA Studio
```

- Each half scans its own keys. The **right half only reports key positions** over Bluetooth.
- The **left half decides what every key means** (layers, mod-taps, macros) and sends the result
  to the computer as a normal keyboard.
- Consequences: only the left half connects to the computer and to DYA Studio; the right half
  cannot be used as a standalone keyboard; the right OLED does not know the active layer.
- Keymap edits made in DYA Studio are **stored on the keyboard**, not in this repository. The
  `.keymap` file is only the starting point after a `settings_reset`.

### Repository map

| Path | What it is |
|---|---|
| `config/Sofle.keymap` | layers, key bindings and dial behaviors (the part you edit most) |
| `config/Sofle.conf` | settings for both halves: Bluetooth, sleep, RGB defaults, display memory |
| `config/west.yml` | which ZMK, Zephyr and DYA modules get downloaded |
| `build.yaml` | which firmwares CI builds (left, right, `settings_reset`) |
| `boards/shields/Sofle/` | the shield: pins (`*.overlay`), OLED and LEDs (`Sofle.dtsi`), per-half options (`Sofle_L.conf`, `Sofle_R.conf`) |
| `src/status_screen.c`, `Kconfig`, `CMakeLists.txt`, `zephyr/module.yml` | the custom OLED screen, packaged as a Zephyr module |
| `.github/workflows/build.yml` | the CI build |

The other folders under `boards/shields/` (`Nice_view_config`, `Sofle_dongle`,
`eyeslash_dongle_config`) are leftovers from the template this repo started from and are not used.

### Glossary

| Term | Meaning |
|---|---|
| **ZMK** | open-source firmware for wireless keyboards, built on Zephyr |
| **Zephyr** | the real-time operating system ZMK runs on |
| **central / peripheral** | the half that talks to the computer / the half that only reports keys |
| **shield** | ZMK's description of a keyboard PCB (pins, matrix, display, LEDs) |
| **`west`** | Zephyr's tool that downloads the sources listed in `west.yml` |
| **UF2** | firmware file you copy onto the controller's USB disk |
| **`settings_reset`** | a tiny firmware that erases pairings and saved settings |
| **DYA Studio** | browser app to edit the keymap, macros, combos and dials live |
| **LVGL** | the graphics library used for the OLED screen |

## Customizing

| I want to… | Do this |
|---|---|
| change a key for now | DYA Studio → Keymap (stored on the keyboard) |
| change a key for good | edit `config/Sofle.keymap`, push, flash the new build (then `settings_reset`) |
| change what a dial does | DYA Studio → *Rotary Encoder Configuration*, or edit `rsr_vol` / `rsr_pg` in the keymap |
| change idle and sleep times | `CONFIG_ZMK_IDLE_TIMEOUT` (300000 ms, 5 minutes) and `CONFIG_ZMK_IDLE_SLEEP_TIMEOUT` (900000 ms) in `config/Sofle.conf` |
| change the RGB defaults | `CONFIG_ZMK_RGB_UNDERGLOW_*` in `config/Sofle.conf` |
| change the Bluetooth name | `CONFIG_ZMK_KEYBOARD_NAME` in `config/Sofle.conf` |
| change the OLED screen | `src/status_screen.c`; flip `CONFIG_SOFLE_STATUS_SCREEN_ROTATE_CW` if it is upside down |
| use another controller wiring | edit the `pro_micro` pins in `boards/shields/Sofle/Sofle_L.overlay` and `Sofle_R.overlay` |

## Troubleshooting

| Symptom | Likely cause and fix |
|---|---|
| The right half does not type | It is a peripheral: check the right OLED says `OK` next to the Bluetooth icon. Run `settings_reset` on **both** halves, reflash, re-pair the computer, and wait a few minutes |
| Computer says "Connected" but nothing types | stale keys after a reset: remove the keyboard from Bluetooth and pair again |
| LEDs and OLED are both dark | `ext_power` was toggled off (LOWER layer, first key of the Tab row, or ADJUST). Press that key again, or reflash |
| OLED alone is dark | the 4 OLED jumpers are on the wrong face, or a display wire is loose (see [Hardware pitfalls](#hardware-pitfalls)) |
| Screen blinks and shows static | LVGL ran out of memory and the keyboard is rebooting: keep the two memory settings from [OLED screen](#oled-screen) |
| Caps Lock stuck on | hold RAISE and press G |
| DYA Studio will not connect | use a Chromium browser over USB, with a data cable straight into the computer (no hub) |
| One column or row is dead | cold solder joint on the header pin of the PCB base (where the controller plugs in); re-solder it first, then suspect the controller |

## Hardware

| Part | Details |
|---|---|
| Board | [PandaKB Sofle RGB MX](https://pandakb.com/guides/sofle-rgb-mx-build-guide) (hot-swap, **reversible** PCB) |
| Controllers | **SuperMini nRF52840** clones (nice!nano UF2 bootloader). ZMK uses the `nice_nano` board |
| Display | 128×32 SSD1306 OLED (I²C), **mounted vertically** on the board |
| LEDs | 72 × SK6812MINI-E (36 per half: 1 indicator, 6 underglow, 29 per-key) |
| Encoders | 2 × EC11 (left and right dial) |
| Power | One LiPo per half wired to B+/B- (the board's CN1 socket did not work on mine, see below) |
| Connection | Bluetooth; the halves talk to each other over BLE (the TRRS cable is unused in ZMK) |

The **left half is the central** (it talks to the computer and to DYA Studio) and the **right half
is a peripheral**. A ZMK peripheral does not show up as a keyboard over USB.

## Branches

| Branch | Purpose |
|---|---|
| `main` | original state of the repository |
| `pandakb-shield` | **ZMK v0.3** firmware (Zephyr 3.5) with the PandaKB shield and the `nice_oled` screen. The safe fallback |
| `dya-teste` | first migration to the `main+dya` fork (uses ZMK's built-in screen) |
| **`dya-tela`** | **the branch in use**: everything from `dya-teste` + configurable dials + custom screen |

## Keymap layers

The keymap lives in [`config/Sofle.keymap`](config/Sofle.keymap).

```
Left thumb  (outer → inner):  GUI  ALT  CTRL  LOWER  ENTER
Right thumb (inner → outer):  SPACE  RAISE  CTRL  ALT  GUI
```

| Layer | How to reach it | What it is for |
|---|---|---|
| 0 `base` | — | letters, numbers, modifiers |
| 1 `lower` | hold **LOWER** | `F1`–`F12`, numbers, symbols (`! @ # $ % ^ & * ( )`), `[ ] { } = - + \ ;` |
| 2 `raise` | hold **RAISE** | arrows, `Del`, `PgUp`/`PgDn`, copy/paste/undo, **`Caps Lock` (RAISE + G)**, Bluetooth profiles |
| 3 `adjust` | hold **LOWER + RAISE** | BT profiles, `BT_CLR`, RGB power and effects |

The dials default to **volume** (left) and **PgUp/PgDn** (right) on every layer, and can be
changed per layer in DYA Studio (Keymap tab → *Rotary Encoder Configuration*).

> Careful with `BT_CLR` (top-left key of the RAISE layer): it erases the Bluetooth pairings.
> Also with `&ext_power EP_TOG`: it sits on the **LOWER layer, first key of the Tab row**, and on
> ADJUST. It cuts power to the LEDs and the OLED, and
> **the state is saved on the keyboard** (unplugging USB does not bring it back; press the key again).

### LED colors

LEDs are changed on the **ADJUST** layer (hold **LOWER + RAISE**):

```
Esc row:   [Esc]       Q        W        E         R         T
           EXT_POWER   Hue-     Hue+     Sat-      Sat+      Effect
Tab row:   [Tab]       A        S        D
           (none)      Bright-  Bright+  RGB on/off
```

The firmware starts on effect 3 (a rotating rainbow), where the hue keys have nothing to change.
**Press `T` once to reach effect 0 (solid color) first**, then use `Q`/`W` for the color.
Effects: 0 solid, 1 breathing, 2 spectrum, 3 swirl. Settings are saved on each half, so do it on both.

## Flashing

`.uf2` files come from GitHub Actions (the `firmware` artifact) or from a
[local build](#building).

For each half:

1. USB plugged in, **double-tap the reset button**. A disk (like `NICENANO`) appears.
2. Copy `settings_reset_dya.uf2`. The board reboots by itself.
3. Double-tap reset again.
4. Copy `Sofle_left_oled_dya.uf2` (left) or `Sofle_right_oled_dya.uf2` (right).

Rules that save time:

- **Run `settings_reset` on both halves** when you change firmware or swap a controller.
  Otherwise the left half keeps the pairing of the old right controller and will not talk to the
  new one.
- On the computer, remove the keyboard from Bluetooth and pair it again after a `settings_reset`.
  BlueZ keeps showing "Connected" with stale keys and no keyboard input.
- The first connection between the halves can **take a few minutes**.
- `settings_reset` also erases whatever you edited in DYA Studio (that is stored on the keyboard
  and never flows back into the `.keymap`).
- Mounting the disk on Linux without sudo: `udisksctl mount -b /dev/sdX` (check with `lsblk`).

## DYA Studio

Open <https://studio.dya.cormoran.works/> in a **Chromium-based** browser and connect the left
half.

- **USB** is the reliable path. On Linux the serial port must be accessible to your user (a `udev`
  rule with `uaccess` for `1d50:615e`).
- On my machine it worked in **Helium**. In Brave the connection was cancelled and the site said
  *"BLE Not Supported"*. Firefox has no Web Bluetooth.
- **Bluetooth** on Linux needs `chrome://flags/#enable-experimental-web-platform-features`, and a
  keyboard already paired as a keyboard may not show up in the device list.
- If you see *"Runtime sensor rotation subsystem is not available"*, the
  `zmk-behavior-runtime-sensor-rotate` module is missing (it is already in this `west.yml`).
- The site's `OS Layout` is visual only; it does not change what the keyboard sends.

The central uses `CONFIG_ZMK_STUDIO_LOCKING=n`, so no unlock key is needed.

## OLED screen

[`src/status_screen.c`](src/status_screen.c) draws a 32×128 screen on an LVGL 9 *canvas* and
rotates it 90° for the vertically mounted OLED.

| | Left (central) | Right (peripheral) |
|---|---|---|
| Row 1 | `USB`, or Bluetooth icon + profile (`1*` connected, `1-` disconnected) | Bluetooth icon + `OK`/`--` (link to the left half) |
| Bolt | USB power present | USB power present |
| Battery | icon with level + `%` | icon with level + `%` |
| Layer | name (4 letters: `BASE`, `LOWE`, `RAIS`, `ADJU`) | — |
| Modifiers | `CTL SFT ALT GUI`, filled box = held | same, from the **key positions** of its own half |

- If the screen comes out upside down, flip `CONFIG_SOFLE_STATUS_SCREEN_ROTATE_CW`
  (in [`Kconfig`](Kconfig)).
- On the right half the positions are `49` (RShift), `57` (RCtrl), `58` (RAlt) and `59` (RGui),
  valid for the 60-position keymap. If you change the layout, change the numbers in
  `periph_track()`.
- **Memory:** the default LVGL pool (2 KB) and display thread stack (3 KB) are not enough to draw
  with a canvas. `config/Sofle.conf` sets `CONFIG_LV_Z_MEM_POOL_SIZE=16384` and
  `CONFIG_ZMK_DISPLAY_DEDICATED_THREAD_STACK_SIZE=6144`. Without them the keyboard **reboots in a
  loop** (the screen blinks and shows static), because LVGL asserts are disabled and running out
  of memory becomes a null pointer.

## Building

### GitHub Actions

A *push* triggers the workflow ([`.github/workflows/build.yml`](.github/workflows/build.yml)),
which uses ZMK's official workflow. Firmware is published in the `firmware` artifact.

### Locally, with Docker (nothing gets uploaded)

It uses the same image as Actions (`zmkfirmware/zmk-build-arm:stable`). The `west` workspace is
about 3 GB and lives outside this repository.

```bash
mkdir -p ~/zmk-build && cd ~/zmk-build
cp -r /path/to/repo/config ./config

# 1) fetch ZMK, Zephyr and modules (once)
docker run --rm -u $(id -u):$(id -g) -e HOME=/tmp \
  -v ~/zmk-build:/ws zmkfirmware/zmk-build-arm:stable bash -c \
  'cd /ws && west init -l config && west update --fetch-opt=--filter=tree:0 -o=--depth=1'

# 2) build (L = left with Studio, R = right)
docker run --rm -u $(id -u):$(id -g) -e HOME=/tmp \
  -v ~/zmk-build:/ws -v /path/to/repo:/repo zmkfirmware/zmk-build-arm:stable bash -c \
  'cd /ws && west zephyr-export && west build -s zmk/app -d build/L -b "nice_nano//zmk" \
     -S studio-rpc-usb-uart -- -DSHIELD=Sofle_L -DZMK_CONFIG=/ws/config -DZMK_EXTRA_MODULES=/repo'
# result: build/L/zephyr/zmk.uf2
```

For the right half, replace `L` with `R` and drop `-S studio-rpc-usb-uart`.
`west zephyr-export` has to run in every `docker run`, because the container's `HOME=/tmp`
disappears.

This repository is a **Zephyr module** ([`zephyr/module.yml`](zephyr/module.yml)): the root
`CMakeLists.txt` and `Kconfig` build the custom screen, and `boards/shields/Sofle` defines the
shield.

## Hardware pitfalls

All of this was found the hard way, and it is what cost the most time.

**The PCB is reversible: always check you are on the right face.**
- The **4 OLED jumpers** go on the **top face** (the same one as the switch sockets), just below
  the controller footprint. Done on the wrong side, the OLED stays dark.
- Every LED has a footprint on both faces. An LED soldered on the wrong face and then removed can
  rip out the trace. The marked corner of the LED must match the mark on the footprint.

**LED chain.** The order is the numbering from PandaKB's guide (1 = indicator near the TRRS, 2–7
underglow, 8–36 keys). If the chain stops at LED *N*, suspect the **last lit LED (N) first**: a
dead or badly soldered `DOUT`, and only then the first dark one (N+1). Repairs that worked:
replacing LED *N*; a thin wire from `DOUT` of *N* to `DIN` of *N+1*.

**Dead rows, columns or a "dead" controller: check the solder on the PCB first, then the controller.**
For days I blamed the controllers, then found **badly soldered header pins on the PCB itself**, the
base where the controller plugs in. Both can be at fault (the controllers are cheap clones), and I
could not tell them apart, so treat it as a sum of causes.
- a pin with a **cold joint / solder that did not flow down** (or a bent leg) gives a **dead row or
  column**: the thumb row on `pro_micro 9` (P1.06) and the `0 P ; /` column on `pro_micro 18`
  (P1.15, label `115`). Re-soldering fixed it;
- swapping controllers can look like a fix because it re-seats the contacts, so it does not prove
  the controller is faulty (or healthy); reflow the base pins first, since it is the cheaper test;
- a controller that seems not to power the OLED or the LEDs may be a weak joint on the power pins
  of the base, or a bad controller (not measured);
- careful: the pin label (`009`) is **not** the `pro_micro` number (`9`).

**Battery.** The **CN1** socket (under the controller) passed no power on my board. Wiring the
battery straight to the **B+/B- pads on the PCB edge** fixed it. Do **not** jumper to `RAW`: it is
the 5 V USB input and bypasses the charger. Check polarity with a multimeter and insulate with
Kapton.

**Testing the right half's matrix.** A peripheral does not become a USB keyboard. To test its
matrix, flash a temporary build as central (`-DCONFIG_ZMK_SPLIT_ROLE_CENTRAL=y`) and then reflash
the normal firmware.

**Enumeration symptoms.** An `lsusb` that lists `1d50:615e` **with no interface at all** (and no
`/dev/input` or `ttyACM0`) means enumeration failed, usually a USB hub. Use a direct motherboard
port and a data cable.

## Zephyr 4.1 migration notes

Moving from ZMK v0.3 to the `main+dya` fork broke these things; here is how each was fixed:

| Symptom | Cause | Fix |
|---|---|---|
| `Aborting due to Kconfig warnings` | `CONFIG_WS2812_STRIP` no longer exists | delete the lines (the driver comes from the devicetree) |
| same error, on `BATTERY_LEVEL_FETCHING`/`PROXY` | central-only options assigned to the peripheral | remove them from `Sofle_R.conf` |
| `LV_IMG_CF_*` / `lv_draw_img_dsc_t` undeclared | `zmk-nice-oled` targets LVGL 8; Zephyr 4.1 uses LVGL 9 | drop the module and use the built-in or the custom screen |
| OLED shows only blocks | `CONFIG_LV_FONT_DEFAULT_MONTSERRAT_26` inherited from nice_view (26 px font on a 32 px display) | remove both font lines |
| board name | `nice_nano_v2` became `nice_nano//zmk` | update `build.yaml` |
| `zmk/display.h: No such file` when building the module | ZMK include paths missing | `zephyr_library_include_directories(${CMAKE_SOURCE_DIR}/include)` |
| `lv_draw_triangle_dsc_t` unknown | header not pulled in by `lvgl.h`, and fields are `bg_color`/`bg_opa` | `#include "draw/lv_draw_triangle.h"` |

## Known limitations

- No `nice_oled` animations (cat, vim). The custom screen shows only the essentials.
- Layer name is 4 letters on the screen.
- Right-half modifiers come from physical key positions, not the keymap (`&mt` and similar do not
  light up).
- The `main+dya` fork is experimental and unpinned.
- RGB, OLED and the PandaKB shield were only tested on my hardware.

## Credits

- [ZMK Firmware](https://github.com/zmkfirmware/zmk) (MIT) and the
  [`cormoran/zmk`](https://github.com/cormoran/zmk) fork with the
  [DYA Studio](https://github.com/cormoran/dya-studio) modules.
- Sofle shield: Ryan Cross and ZMK contributors; board adjustments and build guide from
  [PandaKB](https://pandakb.com/guides/sofle-rgb-mx-build-guide).
- This repository is released under the [MIT License](LICENSE). Files that came from
  upstream projects (such as the Sofle shield under `boards/shields/Sofle`) keep their own
  headers; check each upstream project's license before reusing its code.
