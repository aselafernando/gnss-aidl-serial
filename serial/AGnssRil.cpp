/*
 * Copyright (C) 2026 Asela Fernando
 * SPDX-License-Identifier: Apache-2.0
 */

#include "AGnssRil.h"

namespace aidl::android::hardware::gnss {

using ndk::ScopedAStatus;

ScopedAStatus AGnssRil::setCallback(const std::shared_ptr<IAGnssRilCallback>& callback) {
    mCallback = callback;
    return ScopedAStatus::ok();
}
ScopedAStatus AGnssRil::setRefLocation(const AGnssRefLocation&) {
    return ScopedAStatus::ok();
}
ScopedAStatus AGnssRil::setSetId(SetIdType, const std::string&) {
    return ScopedAStatus::ok();
}
ScopedAStatus AGnssRil::updateNetworkState(const NetworkAttributes&) {
    return ScopedAStatus::ok();
}
ScopedAStatus AGnssRil::injectNiSuplMessageData(const std::vector<uint8_t>&, int) {
    return ScopedAStatus::ok();
}

}  // namespace aidl::android::hardware::gnss
