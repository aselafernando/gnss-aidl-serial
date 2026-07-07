/*
 * Copyright (C) 2026 Asela Fernando
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "gnss_serial"

#include "Gnss.h"

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <log/log.h>

using ::aidl::android::hardware::gnss::Gnss;

int main() {
    ALOGI("Serial NMEA GNSS AIDL HAL starting");
    ABinderProcess_setThreadPoolMaxThreadCount(2);

    std::shared_ptr<Gnss> gnss = ndk::SharedRefBase::make<Gnss>();
    const std::string instance = std::string(Gnss::descriptor) + "/default";

    binder_status_t status =
            AServiceManager_addService(gnss->asBinder().get(), instance.c_str());
    if (status != STATUS_OK) {
        ALOGE("failed to register %s (status %d)", instance.c_str(), status);
        return EXIT_FAILURE;
    }
    ALOGI("registered %s", instance.c_str());

    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;  // joinThreadPool should never return
}
