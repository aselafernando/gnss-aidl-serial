/*
 * Copyright (C) 2026 Asela Fernando
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/gnss/BnGnssConfiguration.h>

namespace aidl::android::hardware::gnss {

// Minimal IGnssConfiguration: a serial NMEA receiver has no SUPL/A-GNSS state,
// so the setters are accepted and recorded only for the satellite blocklist,
// which the parser does not currently enforce.
class GnssConfiguration : public BnGnssConfiguration {
  public:
    ndk::ScopedAStatus setSuplVersion(int version) override;
    ndk::ScopedAStatus setSuplMode(int mode) override;
    ndk::ScopedAStatus setLppProfile(int lppProfile) override;
    ndk::ScopedAStatus setGlonassPositioningProtocol(int protocol) override;
    ndk::ScopedAStatus setEmergencySuplPdn(bool enable) override;
    ndk::ScopedAStatus setEsExtensionSec(int emergencyExtensionSeconds) override;
    ndk::ScopedAStatus setBlocklist(
            const std::vector<BlocklistedSource>& blocklist) override;
};

}  // namespace aidl::android::hardware::gnss
