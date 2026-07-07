/*
 * Copyright (C) 2026 Asela Fernando
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/gnss/BnAGnssRil.h>

#include <cstdint>
#include <vector>

namespace aidl::android::hardware::gnss {

// No RIL-assisted location for a serial receiver; accept and ignore.
class AGnssRil : public BnAGnssRil {
  public:
    ndk::ScopedAStatus setCallback(
            const std::shared_ptr<IAGnssRilCallback>& callback) override;
    ndk::ScopedAStatus setRefLocation(
            const AGnssRefLocation& agnssReflocation) override;
    ndk::ScopedAStatus setSetId(SetIdType type, const std::string& setid) override;
    ndk::ScopedAStatus updateNetworkState(
            const NetworkAttributes& attributes) override;
    ndk::ScopedAStatus injectNiSuplMessageData(const std::vector<uint8_t>& msgData,
                                               int slotIndex) override;

  private:
    std::shared_ptr<IAGnssRilCallback> mCallback;
};

}  // namespace aidl::android::hardware::gnss
