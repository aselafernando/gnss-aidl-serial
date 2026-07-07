# Serial NMEA GNSS HAL (AIDL, APEX)

An **AIDL** implementation of the Android GNSS HAL
(`android.hardware.gnss`, declared at **V4** — accepted by **Android 17**,
whose compatibility matrix takes versions 2–7) for a serial / USB NMEA-0183
receiver, packaged as a self-contained **vendor APEX**
(`com.android.hardware.gnss.serial`). Developed and verified on an Android 17
AAOS tree (x86_64 emulator target).

## Quick start

The whole directory is a self-contained Soong module: clone it anywhere under
your AOSP tree, generate keys, add it to a product, build.

```sh
# 1. clone into your tree (<vendor> is any name you pick)
cd "$ANDROID_BUILD_TOP"
git clone https://github.com/aselafernando/gnss-aidl-serial.git device/<vendor>/gnss-aidl-serial

# 2. generate the signing keys (once; writes to keys/, which is git-ignored)
device/<vendor>/gnss-aidl-serial/generate-keys.sh

# 3. add to your product .mk and BoardConfig.mk
#    PRODUCT_PACKAGES        += com.android.hardware.gnss.serial
#    BOARD_VENDOR_SEPOLICY_DIRS += device/<vendor>/gnss-aidl-serial/sepolicy

# 4. build
m com.android.hardware.gnss.serial      # module only, or `m` for a full image
```

A ready-made emulator product and a `ueventd.rc` snippet are in `examples/`.
Details on each step follow below.

## Layout

```
gnss-aidl-serial/               # clone into device/<vendor>/gnss-aidl-serial
├── README.md
├── generate-keys.sh            # one-command signing-key generation -> keys/
├── Android.bp                  # apex + apex_key + certificate
├── apex_manifest.json          # name + version of the APEX
├── file_contexts               # SELinux labels for APEX payload
├── keys/                       # signing material (git-ignored; not in a clone)
├── serial/
│   ├── Android.bp              # cc_binary (the HAL service)
│   ├── gnss-serial.rc          # init service (bundled in APEX)
│   ├── gnss-serial.xml         # VINTF fragment (bundled in APEX)
│   ├── service.cpp             # main(): register IGnss/default
│   ├── Gnss.{h,cpp}            # IGnss: serial reader thread + dispatch
│   ├── SerialDevice.{h,cpp}    # tty open/termios/baud + UBX CFG-RATE
│   ├── NmeaParser.{h,cpp}      # NMEA-0183 -> GnssLocation / GnssSvInfo
│   ├── GnssConfiguration.{h,cpp}
│   ├── GnssPowerIndication.{h,cpp}
│   ├── GnssMeasurementInterface.{h,cpp}
│   ├── GnssDebug.{h,cpp}
│   ├── GnssAntennaInfo.{h,cpp}
│   ├── AGnss.{h,cpp}
│   ├── AGnssRil.{h,cpp}
│   └── GnssVisibilityControl.{h,cpp}
├── sepolicy/
│   ├── file_contexts           # /dev/tty* -> gps_device
│   ├── property_contexts       # persist.vendor.gps.* -> vendor_gps_prop
│   └── hal_gnss_serial.te      # extra allow rules for the serial path
└── examples/
    ├── ueventd.rc              # GPS tty DAC perms — append to the DEVICE
    │                           # tree's ueventd.rc (APEXes can't carry these)
    └── emulator/               # AAOS x86_64 emulator product (my_car_x86_64)
```

Note on tty permissions: SELinux labels come from `sepolicy/`, but the plain
DAC ownership of the GPS tty (`0660 gps system`) must be granted by the
*device tree's* ueventd.rc — ueventd does not read ueventd rules from APEXes.
Append the lines in `examples/ueventd.rc` to your device's ueventd file.

## Configuration

System properties, read at service start:

| Property | Meaning | Default |
| --- | --- | --- |
| `persist.vendor.gps.device` | explicit tty under `/dev` (e.g. `ttyO1`, `ttyUSB0`, `ttyACM0`, `ttyAMA0`); overrides USB matching | *unset* |
| `persist.vendor.gps.usb` | USB `vid:pid` used to locate the receiver by identity when `device` is unset — robust against USB enumeration order | `067b:2303` (PL2303 / GlobalSat BU-353) |
| `persist.vendor.gps.ttybaud` | 4800 / 9600 / 19200 / 38400 / 57600 / 115200 | `4800` (NMEA-0183 standard rate) |
| `persist.vendor.gps.max_rate` | fix interval: 1–65 s, or 250–65000 ms (UBX receivers) | `1000` ms |
| `persist.vendor.gps.time_sync` | steer system clock when off by > N s (20–60 typical); 0 = off | `0` |

The framework's `setPositionMode(minIntervalMs)` overrides `max_rate` at runtime
and is pushed to the receiver as a UBX `CFG-RATE` message.

Device resolution: if `persist.vendor.gps.device` is set it is used as-is. If it is
unset, `/sys/class/tty` is scanned for a tty whose parent USB device matches
the `persist.vendor.gps.usb` vid:pid, so a USB receiver is found regardless of
which `ttyUSB*`/`ttyACM*` index it enumerated as. If several identical
receivers are attached the lowest-sorting name is used (a warning is logged).

