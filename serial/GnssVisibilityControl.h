/*
 * Copyright (C) 2026 Asela Fernando
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/gnss/visibility_control/BnGnssVisibilityControl.h>

namespace aidl::android::hardware::gnss::visibility_control {

// Non-framework-initiated (NFW) location access notification is not generated
// by this HAL; the framework still requires the extension to be present.
class GnssVisibilityControl : public BnGnssVisibilityControl {
  public:
    ndk::ScopedAStatus enableNfwLocationAccess(
            const std::vector<std::string>& proxyApps) override;
    ndk::ScopedAStatus setCallback(
            const std::shared_ptr<IGnssVisibilityControlCallback>& callback) override;

  private:
    std::shared_ptr<IGnssVisibilityControlCallback> mCallback;
};

}  // namespace aidl::android::hardware::gnss::visibility_control
