/*
 * Copyright (C) 2009 Michael Trimarchi
 * Copyright (C) 2015 Keith Conger
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

#define LOG_TAG "gnss_serial_dev"

#include "SerialDevice.h"

#include <cutils/properties.h>
#include <log/log.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <vector>

namespace aidl::android::hardware::gnss {

namespace {
speed_t baudFromString(const char* s) {
    if (!strcmp(s, "4800")) return B4800;
    if (!strcmp(s, "9600")) return B9600;
    if (!strcmp(s, "19200")) return B19200;
    if (!strcmp(s, "38400")) return B38400;
    if (!strcmp(s, "57600")) return B57600;
    if (!strcmp(s, "115200")) return B115200;
    return 0;
}

bool parseUsbId(const char* s, unsigned* vid, unsigned* pid) {
    int consumed = 0;
    return sscanf(s, "%4x:%4x%n", vid, pid, &consumed) == 2 && s[consumed] == '\0';
}

// Small sysfs attribute (e.g. idVendor), trimmed; "" if absent/unreadable.
std::string readSysfsAttr(const std::string& path) {
    int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) return "";
    char buf[32];
    ssize_t n = ::read(fd, buf, sizeof(buf) - 1);
    ::close(fd);
    if (n <= 0) return "";
    buf[n] = '\0';
    std::string s(buf);
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    return s;
}

// True if the tty's ancestor USB device has the given idVendor/idProduct.
// /sys/class/tty/<name>/device lands on the port or interface device; the
// USB device that carries the ids is a level or two further up, so walk up.
bool ttyMatchesUsbId(const std::string& name, unsigned vid, unsigned pid) {
    std::string node = "/sys/class/tty/" + name + "/device";
    for (int depth = 0; depth < 4; ++depth) {
        std::string v = readSysfsAttr(node + "/idVendor");
        std::string p = readSysfsAttr(node + "/idProduct");
        if (!v.empty() && !p.empty()) {
            return strtoul(v.c_str(), nullptr, 16) == vid &&
                   strtoul(p.c_str(), nullptr, 16) == pid;
        }
        node += "/..";
    }
    return false;
}

// Name (without /dev/) of the first tty whose USB device matches vid:pid.
std::string findUsbSerialDevice(unsigned vid, unsigned pid) {
    DIR* dir = opendir("/sys/class/tty");
    if (dir == nullptr) {
        ALOGE("cannot scan /sys/class/tty: %s", strerror(errno));
        return "";
    }
    std::vector<std::string> matches;
    while (struct dirent* e = readdir(dir)) {
        if (e->d_name[0] == '.') continue;
        if (ttyMatchesUsbId(e->d_name, vid, pid)) matches.emplace_back(e->d_name);
    }
    closedir(dir);
    if (matches.empty()) return "";
    std::sort(matches.begin(), matches.end());
    if (matches.size() > 1) {
        ALOGW("%zu ttys match USB %04x:%04x; using %s (set persist.vendor.gps.device to override)",
              matches.size(), vid, pid, matches.front().c_str());
    }
    return matches.front();
}
}  // namespace

std::string SerialDevice::resolveDevicePath() {
    char prop[PROPERTY_VALUE_MAX];

    // Explicit node name wins.
    if (property_get("persist.vendor.gps.device", prop, "") > 0) {
        return std::string("/dev/") + prop;
    }

    // Otherwise locate the receiver by USB vid:pid, immune to enumeration
    // order. Default: Prolific PL2303 (GlobalSat BU-353 and friends).
    property_get("persist.vendor.gps.usb", prop, "067b:2303");
    unsigned vid = 0, pid = 0;
    if (!parseUsbId(prop, &vid, &pid)) {
        ALOGE("invalid persist.vendor.gps.usb '%s' (expected vid:pid, e.g. 067b:2303)", prop);
        return "";
    }
    std::string name = findUsbSerialDevice(vid, pid);
    if (name.empty()) {
        ALOGE("no tty with USB id %04x:%04x found and persist.vendor.gps.device is not set",
              vid, pid);
        return "";
    }
    ALOGI("resolved USB %04x:%04x -> /dev/%s", vid, pid, name.c_str());
    return "/dev/" + name;
}

bool SerialDevice::open() {
    mDevicePath = resolveDevicePath();
    if (mDevicePath.empty()) return false;

    do {
        mFd = ::open(mDevicePath.c_str(), O_RDWR | O_NOCTTY);
    } while (mFd < 0 && errno == EINTR);

    if (mFd < 0) {
        ALOGE("could not open gps serial device %s: %s", mDevicePath.c_str(), strerror(errno));
        return false;
    }

    ALOGI("opened gps serial device %s", mDevicePath.c_str());
    if (!applyTermios()) {
        close();
        return false;
    }
    return true;
}

bool SerialDevice::applyTermios() {
    if (!isatty(mFd)) {
        ALOGW("%s is not a tty; using raw fd", mDevicePath.c_str());
        return true;
    }

    char prop[PROPERTY_VALUE_MAX];
    property_get("persist.vendor.gps.ttybaud", prop, "4800");
    speed_t baud = baudFromString(prop);
    if (baud == 0) {
        ALOGE("unknown gps baud rate '%s'", prop);
        return false;
    }
    ALOGI("setting gps baud rate to %s", prop);

    struct termios ios;
    tcgetattr(mFd, &ios);
    ios.c_lflag = 0;                       // raw: no ECHO/ICANON
    ios.c_oflag &= ~ONLCR;                 // no \n -> \r\n on output
    ios.c_iflag &= ~(ICRNL | INLCR);       // no CR/LF translation on input
    ios.c_iflag |= (IGNCR | IXOFF);        // ignore CR, software flow control
    ios.c_cflag = baud | CRTSCTS | CS8 | CLOCAL | CREAD;
    tcsetattr(mFd, TCSANOW, &ios);
    return true;
}

void SerialDevice::close() {
    if (mFd >= 0) {
        ::close(mFd);
        mFd = -1;
    }
}

ssize_t SerialDevice::read(char* buf, size_t len) {
    ssize_t ret;
    do {
        ret = ::read(mFd, buf, len);
    } while (ret < 0 && errno == EINTR);
    return ret;
}

// ---- UBX CFG-RATE -----------------------------------------------------------

static void ubxChecksum(const unsigned char* msg, int size, unsigned char* ckA,
                        unsigned char* ckB) {
    *ckA = *ckB = 0;
    for (int i = 0; i < size; i++) {
        *ckA += msg[i];
        *ckB += *ckA;
    }
}

void SerialDevice::setMeasurementRate(unsigned short periodMs) {
    if (mFd < 0) return;

    // B5 62 06 08 | len=06 00 | measRate(2) navRate(2) timeRef(2) | ckA ckB
    unsigned char buff[14] = {0xB5, 0x62, 0x06, 0x08, 0x06, 0x00};
    buff[6] = periodMs & 0xFF;
    buff[7] = (periodMs >> 8) & 0xFF;
    buff[8] = 1;  // navRate
    buff[9] = 0;
    buff[10] = 1;  // timeRef = GPS time
    buff[11] = 0;
    ubxChecksum(buff + 2, 10, &buff[12], &buff[13]);

    size_t n = 0;
    while (n < sizeof(buff)) {
        ssize_t ret = ::write(mFd, buff + n, sizeof(buff) - n);
        if (ret < 0) {
            if (errno == EINTR) continue;
            ALOGW("failed writing UBX CFG-RATE: %s", strerror(errno));
            break;
        }
        n += ret;
    }
}

}  // namespace aidl::android::hardware::gnss
