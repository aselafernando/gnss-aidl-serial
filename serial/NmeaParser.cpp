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

#define LOG_TAG "gnss_serial_nmea"

#include "NmeaParser.h"

#include <aidl/android/hardware/gnss/ElapsedRealtime.h>
#include <aidl/android/hardware/gnss/GnssConstellationType.h>
#include <log/log.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sys/time.h>

namespace aidl::android::hardware::gnss {

using GnssConstellationType = ::aidl::android::hardware::gnss::GnssConstellationType;

namespace {

// ---- minimal field tokenizer over a single sentence ------------------------

constexpr int kMaxTokens = 32;

struct Token {
    const char* p = "";
    const char* end = "";
    bool empty() const { return p >= end; }
    int len() const { return static_cast<int>(end - p); }
};

struct Tokenizer {
    int count = 0;
    Token tokens[kMaxTokens];

    Tokenizer(const char* p, const char* end) {
        // optional leading '$'
        if (p < end && p[0] == '$') p += 1;
        // strip trailing CR/LF
        if (end > p && end[-1] == '\n') {
            end -= 1;
            if (end > p && end[-1] == '\r') end -= 1;
        }
        // strip "*XX" checksum
        if (end >= p + 3 && end[-3] == '*') end -= 3;

        while (p < end && count < kMaxTokens) {
            const char* q = static_cast<const char*>(memchr(p, ',', end - p));
            if (q == nullptr) q = end;
            tokens[count].p = p;
            tokens[count].end = q;
            count += 1;
            p = (q < end) ? q + 1 : q;
        }
    }

