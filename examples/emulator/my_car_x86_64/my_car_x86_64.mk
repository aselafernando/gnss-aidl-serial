# AAOS x86_64 emulator + serial NMEA GNSS HAL.
$(call inherit-product, device/generic/car/sdk_car_x86_64.mk)

PRODUCT_NAME := my_car_x86_64
PRODUCT_DEVICE := my_car_x86_64
PRODUCT_MODEL := AAOS emulator with serial GNSS

PRODUCT_PACKAGES += com.android.hardware.gnss.serial

# Guest end of `emulator -qemu -serial <host port>` — adjust after verifying
# with /proc/tty/driver/serial (see the gnss-aidl-serial README).
PRODUCT_VENDOR_PROPERTIES += \
    persist.vendor.gps.device=ttyS1 \
    persist.vendor.gps.ttybaud=4800 \
    persist.vendor.gps.time_sync=0