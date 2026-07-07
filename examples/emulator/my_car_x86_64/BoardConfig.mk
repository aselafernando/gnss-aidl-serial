# Board config for my_car_x86_64: the stock emulator board plus the GNSS
# sepolicy. Confirm the include path against your tree:
#   grep PRODUCT_DEVICE device/generic/car/sdk_car_x86_64.mk
#   find device -path "*<that device>*BoardConfig.mk"
include device/generic/car/emulator_car64_x86_64/BoardConfig.mk

# Set <vendor> to wherever you cloned gnss-aidl-serial (see ../README.md).
BOARD_VENDOR_SEPOLICY_DIRS += device/<vendor>/gnss-aidl-serial/sepolicy
