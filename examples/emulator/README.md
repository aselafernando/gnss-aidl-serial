# AAOS x86_64 emulator integration example

`<vendor>` below is the directory you cloned `gnss-aidl-serial` into — e.g.
if it lives at `device/acme/gnss-aidl-serial`, `<vendor>` is `acme`. Copy
`my_car_x86_64/` to `device/<vendor>/my_car_x86_64/` (alongside the cloned
`gnss-aidl-serial`), then:

1. Set `<vendor>` in `my_car_x86_64/BoardConfig.mk` to your actual directory,
   and verify the `include` path there plus the release config in
   `COMMON_LUNCH_CHOICES` match your tree (comments in the files say how).
2. The stock emulator GNSS HAL (`android.hardware.gnss-service.ranchu`) is
   excluded automatically via the APEX's `overrides:` list, so there is only
   one `IGnss/default`. (If your goldfish tree names it differently, add that
   name to `overrides:` in the top-level `Android.bp`.)

   One goldfish edit is still required: append the GPS tty permission lines
   from `../ueventd.rc` to `device/generic/goldfish/init/ueventd.rc`
   (installed as `/vendor/etc/ueventd.rc`), unless it already makes `ttyS*`
   readable. Commit that change locally in the goldfish repo.
3. `lunch my_car_x86_64-trunk_staging-userdebug && m`
4. Launch with the host GPS forwarded, e.g.
   `emulator -qemu -serial /dev/ttyUSB0` (Linux) or `-serial COM5` (Windows).
   Confirm which guest ttyS the port landed on via
   `cat /proc/tty/driver/serial` and fix `persist.vendor.gps.device`
   accordingly (`setprop` at runtime, then bake into `my_car_x86_64.mk`).

## Why three files?

Each file is read by a different part of the build and only found under its
exact name:

- `AndroidProducts.mk` — product *discovery*. The build globs
  `device/*/*/AndroidProducts.mk` to learn what products exist; without it
  `lunch` has never heard of `my_car_x86_64`, no matter what other files say.
- `my_car_x86_64.mk` — the product definition (`PRODUCT_*` variables),
  parsed during product configuration.
- `BoardConfig.mk` — board/soong configuration (`BOARD_*`, `TARGET_*`),
  parsed in a separate earlier phase via
  `device/*/$(TARGET_DEVICE)/BoardConfig.mk`. `BOARD_*` variables assigned in
  a product `.mk` are silently ignored, which is why
  `BOARD_VENDOR_SEPOLICY_DIRS` cannot live in `my_car_x86_64.mk`.
