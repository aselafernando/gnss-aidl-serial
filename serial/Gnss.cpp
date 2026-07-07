/*
 * Copyright (C) 2026 Asela Fernando
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "gnss_serial"

#include "Gnss.h"

#include "AGnss.h"
#include "AGnssRil.h"
#include "GnssAntennaInfo.h"
#include "GnssConfiguration.h"
#include "GnssDebug.h"
#include "GnssMeasurementInterface.h"
#include "GnssPowerIndication.h"
#include "GnssVisibilityControl.h"

#include <cutils/properties.h>
#include <log/log.h>

namespace aidl::android::hardware::gnss {

using ndk::ScopedAStatus;

// Slow background rate used while no session is active, matching the legacy
// driver's 10 s idle / 1 s active behaviour.
static constexpr unsigned short kIdleRateMs = 10000;

Gnss::Gnss() {
    mGnssConfiguration = ndk::SharedRefBase::make<GnssConfiguration>();
    mGnssPowerIndication = ndk::SharedRefBase::make<GnssPowerIndication>();

    // Wire the parser's outputs to the AIDL callbacks.
    mParser.setLocationCallback([this](const GnssLocation& loc) { reportLocation(loc); });
    mParser.setSvStatusCallback(
            [this](const std::vector<IGnssCallback::GnssSvInfo>& svs) { reportSvStatus(svs); });
    mParser.setNmeaCallback(
            [this](int64_t ts, const std::string& s) { reportNmea(ts, s); });

    char prop[PROPERTY_VALUE_MAX];
    long timeSync = 0;
    if (property_get("persist.vendor.gps.time_sync", prop, "") != 0) {
        timeSync = atol(prop);
    }
    mParser.setTimeSyncThreshold(timeSync);

    // Initial fix interval from persist.vendor.gps.max_rate (legacy semantics).
    unsigned short period = 1000;
    if (property_get("persist.vendor.gps.max_rate", prop, "") != 0) {
        unsigned long rate = strtoul(prop, nullptr, 10);
        if (rate > 0 && rate < 66) {
            period = (unsigned short)(rate * 1000);
        } else if (rate >= 250 && rate < 65536) {
            period = (unsigned short)rate;
        }
    }
    mFixIntervalMs = period;
}

Gnss::~Gnss() {
    close();
}

unsigned short Gnss::clampRateMs(int minIntervalMs) {
    if (minIntervalMs < 250) return 250;
    if (minIntervalMs > 65000) return 65000;
    return (unsigned short)minIntervalMs;
}

// ---- lifecycle --------------------------------------------------------------

ScopedAStatus Gnss::setCallback(const std::shared_ptr<IGnssCallback>& callback) {
    if (callback == nullptr) {
        return ScopedAStatus::fromExceptionCode(EX_NULL_POINTER);
    }
    {
        std::lock_guard<std::mutex> lock(mCallbackMutex);
        mCallback = callback;
    }

    // Advertise the (small) capability set a bare NMEA receiver supports and
    // basic system info. Scheduling is honoured via the UBX rate command.
    callback->gnssSetCapabilitiesCb(IGnssCallback::CAPABILITY_SCHEDULING);

    IGnssCallback::GnssSystemInfo info;
    info.yearOfHw = 2023;
    info.name = "Serial NMEA GNSS";
    callback->gnssSetSystemInfoCb(info);
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::start() {
    if (mSessionActive.exchange(true)) {
        return ScopedAStatus::ok();  // already running
    }

    if (!mThreadRunning.exchange(true)) {
        mThread = std::thread(&Gnss::readerThreadLoop, this);
    }

    reportStatus(IGnssCallback::GnssStatusValue::SESSION_BEGIN);
    mDevice.setMeasurementRate(mFixIntervalMs);
    ALOGI("session started, fix interval %u ms", mFixIntervalMs.load());
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::stop() {
    if (!mSessionActive.exchange(false)) {
        return ScopedAStatus::ok();
    }
    mDevice.setMeasurementRate(kIdleRateMs);
    reportStatus(IGnssCallback::GnssStatusValue::SESSION_END);
    ALOGI("session stopped");
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::close() {
    stop();
    mThreadRunning = false;
    mDevice.close();  // unblock the reader's blocking read()
    if (mThread.joinable()) {
        mThread.join();
    }
    std::lock_guard<std::mutex> lock(mCallbackMutex);
    mCallback = nullptr;
    return ScopedAStatus::ok();
}

// ---- reader thread ----------------------------------------------------------

void Gnss::readerThreadLoop() {
    if (!mDevice.open()) {
        ALOGE("reader thread exiting: serial device unavailable");
        mThreadRunning = false;
        return;
    }
    mDevice.setMeasurementRate(mSessionActive ? mFixIntervalMs.load() : kIdleRateMs);

    char buf[256];
    while (mThreadRunning) {
        ssize_t n = mDevice.read(buf, sizeof(buf));
        if (n > 0) {
            if (mSessionActive) {
                mParser.addBytes(buf, (size_t)n);
            }
        } else if (n == 0) {
            // EOF — device closed underneath us.
            break;
        } else {
            ALOGE("serial read error; reader thread exiting");
            break;
        }
    }
    mDevice.close();
}

// ---- report helpers ---------------------------------------------------------

void Gnss::reportLocation(const GnssLocation& location) {
    std::lock_guard<std::mutex> lock(mCallbackMutex);
    if (mCallback) mCallback->gnssLocationCb(location);
}

void Gnss::reportSvStatus(const std::vector<IGnssCallback::GnssSvInfo>& svList) {
    if (!mSvStatusEnabled) return;
    std::lock_guard<std::mutex> lock(mCallbackMutex);
    if (mCallback) mCallback->gnssSvStatusCb(svList);
}

void Gnss::reportNmea(int64_t timestampMs, const std::string& nmea) {
    if (!mNmeaEnabled) return;
    std::lock_guard<std::mutex> lock(mCallbackMutex);
    if (mCallback) mCallback->gnssNmeaCb(timestampMs, nmea);
}

void Gnss::reportStatus(IGnssCallback::GnssStatusValue status) {
    std::lock_guard<std::mutex> lock(mCallbackMutex);
    if (mCallback) mCallback->gnssStatusCb(status);
}

// ---- scheduling & toggles ---------------------------------------------------

ScopedAStatus Gnss::setPositionMode(const IGnss::PositionModeOptions& options) {
    unsigned short rate = clampRateMs(options.minIntervalMs);
    mFixIntervalMs = rate;
    if (mSessionActive) {
        mDevice.setMeasurementRate(rate);
    }
    ALOGI("setPositionMode: minInterval=%d ms -> rate=%u ms", options.minIntervalMs, rate);
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::startSvStatus() {
    mSvStatusEnabled = true;
    return ScopedAStatus::ok();
}
ScopedAStatus Gnss::stopSvStatus() {
    mSvStatusEnabled = false;
    return ScopedAStatus::ok();
}
ScopedAStatus Gnss::startNmea() {
    mNmeaEnabled = true;
    return ScopedAStatus::ok();
}
ScopedAStatus Gnss::stopNmea() {
    mNmeaEnabled = false;
    return ScopedAStatus::ok();
}

// ---- assistance injection (no-ops for a bare serial receiver) ---------------

ScopedAStatus Gnss::injectTime(int64_t, int64_t, int) {
    return ScopedAStatus::ok();
}
ScopedAStatus Gnss::injectLocation(const GnssLocation&) {
    return ScopedAStatus::ok();
}
ScopedAStatus Gnss::injectBestLocation(const GnssLocation&) {
    return ScopedAStatus::ok();
}
ScopedAStatus Gnss::deleteAidingData(IGnss::GnssAidingData) {
    return ScopedAStatus::ok();
}

// ---- extensions -------------------------------------------------------------

ScopedAStatus Gnss::getExtensionGnssConfiguration(std::shared_ptr<IGnssConfiguration>* out) {
    *out = mGnssConfiguration;
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::getExtensionGnssPowerIndication(
        std::shared_ptr<IGnssPowerIndication>* out) {
    *out = mGnssPowerIndication;
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::getExtensionGnssMeasurement(
        std::shared_ptr<IGnssMeasurementInterface>* out) {
    *out = ndk::SharedRefBase::make<GnssMeasurementInterface>();
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::getExtensionAGnss(std::shared_ptr<IAGnss>* out) {
    *out = ndk::SharedRefBase::make<AGnss>();
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::getExtensionAGnssRil(std::shared_ptr<IAGnssRil>* out) {
    *out = ndk::SharedRefBase::make<AGnssRil>();
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::getExtensionGnssVisibilityControl(
        std::shared_ptr<visibility_control::IGnssVisibilityControl>* out) {
    *out = ndk::SharedRefBase::make<visibility_control::GnssVisibilityControl>();
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::getExtensionGnssDebug(std::shared_ptr<IGnssDebug>* out) {
    *out = ndk::SharedRefBase::make<GnssDebug>();
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::getExtensionGnssAntennaInfo(std::shared_ptr<IGnssAntennaInfo>* out) {
    *out = ndk::SharedRefBase::make<GnssAntennaInfo>();
    return ScopedAStatus::ok();
}

// Unsupported extensions must fail the getter: the framework's wrappers only
// check the returned status (checkAidlStatus), not the pointer, and wrap a
// null binder — system_server then SIGSEGVs on wrapper->setCallback (seen with
// IGnssPsds during GnssNative init). A non-OK status makes the framework treat
// the extension as absent.
ScopedAStatus Gnss::getExtensionPsds(std::shared_ptr<IGnssPsds>* out) {
    *out = nullptr;
    return ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ScopedAStatus Gnss::getExtensionGnssBatching(std::shared_ptr<IGnssBatching>* out) {
    *out = nullptr;
    return ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ScopedAStatus Gnss::getExtensionGnssGeofence(std::shared_ptr<IGnssGeofence>* out) {
    *out = nullptr;
    return ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ScopedAStatus Gnss::getExtensionGnssNavigationMessage(
        std::shared_ptr<IGnssNavigationMessageInterface>* out) {
    *out = nullptr;
    return ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ScopedAStatus Gnss::getExtensionMeasurementCorrections(
        std::shared_ptr<measurement_corrections::IMeasurementCorrectionsInterface>* out) {
    *out = nullptr;
    return ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

}  // namespace aidl::android::hardware::gnss