    Token get(int index) const {
        if (index < 0 || index >= count) return Token{};
        return tokens[index];
    }
};

int str2int(const char* p, const char* end) {
    int result = 0;
    for (; p < end; p++) {
        int c = *p - '0';
        if ((unsigned)c >= 10) return -1;
        result = result * 10 + c;
    }
    return result;
}

double str2float(const char* p, const char* end) {
    size_t len = end - p;
    char temp[32];
    if (len == 0 || len >= sizeof(temp)) return 0.;
    memcpy(temp, p, len);
    temp[len] = '\0';
    return strtod(temp, nullptr);
}

// NMEA "ddmm.mmmm" -> decimal degrees
double convertFromHHMM(const Token& tok) {
    double val = str2float(tok.p, tok.end);
    int degrees = (int)(std::floor(val) / 100);
    double minutes = val - degrees * 100.;
    return degrees + minutes / 60.0;
}

// Map a GSV/GSA talker id (e.g. "GP", "GL", "GA", "GB"/"BD", "GQ") to a
// constellation, and translate the NMEA satellite number to a GNSS svid.
GnssConstellationType constellationForTalker(const char* talker) {
    if (!memcmp(talker, "GP", 2)) return GnssConstellationType::GPS;
    if (!memcmp(talker, "GL", 2)) return GnssConstellationType::GLONASS;
    if (!memcmp(talker, "GA", 2)) return GnssConstellationType::GALILEO;
    if (!memcmp(talker, "GB", 2) || !memcmp(talker, "BD", 2))
        return GnssConstellationType::BEIDOU;
    if (!memcmp(talker, "GQ", 2)) return GnssConstellationType::QZSS;
    return GnssConstellationType::UNKNOWN;
}

int64_t bootTimeNanos() {
    struct timespec ts;
    clock_gettime(CLOCK_BOOTTIME, &ts);
    return static_cast<int64_t>(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
}

void stampElapsedRealtime(GnssLocation* loc) {
    loc->elapsedRealtime.flags = ElapsedRealtime::HAS_TIMESTAMP_NS |
                                 ElapsedRealtime::HAS_TIME_UNCERTAINTY_NS;
    loc->elapsedRealtime.timestampNs = bootTimeNanos();
    loc->elapsedRealtime.timeUncertaintyNs = 0;
}

}  // namespace

void NmeaParser::reset() {
    mPos = 0;
    mOverflow = false;
    mUtcYear = mUtcMon = mUtcDay = -1;
    mHaveGsa = false;
    mFix = GnssLocation{};
    mSvs.clear();
    memset(mUsedInFix, 0, sizeof(mUsedInFix));
}

void NmeaParser::addBytes(const char* data, size_t len) {
    for (size_t i = 0; i < len; i++) addChar(data[i]);
}

void NmeaParser::addChar(char c) {
    if (mOverflow) {
        mOverflow = (c != '\n');
        return;
    }
    if (mPos >= kMaxNmeaSize) {
        mOverflow = true;
        mPos = 0;
        return;
    }
    mLine[mPos++] = c;
    if (c == '\n') {
        parseSentence();
        mPos = 0;
    }
}

void NmeaParser::parseSentence() {
    if (mPos < 9) return;
    mLine[mPos] = 0;

    struct timeval tv;
    gettimeofday(&tv, nullptr);
    const int64_t nowMs = (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;

    // Raw sentence to the framework (gnssNmeaCb) regardless of type.
    if (mNmeaCb) mNmeaCb(nowMs, std::string(mLine, mPos));

    Tokenizer tzer(mLine, mLine + mPos);
    Token id = tzer.get(0);
    if (id.len() < 5) return;

    const char* talker = id.p;       // e.g. "GP"
    const char* type = id.p + 2;     // e.g. "GGA"

    bool sendFix = false;

    if (!memcmp(type, "GGA", 3)) {
        // field 1 (UTC time) is unused: RMC carries both date and time
        Token tokLat = tzer.get(2), tokLatH = tzer.get(3);
        Token tokLon = tzer.get(4), tokLonH = tzer.get(5);
        Token tokFix = tzer.get(6);
        Token tokAcc = tzer.get(8);
        Token tokAlt = tzer.get(9);

        int fix = str2int(tokFix.p, tokFix.end);
        if (fix > 0) {
            if (tokLat.len() >= 6 && tokLon.len() >= 6) {
                double lat = convertFromHHMM(tokLat);
                if (!tokLatH.empty() && tokLatH.p[0] == 'S') lat = -lat;
                double lon = convertFromHHMM(tokLon);
                if (!tokLonH.empty() && tokLonH.p[0] == 'W') lon = -lon;
                mFix.gnssLocationFlags |= GnssLocation::HAS_LAT_LONG;
                mFix.latitudeDegrees = lat;
                mFix.longitudeDegrees = lon;
            }
            if (!tokAlt.empty()) {
                mFix.gnssLocationFlags |= GnssLocation::HAS_ALTITUDE;
                mFix.altitudeMeters = str2float(tokAlt.p, tokAlt.end);
            }
        }
        if (!mHaveGsa && !tokAcc.empty()) {
            double acc = str2float(tokAcc.p, tokAcc.end);
            if (acc <= 99.0 && fix > 0) {
                mFix.gnssLocationFlags |= GnssLocation::HAS_HORIZONTAL_ACCURACY;
                mFix.horizontalAccuracyMeters = acc;
            }
            sendFix = true;
        }
    } else if (!memcmp(type, "GSA", 3)) {
        Token tokMode = tzer.get(2);  // 1=no fix, 2=2D, 3=3D
        Token tokHdop = tzer.get(16);
        int mode = str2int(tokMode.p, tokMode.end);
        if (mode == 2) mFix.gnssLocationFlags &= ~GnssLocation::HAS_ALTITUDE;
        if (!tokHdop.empty() && mode > 1) {
            double acc = str2float(tokHdop.p, tokHdop.end);
            if (acc <= 99.0) {
                mFix.gnssLocationFlags |= GnssLocation::HAS_HORIZONTAL_ACCURACY;
                mFix.horizontalAccuracyMeters = acc;
            }
        }
        // svids 3..14 used in fix
        for (int i = 0; i < 12; i++) {
            Token t = tzer.get(3 + i);
            mUsedInFix[i] = t.empty() ? 0 : str2int(t.p, t.end);
        }
        mHaveGsa = true;
        sendFix = true;
    } else if (!memcmp(type, "GSV", 3)) {
        GnssConstellationType constellation = constellationForTalker(talker);
        int numMessages = str2int(tzer.get(1).p, tzer.get(1).end);
        int msgNumber = str2int(tzer.get(2).p, tzer.get(2).end);
        int svsInView = str2int(tzer.get(3).p, tzer.get(3).end);

        if (msgNumber == 1) {
            // start of a constellation burst: drop previous entries for it
            mSvs.erase(std::remove_if(mSvs.begin(), mSvs.end(),
                                      [&](const GnssSvInfo& s) {
                                          return s.constellation == constellation;
                                      }),
                       mSvs.end());
        }

        for (int i = 0; i < 4; i++) {
            int base = 4 + i * 4;
            Token tokPrn = tzer.get(base);
            if (tokPrn.empty()) continue;
            int prn = str2int(tokPrn.p, tokPrn.end);
            if (prn <= 0) continue;

            GnssSvInfo sv;
            sv.svid = prn;
            sv.constellation = constellation;
            Token tokElev = tzer.get(base + 1);
            Token tokAzim = tzer.get(base + 2);
            Token tokSnr = tzer.get(base + 3);
            sv.elevationDegrees = tokElev.empty() ? 0 : str2float(tokElev.p, tokElev.end);
            sv.azimuthDegrees = tokAzim.empty() ? 0 : str2float(tokAzim.p, tokAzim.end);
            sv.cN0Dbhz = tokSnr.empty() ? 0 : str2float(tokSnr.p, tokSnr.end);
            sv.basebandCN0DbHz = sv.cN0Dbhz;
            sv.carrierFrequencyHz = 0;
            sv.svFlag = 0;
            for (int u = 0; u < 12; u++) {
                if (mUsedInFix[u] == prn) {
                    sv.svFlag |= (int)IGnssCallback::GnssSvFlags::USED_IN_FIX;
                }
            }
            if ((int)mSvs.size() < kMaxSvs) mSvs.push_back(sv);
        }

        // Emit once the last message of the *combined* "GN" burst arrives.
        // GSV is per-constellation; we report after each constellation's final
        // message so the framework gets incremental SV updates.
        if (numMessages == msgNumber && mSvStatusCb) {
            mSvStatusCb(mSvs);
        }
        (void)svsInView;
    } else if (!memcmp(type, "RMC", 3)) {
        Token tokTime = tzer.get(1);
        Token tokStatus = tzer.get(2);  // A=valid, V=warning
        Token tokLat = tzer.get(3), tokLatH = tzer.get(4);
        Token tokLon = tzer.get(5), tokLonH = tzer.get(6);
        Token tokSpeed = tzer.get(7);
        Token tokBearing = tzer.get(8);
        Token tokDate = tzer.get(9);

        if (!tokStatus.empty() && tokStatus.p[0] == 'A') {
            // date (ddmmyy)
            if (tokDate.len() == 6) {
                mUtcDay = str2int(tokDate.p, tokDate.p + 2);
                mUtcMon = str2int(tokDate.p + 2, tokDate.p + 4);
                mUtcYear = str2int(tokDate.p + 4, tokDate.p + 6) + 2000;
            }
            // time + date -> epoch ms
            if (mUtcYear > 0 && tokTime.len() >= 6) {
                struct tm tm = {};
                tm.tm_hour = str2int(tokTime.p, tokTime.p + 2);
                tm.tm_min = str2int(tokTime.p + 2, tokTime.p + 4);
                tm.tm_sec = (int)str2float(tokTime.p + 4, tokTime.end);
                tm.tm_year = mUtcYear - 1900;
                tm.tm_mon = mUtcMon - 1;
                tm.tm_mday = mUtcDay;
                tm.tm_isdst = -1;
                time_t gmt = timegm(&tm);

                // GPS week-number rollover correction. Receivers with a
                // 10-bit week counter and old firmware (e.g. SiRF III /
                // GlobalSat BU-353) report dates exactly N*1024 weeks in the
                // past. Any date before this floor cannot be genuine — shift
                // forward in whole rollover periods until plausible. Must
                // happen before the time_sync clock steering below.
                constexpr time_t kRolloverPeriodSec = 1024LL * 7 * 24 * 3600;
                constexpr time_t kPlausibleFloorSec = 1735689600;  // 2025-01-01T00:00:00Z
                if (gmt > 0 && gmt < kPlausibleFloorSec) {
                    time_t corrected = gmt;
                    while (corrected < kPlausibleFloorSec) corrected += kRolloverPeriodSec;
                    if (!mRolloverLogged) {
                        ALOGI("GPS week rollover: receiver date %ld shifted to %ld",
                              (long)gmt, (long)corrected);
                        mRolloverLogged = true;
                    }
                    gmt = corrected;
                }

                mFix.timestampMillis = (int64_t)gmt * 1000;

                if (mTimeSyncThreshold > 0) {
                    long diff = (long)(time(nullptr) - gmt);
                    if (diff < -mTimeSyncThreshold || diff > mTimeSyncThreshold) {
                        struct timeval newtv = {gmt, 0};
                        if (settimeofday(&newtv, nullptr) == 0) {
                            ALOGD("system clock steered to GPS UTC time");
                        }
                    }
                }
            }
            if (tokLat.len() >= 6 && tokLon.len() >= 6) {
                double lat = convertFromHHMM(tokLat);
                if (!tokLatH.empty() && tokLatH.p[0] == 'S') lat = -lat;
                double lon = convertFromHHMM(tokLon);
                if (!tokLonH.empty() && tokLonH.p[0] == 'W') lon = -lon;
                mFix.gnssLocationFlags |= GnssLocation::HAS_LAT_LONG;
                mFix.latitudeDegrees = lat;
                mFix.longitudeDegrees = lon;
            }
            if (!tokBearing.empty()) {
                mFix.gnssLocationFlags |= GnssLocation::HAS_BEARING;
                mFix.bearingDegrees = str2float(tokBearing.p, tokBearing.end);
            }
            if (!tokSpeed.empty()) {
                mFix.gnssLocationFlags |= GnssLocation::HAS_SPEED;
                mFix.speedMetersPerSec =
                        str2float(tokSpeed.p, tokSpeed.end) * (1.852 / 3.6);  // knots -> m/s
            }
        }
    } else if (!memcmp(type, "VTG", 3)) {
        Token tokStatus = tzer.get(9);
        if (!tokStatus.empty() && tokStatus.p[0] != 'N') {
            Token tokBearing = tzer.get(1);  // true track, degrees
            Token tokSpeed = tzer.get(5);    // speed over ground, knots
            if (!tokBearing.empty()) {
                mFix.gnssLocationFlags |= GnssLocation::HAS_BEARING;
                mFix.bearingDegrees = str2float(tokBearing.p, tokBearing.end);
            }
            if (!tokSpeed.empty()) {
                mFix.gnssLocationFlags |= GnssLocation::HAS_SPEED;
                mFix.speedMetersPerSec =
                        str2float(tokSpeed.p, tokSpeed.end) * (1.852 / 3.6);
            }
        }
    }

    if (sendFix && (mFix.gnssLocationFlags & GnssLocation::HAS_LAT_LONG) && mLocationCb) {
        stampElapsedRealtime(&mFix);
        mLocationCb(mFix);
        // clear per-cycle accumulation; keep date for the next RMC
        mFix.gnssLocationFlags = 0;
    }
}

}  // namespace aidl::android::hardware::gnss
