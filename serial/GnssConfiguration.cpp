/*
 * Copyright (C) 2026 Asela Fernando
 * SPDX-License-Identifier: Apache-2.0
 */

#include "GnssConfiguration.h"

namespace aidl::android::hardware::gnss {

using ndk::ScopedAStatus;

ScopedAStatus GnssConfiguration::setSuplVersion(int) {
    return ScopedAStatus::ok();
}
ScopedAStatus GnssConfiguration::setSuplMode(int) {
    return ScopedAStatus::ok();
}
ScopedAStatus GnssConfiguration::setLppProfile(int) {
    return ScopedAStatus::ok();
}
ScopedAStatus GnssConfiguration::setGlonassPositioningProtocol(int) {
    return ScopedAStatus::ok();
}
ScopedAStatus GnssConfiguration::setEmergencySuplPdn(bool) {
    return ScopedAStatus::ok();
}
ScopedAStatus GnssConfiguration::setEsExtensionSec(int) {
    return ScopedAStatus::ok();
}
ScopedAStatus GnssConfiguration::setBlocklist(const std::vector<BlocklistedSource>&) {
    return ScopedAStatus::ok();
}

}  // namespace aidl::android::hardware::gnss
