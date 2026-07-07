/*
 * Copyright (C) 2026 Asela Fernando
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/gnss/BnAGnss.h>

namespace aidl::android::hardware::gnss {

// A serial receiver has no network assistance path; methods are accepted as
// no-ops so the framework's A-GNSS plumbing initialises cleanly.
class AGnss : public BnAGnss {
  public:
    ndk::ScopedAStatus setCallback(
            const std::shared_ptr<IAGnssCallback>& callback) override;
    ndk::ScopedAStatus dataConnClosed() override;
    ndk::ScopedAStatus dataConnFailed() override;
    ndk::ScopedAStatus setServer(IAGnssCallback::AGnssType type,
                                 const std::string& hostname, int port) override;
    ndk::ScopedAStatus dataConnOpen(int64_t networkHandle, const std::string& apn,
                                    AGnss::ApnIpType apnIpType) override;

  private:
    std::shared_ptr<IAGnssCallback> mCallback;
};

}  // namespace aidl::android::hardware::gnss
