/*
 * Copyright (C) 2026 Asela Fernando
 * SPDX-License-Identifier: Apache-2.0
 */

#include "GnssVisibilityControl.h"

namespace aidl::android::hardware::gnss::visibility_control {

using ndk::ScopedAStatus;

ScopedAStatus GnssVisibilityControl::enableNfwLocationAccess(
        const std::vector<std::string>&) {
    return ScopedAStatus::ok();
}

ScopedAStatus GnssVisibilityControl::setCallback(
        const std::shared_ptr<IGnssVisibilityControlCallback>& callback) {
    mCallback = callback;
    return ScopedAStatus::ok();
}

}  // namespace aidl::android::hardware::gnss::visibility_control
