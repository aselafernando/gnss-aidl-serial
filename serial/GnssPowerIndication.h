/*
 * Copyright (C) 2026 Asela Fernando
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/gnss/BnGnssPowerIndication.h>

#include <mutex>

namespace aidl::android::hardware::gnss {

// Minimal IGnssPowerIndication. Power accounting is not available from a serial
// receiver, so we report zeroed stats on request.
class GnssPowerIndication : public BnGnssPowerIndication {
  public:
    ndk::ScopedAStatus setCallback(
            const std::shared_ptr<IGnssPowerIndicationCallback>& callback) override;
    ndk::ScopedAStatus requestGnssPowerStats() override;

  private:
    std::mutex mMutex;
    std::shared_ptr<IGnssPowerIndicationCallback> mCallback;
};

}  // namespace aidl::android::hardware::gnss
