/*
 * Copyright (C) 2026 Asela Fernando
 * SPDX-License-Identifier: Apache-2.0
 */

#include "GnssMeasurementInterface.h"

namespace aidl::android::hardware::gnss {

using ndk::ScopedAStatus;

ScopedAStatus GnssMeasurementInterface::setCallback(
        const std::shared_ptr<IGnssMeasurementCallback>&, bool, bool) {
    return ScopedAStatus::ok();
}

ScopedAStatus GnssMeasurementInterface::setCallbackWithOptions(
        const std::shared_ptr<IGnssMeasurementCallback>&, const Options&) {
    return ScopedAStatus::ok();
}

ScopedAStatus GnssMeasurementInterface::close() {
    return ScopedAStatus::ok();
}

}  // namespace aidl::android::hardware::gnss
