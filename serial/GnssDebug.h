/*
 * Copyright (C) 2026 Asela Fernando
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/gnss/BnGnssDebug.h>

namespace aidl::android::hardware::gnss {

// getExtensionGnssDebug() is non-nullable, so we provide an implementation that
// reports empty debug data (a serial NMEA receiver exposes no internal state).
class GnssDebug : public BnGnssDebug {
  public:
    ndk::ScopedAStatus getDebugData(DebugData* debugData) override;
};

}  // namespace aidl::android::hardware::gnss