## Signing keys

The APEX is signed with two key pairs — an AVB pair for the filesystem image
and an X.509 certificate for the package container. `generate-keys.sh` creates
all four files under `keys/`:

```sh
./generate-keys.sh            # no-op if keys/ already populated
./generate-keys.sh --force    # regenerate from scratch
```

It needs only `openssl` and `python3` (both present in an AOSP checkout), so it
works before `lunch` and on any host; if `avbtool` is on `PATH` it is used for
the `.avbpubkey`, otherwise an equivalent generator produces the identical
format. `keys/` is git-ignored — the keys stay on your machine, and a fresh
clone must run this script before the first build. The paths are wired in
`Android.bp` (`apex_key` and `android_app_certificate`), so nothing else needs
touching.

These are self-signed developer keys, fine for `userdebug`/`eng` and for any
build where you control verified boot (the emulator, most single-board images).
A production `user` build should sign with your release keys instead.

## Building

```sh
PRODUCT_PACKAGES += com.android.hardware.gnss.serial
BOARD_VENDOR_SEPOLICY_DIRS += device/<vendor>/gnss-aidl-serial/sepolicy
```

Add those to your product (and `BoardConfig.mk` for the second line — see
`examples/`), then:

```sh
m com.android.hardware.gnss.serial      # module only
m                                       # or a full image with the HAL in it
```

## Replacing the device's existing GNSS HAL

Only one service may provide `android.hardware.gnss.IGnss/default`. If the
target already ships a GNSS HAL, it must not land in the image or the two
services race for the registration — the stock one typically wins, and this
HAL's process exits while `dumpsys`/apps silently keep using the old
implementation (on the emulator that means the *fake* virtual-device GPS, which
looks deceptively alive).

**This is handled automatically**: the APEX declares `overrides:` for the
known stock GNSS modules (goldfish/ranchu, AOSP example), so simply adding
`com.android.hardware.gnss.serial` to `PRODUCT_PACKAGES` excludes them from
the image — no edits to the goldfish or device trees needed.

If your tree ships a GNSS HAL under a different module name, add that name to
the `overrides:` list in `Android.bp` (find it with
`grep -rn gnss device/<vendor>/<board> --include='*.mk'`).

To verify on the running device: `adb shell ps -A | grep gnss` must show only
`android.hardware.gnss-service.serial`, and
`adb shell getprop init.svc.vendor.gnss-serial` must say `running`.

## Interface version

Builds against the frozen **V4** NDK backend (`android.hardware.gnss-V4-ndk`).
Android 17 trees carry frozen versions up to **V7**, and the framework
compatibility matrices accept a range — `2-4` at FCM level 202404, `2-6` at
202504, `2-7` at 202604/202704 — so a V4 declaration passes the VINTF check on
every current target while staying buildable on Android 16 trees too.

Two files pin the version and must stay in lockstep if you choose to bump it
(e.g. to V7; the build will surface any newly mandatory methods as pure-virtual
compile errors, each stubbable like the existing no-op extensions):

- `serial/Android.bp` → `android.hardware.gnss-V4-ndk`
- `serial/gnss-serial.xml` → `<version>4</version>`

To see what your tree offers and accepts:

```sh
ls hardware/interfaces/gnss/aidl/aidl_api/android.hardware.gnss/
grep -B2 -A6 'android.hardware.gnss<' \
    hardware/interfaces/compatibility_matrices/compatibility_matrix.*.xml
```

Notes on what V4 requires beyond the original V1/V2 surface, all handled here:
- `IGnssMeasurementInterface.setCallbackWithOptions` (V2+)
- `IAGnssRil.injectNiSuplMessageData` (V3+)
- `IGnss`, `IGnssCallback` and every other extension are unchanged from V2
  through V4.

## Scope / limitations

- Only the data a bare NMEA stream provides is reported: location, satellites in
  view, raw NMEA, and session status. Raw GNSS **measurements**, **navigation
  messages**, **PSDS/A-GNSS assistance**, **batching**, **geofencing**, and
  **antenna info** are not derivable from NMEA and are exposed as empty/no-op
  extensions (unsupported getters fail with `EX_UNSUPPORTED_OPERATION`; the
  non-nullable `IGnssDebug` and `IGnssAntennaInfo` return empty data).
- Vertical accuracy is not emitted (NMEA carries only HDOP); horizontal accuracy
  is approximated from GGA HDOP / GSA, matching the legacy behaviour.
- GPS **week-number rollover** is corrected automatically: receivers with old
  10-bit-week firmware (SiRF III / BU-353 era) report dates exactly N×1024
  weeks in the past; the parser shifts any date before 2025-01-01 forward in
  whole 1024-week periods. The floor constant (`kPlausibleFloorSec` in
  `NmeaParser.cpp`) is valid until ~2044.
- `gps_device` / serial paths in `sepolicy/file_contexts` are examples — set them
  to the actual node named by `persist.vendor.gps.device` on your board.
