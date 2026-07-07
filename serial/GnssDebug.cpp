/*
 * Copyright (C) 2026 Asela Fernando
 * SPDX-License-Identifier: Apache-2.0
 */

#include "GnssDebug.h"

namespace aidl::android::hardware::gnss {

using ndk::ScopedAStatus;

ScopedAStatus GnssDebug::getDebugData(DebugData* debugData) {
    // Default-constructed: position.valid / time.valid are false and the
    // satellite list is empty -- a serial receiver exposes no engine internals.
    *debugData = DebugData{};
    return ScopedAStatus::ok();
}

}  // namespace aidl::android::hardware::gnss
