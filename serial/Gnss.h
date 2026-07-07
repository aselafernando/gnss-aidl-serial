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

#pragma once

#include <aidl/android/hardware/gnss/BnGnss.h>
#include <aidl/android/hardware/gnss/BnGnssConfiguration.h>
#include <aidl/android/hardware/gnss/BnGnssPowerIndication.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

#include "NmeaParser.h"
#include "SerialDevice.h"

namespace aidl::android::hardware::gnss {

/**
 * Serial NMEA GNSS HAL.
 *
 * Implements android.hardware.gnss.IGnss by reading NMEA-0183 from a UART/USB
 * serial receiver and translating it onto the AIDL callback surface
 * (gnssLocationCb / gnssSvStatusCb / gnssNmeaCb / gnssStatusCb).
 *
 * The non-nullable extension interfaces required by the framework are returned
 * as minimal implementations; extensions the serial path cannot provide fail
 * with EX_UNSUPPORTED_OPERATION (never ok()+null: the framework wraps the
 * result without a null check and would crash).
 */
class Gnss : public BnGnss {
  public:
    Gnss();
    ~Gnss();

    // Core lifecycle
    ndk::ScopedAStatus setCallback(const std::shared_ptr<IGnssCallback>& callback) override;
    ndk::ScopedAStatus start() override;
    ndk::ScopedAStatus stop() override;
    ndk::ScopedAStatus close() override;

    // Position scheduling
    ndk::ScopedAStatus setPositionMode(const IGnss::PositionModeOptions& options) override;

    // Reporting toggles (V2+)
    ndk::ScopedAStatus startSvStatus() override;
    ndk::ScopedAStatus stopSvStatus() override;
    ndk::ScopedAStatus startNmea() override;
    ndk::ScopedAStatus stopNmea() override;

    // Assistance injection — accepted but unused by a bare serial receiver.
    ndk::ScopedAStatus injectTime(int64_t timeMs, int64_t timeReferenceMs,
                                  int uncertaintyMs) override;
    ndk::ScopedAStatus injectLocation(const GnssLocation& location) override;
    ndk::ScopedAStatus injectBestLocation(const GnssLocation& location) override;
    ndk::ScopedAStatus deleteAidingData(IGnss::GnssAidingData aidingDataFlags) override;

    // Non-nullable extensions (minimal implementations).
    ndk::ScopedAStatus getExtensionGnssConfiguration(
            std::shared_ptr<IGnssConfiguration>* out) override;
    ndk::ScopedAStatus getExtensionGnssPowerIndication(
            std::shared_ptr<IGnssPowerIndication>* out) override;
    ndk::ScopedAStatus getExtensionGnssMeasurement(
            std::shared_ptr<IGnssMeasurementInterface>* out) override;
    ndk::ScopedAStatus getExtensionAGnss(std::shared_ptr<IAGnss>* out) override;
    ndk::ScopedAStatus getExtensionAGnssRil(std::shared_ptr<IAGnssRil>* out) override;
    ndk::ScopedAStatus getExtensionGnssVisibilityControl(
            std::shared_ptr<visibility_control::IGnssVisibilityControl>* out) override;
    ndk::ScopedAStatus getExtensionGnssDebug(std::shared_ptr<IGnssDebug>* out) override;
    ndk::ScopedAStatus getExtensionGnssAntennaInfo(
            std::shared_ptr<IGnssAntennaInfo>* out) override;

    // Nullable extensions — not supported by a serial NMEA receiver.
    ndk::ScopedAStatus getExtensionPsds(std::shared_ptr<IGnssPsds>* out) override;
    ndk::ScopedAStatus getExtensionGnssBatching(
            std::shared_ptr<IGnssBatching>* out) override;
    ndk::ScopedAStatus getExtensionGnssGeofence(
            std::shared_ptr<IGnssGeofence>* out) override;
    ndk::ScopedAStatus getExtensionGnssNavigationMessage(
            std::shared_ptr<IGnssNavigationMessageInterface>* out) override;
    ndk::ScopedAStatus getExtensionMeasurementCorrections(
            std::shared_ptr<measurement_corrections::IMeasurementCorrectionsInterface>* out)
            override;

  private:
    void readerThreadLoop();
    void reportLocation(const GnssLocation& location);
    void reportSvStatus(const std::vector<IGnssCallback::GnssSvInfo>& svList);
    void reportNmea(int64_t timestampMs, const std::string& nmea);
    void reportStatus(IGnssCallback::GnssStatusValue status);

    static unsigned short clampRateMs(int minIntervalMs);

    std::mutex mCallbackMutex;
    std::shared_ptr<IGnssCallback> mCallback;

    SerialDevice mDevice;
    NmeaParser mParser;

    std::thread mThread;
    std::atomic<bool> mThreadRunning{false};  // reader thread alive
    std::atomic<bool> mSessionActive{false};  // start()/stop() session

    std::atomic<bool> mNmeaEnabled{true};
    std::atomic<bool> mSvStatusEnabled{true};
    std::atomic<unsigned short> mFixIntervalMs{1000};

    std::shared_ptr<IGnssConfiguration> mGnssConfiguration;
    std::shared_ptr<IGnssPowerIndication> mGnssPowerIndication;
};

}  // namespace aidl::android::hardware::gnss
