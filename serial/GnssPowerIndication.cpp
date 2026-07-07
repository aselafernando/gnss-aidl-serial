/*
 * Copyright (C) 2026 Asela Fernando
 * SPDX-License-Identifier: Apache-2.0
 */

#include "GnssPowerIndication.h"

#include <aidl/android/hardware/gnss/GnssPowerStats.h>

namespace aidl::android::hardware::gnss {

using ndk::ScopedAStatus;

ScopedAStatus GnssPowerIndication::setCallback(
        const std::shared_ptr<IGnssPowerIndicationCallback>& callback) {
    std::lock_guard<std::mutex> lock(mMutex);
    mCallback = callback;
    if (mCallback) {
        // No power-domain instrumentation available.
        mCallback->setCapabilitiesCb(0);
    }
    return ScopedAStatus::ok();
}

ScopedAStatus GnssPowerIndication::requestGnssPowerStats() {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mCallback) {
        GnssPowerStats stats{};
        stats.elapsedRealtime.flags = ElapsedRealtime::HAS_TIMESTAMP_NS;
        stats.elapsedRealtime.timestampNs = 0;
        mCallback->gnssPowerStatsCb(stats);
    }
    return ScopedAStatus::ok();
}

}  // namespace aidl::android::hardware::gnss
