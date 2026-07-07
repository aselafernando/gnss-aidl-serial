/*
 * Copyright (C) 2026 Asela Fernando
 * SPDX-License-Identifier: Apache-2.0
 */

#include "GnssAntennaInfo.h"

namespace aidl::android::hardware::gnss {

using ndk::ScopedAStatus;

ScopedAStatus GnssAntennaInfo::setCallback(
        const std::shared_ptr<IGnssAntennaInfoCallback>& callback) {
    mCallback = callback;
    return ScopedAStatus::ok();
}

ScopedAStatus GnssAntennaInfo::close() {
    mCallback = nullptr;
    return ScopedAStatus::ok();
}

}  // namespace aidl::android::hardware::gnss
