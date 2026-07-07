/*
 * Copyright (C) 2026 Asela Fernando
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/gnss/BnGnssAntennaInfo.h>

namespace aidl::android::hardware::gnss {

// getExtensionGnssAntennaInfo() is non-nullable. A serial receiver provides no
// antenna geometry, so registration is accepted but no info is ever reported.
class GnssAntennaInfo : public BnGnssAntennaInfo {
  public:
    ndk::ScopedAStatus setCallback(
            const std::shared_ptr<IGnssAntennaInfoCallback>& callback) override;
    ndk::ScopedAStatus close() override;

  private:
    std::shared_ptr<IGnssAntennaInfoCallback> mCallback;
};

}  // namespace aidl::android::hardware::gnss
