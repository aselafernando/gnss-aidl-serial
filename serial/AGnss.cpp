/*
 * Copyright (C) 2026 Asela Fernando
 * SPDX-License-Identifier: Apache-2.0
 */

#include "AGnss.h"

namespace aidl::android::hardware::gnss {

using ndk::ScopedAStatus;

ScopedAStatus AGnss::setCallback(const std::shared_ptr<IAGnssCallback>& callback) {
    mCallback = callback;
    return ScopedAStatus::ok();
}
ScopedAStatus AGnss::dataConnClosed() {
    return ScopedAStatus::ok();
}
ScopedAStatus AGnss::dataConnFailed() {
    return ScopedAStatus::ok();
}
ScopedAStatus AGnss::setServer(IAGnssCallback::AGnssType, const std::string&, int) {
    return ScopedAStatus::ok();
}
ScopedAStatus AGnss::dataConnOpen(int64_t, const std::string&, AGnss::ApnIpType) {
    return ScopedAStatus::ok();
}

}  // namespace aidl::android::hardware::gnss
