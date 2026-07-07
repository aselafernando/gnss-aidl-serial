/*
 * Copyright (C) 2006 The Android Open Source Project
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

#include <aidl/android/hardware/gnss/GnssLocation.h>
#include <aidl/android/hardware/gnss/IGnssCallback.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace aidl::android::hardware::gnss {

/**
 * Streaming NMEA-0183 parser.
 *
 * Feed it raw serial bytes via addBytes(); it accumulates complete sentences,
 * parses the GGA/GSA/GSV/RMC/VTG talkers and invokes the registered callbacks:
 *
 *   - onNmea       : every raw sentence (timestamp + text), for gnssNmeaCb
 *   - onLocation   : a fused GnssLocation once a position fix is complete
 *   - onSvStatus   : the satellite-in-view list once a GSV burst completes
 *
 * This is a direct, multi-constellation aware port of the legacy
 * hardware/gps.h NMEA reader, retargeted onto the AIDL data model.
 */
class NmeaParser {
  public:
    using GnssLocation = ::aidl::android::hardware::gnss::GnssLocation;
    using GnssSvInfo = ::aidl::android::hardware::gnss::IGnssCallback::GnssSvInfo;

    using NmeaCallback = std::function<void(int64_t timestampMs, const std::string& sentence)>;
    using LocationCallback = std::function<void(const GnssLocation&)>;
    using SvStatusCallback = std::function<void(const std::vector<GnssSvInfo>&)>;

    NmeaParser() { reset(); }

    void setNmeaCallback(NmeaCallback cb) { mNmeaCb = std::move(cb); }
    void setLocationCallback(LocationCallback cb) { mLocationCb = std::move(cb); }
    void setSvStatusCallback(SvStatusCallback cb) { mSvStatusCb = std::move(cb); }

    // When enabled (default), the system clock is steered towards GPS UTC time
    // whenever the difference exceeds thresholdSeconds. Mirrors the legacy
    // persist.vendor.gps.time_sync behaviour. 0 disables.
    void setTimeSyncThreshold(long thresholdSeconds) { mTimeSyncThreshold = thresholdSeconds; }

    void reset();

    // Feed raw bytes read from the serial device.
    void addBytes(const char* data, size_t len);

  private:
    static constexpr int kMaxNmeaSize = 255;
    static constexpr int kMaxSvs = 64;

    void addChar(char c);
    void parseSentence();

    NmeaCallback mNmeaCb;
    LocationCallback mLocationCb;
    SvStatusCallback mSvStatusCb;

    long mTimeSyncThreshold = 0;

    // line accumulator
    int mPos = 0;
    bool mOverflow = false;
    char mLine[kMaxNmeaSize + 1];

    // date carried across sentences (GGA carries time only)
    int mUtcYear = -1;
    int mUtcMon = -1;
    int mUtcDay = -1;

    // GSA seen this cycle -> trust GSA accuracy/altitude masking over GGA
    bool mHaveGsa = false;

    // week-number rollover correction reported once per session
    bool mRolloverLogged = false;

    // fix accumulated across the sentences of one reporting cycle
    GnssLocation mFix;

    // satellite bookkeeping
    std::vector<GnssSvInfo> mSvs;
    int mUsedInFix[12] = {0};
};

}  // namespace aidl::android::hardware::gnss
