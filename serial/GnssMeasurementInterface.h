/*
 * Copyright (C) 2026 Asela Fernando
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/gnss/BnGnssMeasurementInterface.h>

namespace aidl::android::hardware::gnss {

// Raw GNSS measurements are not derivable from NMEA, so this extension accepts
// registration but never reports measurements.
class GnssMeasurementInterface : public BnGnssMeasurementInterface {
  public:
    ndk::ScopedAStatus setCallback(
            const std::shared_ptr<IGnssMeasurementCallback>& callback,
            bool enableFullTracking, bool enableCorrVecOutputs) override;
    ndk::ScopedAStatus setCallbackWithOptions(
            const std::shared_ptr<IGnssMeasurementCallback>& callback,
            const Options& options) override;
    ndk::ScopedAStatus close() override;
};

}  // namespace aidl::android::hardware::gnss
