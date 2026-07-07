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

#include <cstddef>
#include <string>

namespace aidl::android::hardware::gnss {

/**
 * Thin RAII wrapper around the GPS serial tty.
 *
 * The device path and baud rate are taken from system properties:
 *   persist.vendor.gps.device   explicit tty, e.g. "ttyO1" -> /dev/ttyO1;
 *                               overrides USB matching when set
 *   persist.vendor.gps.usb      USB vid:pid to locate the receiver by identity
 *                               (immune to enumeration order); default
 *                               "067b:2303" (PL2303, GlobalSat BU-353)
 *   persist.vendor.gps.ttybaud  4800..115200  -> default 4800
 *
 * setMeasurementRate() emits the u-blox UBX CFG-RATE message so receivers that
 * speak UBX honour the requested fix interval; it is a no-op on the wire for
 * pure-NMEA receivers (they ignore the unknown binary frame).
 */
class SerialDevice {
  public:
    SerialDevice() = default;
    ~SerialDevice() { close(); }

    SerialDevice(const SerialDevice&) = delete;
    SerialDevice& operator=(const SerialDevice&) = delete;

    // Opens the GPS tty (persist.vendor.gps.device, or found by USB vid:pid)
    // and applies termios. Returns false if no device resolves or it cannot
    // be opened.
    bool open();
    void close();

    bool isOpen() const { return mFd >= 0; }
    int fd() const { return mFd; }

    // Blocking read into buf; returns bytes read, 0 on EOF, -1 on error.
    ssize_t read(char* buf, size_t len);

    // Configure the receiver fix interval (UBX CFG-RATE). periodMs is clamped
    // by the caller to a sane range.
    void setMeasurementRate(unsigned short periodMs);

  private:
    // /dev path from persist.vendor.gps.device, else by scanning
    // /sys/class/tty for the persist.vendor.gps.usb vid:pid. "" if none.
    std::string resolveDevicePath();

    bool applyTermios();

    int mFd = -1;
    std::string mDevicePath;
};

}  // namespace aidl::android::hardware::gnss
