#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

#include <array>
#include <vector>

namespace {

constexpr const char kHbmPath[] = "/sys/kernel/display_drivers/hbm";

// Derived from the Goodix driver call sites that report FOD-UP/FOD-DOWN via
// mz_gesture_report().
constexpr unsigned short kFodGestureKeyCode = 0x272;

constexpr int kHbmOnValue = 6;
constexpr int kHbmOffValue = 7;

constexpr int kMaxInputDevices = 64;
constexpr int kPollTimeoutMs = 1000;
constexpr int kRescanIntervalSeconds = 5;
constexpr int kHbmAutoOffTimeoutMs = 400;

volatile sig_atomic_t gShouldExit = 0;

int64_t elapsed_realtime_ms() {
    timespec ts = {};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<int64_t>(ts.tv_sec) * 1000LL + ts.tv_nsec / 1000000LL;
}

void log_line(const char* level, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "mz_fp_hbm_daemon[%s]: ", level);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

void handle_signal(int /*signal*/) {
    gShouldExit = 1;
}

bool write_text_node(const char* path, const char* value) {
    int fd = TEMP_FAILURE_RETRY(open(path, O_WRONLY | O_CLOEXEC));
    if (fd < 0) {
        log_line("E", "open %s failed: %s", path, strerror(errno));
        return false;
    }

    const size_t len = strlen(value);
    const ssize_t written = TEMP_FAILURE_RETRY(write(fd, value, len));
    const int saved_errno = errno;
    close(fd);

    if (written != static_cast<ssize_t>(len)) {
        errno = saved_errno;
        log_line("E", "write %s failed: wrote=%zd err=%s", path, written, strerror(errno));
        return false;
    }

    return true;
}

bool write_int_node(const char* path, int value) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d\n", value);
    return write_text_node(path, buf);
}

class HbmController {
  public:
    void setState(bool on) {
        if (is_hbm_on_ == on) {
            if (on) {
                hbm_off_deadline_ms_ = elapsed_realtime_ms() + kHbmAutoOffTimeoutMs;
            }
            return;
        }

        if (write_int_node(kHbmPath, on ? kHbmOnValue : kHbmOffValue)) {
            is_hbm_on_ = on;
            hbm_off_deadline_ms_ = on ? (elapsed_realtime_ms() + kHbmAutoOffTimeoutMs) : 0;
            log_line("I", "HBM %s", on ? "on" : "off");
        }
    }

    int pollTimeoutMs() const {
        if (!is_hbm_on_ || hbm_off_deadline_ms_ == 0) {
            return kPollTimeoutMs;
        }

        const int64_t remaining_ms = hbm_off_deadline_ms_ - elapsed_realtime_ms();
        if (remaining_ms <= 0) {
            return 0;
        }

        return remaining_ms < kPollTimeoutMs ? static_cast<int>(remaining_ms) : kPollTimeoutMs;
    }

    void handleTimeout() {
        if (!is_hbm_on_ || hbm_off_deadline_ms_ == 0) {
            return;
        }

        if (elapsed_realtime_ms() < hbm_off_deadline_ms_) {
            return;
        }

        log_line("I", "HBM auto-off after %d ms", kHbmAutoOffTimeoutMs);
        setState(false);
    }

    void shutdown() {
        setState(false);
    }

  private:
    bool is_hbm_on_ = false;
    int64_t hbm_off_deadline_ms_ = 0;
};

struct InputDevice {
    int fd = -1;
};

void close_input_device(InputDevice* device) {
    if (device->fd >= 0) {
        close(device->fd);
        device->fd = -1;
    }
}

void scan_input_devices(std::array<InputDevice, kMaxInputDevices>* devices) {
    for (int i = 0; i < kMaxInputDevices; ++i) {
        if ((*devices)[i].fd >= 0) {
            continue;
        }

        char path[32];
        snprintf(path, sizeof(path), "/dev/input/event%d", i);

        const int fd = TEMP_FAILURE_RETRY(open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC));
        if (fd < 0) {
            continue;
        }

        (*devices)[i].fd = fd;
        log_line("I", "opened %s", path);
    }
}

bool handle_events(int fd, HbmController* hbm) {
    for (;;) {
        input_event ev = {};
        const ssize_t n = TEMP_FAILURE_RETRY(read(fd, &ev, sizeof(ev)));
        if (n == static_cast<ssize_t>(sizeof(ev))) {
            if (ev.type == EV_KEY && ev.code == kFodGestureKeyCode) {
                log_line("I", "FOD event code=0x%x value=%d", ev.code, ev.value);
                hbm->setState(ev.value != 0);
            }
            continue;
        }

        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return true;
        }

        if (n == 0) {
            log_line("E", "input device closed");
        } else if (n < 0) {
            log_line("E", "read input event failed: %s", strerror(errno));
        } else {
            log_line("E", "short read from input device: %zd", n);
        }
        return false;
    }
}

}  // namespace

int main() {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    std::array<InputDevice, kMaxInputDevices> devices = {};
    HbmController hbm;

    int seconds_since_rescan = kRescanIntervalSeconds;
    log_line("I", "daemon start, listening for FOD key 0x%x", kFodGestureKeyCode);
    hbm.setState(false);

    while (!gShouldExit) {
        if (seconds_since_rescan >= kRescanIntervalSeconds) {
            scan_input_devices(&devices);
            seconds_since_rescan = 0;
        }

        std::vector<pollfd> poll_fds;
        std::vector<int> poll_indexes;
        poll_fds.reserve(kMaxInputDevices);
        poll_indexes.reserve(kMaxInputDevices);

        for (int i = 0; i < kMaxInputDevices; ++i) {
            if (devices[i].fd < 0) {
                continue;
            }

            pollfd pfd = {};
            pfd.fd = devices[i].fd;
            pfd.events = POLLIN | POLLERR | POLLHUP;
            poll_fds.push_back(pfd);
            poll_indexes.push_back(i);
        }

        const int ret = TEMP_FAILURE_RETRY(
                poll(poll_fds.data(), poll_fds.size(), hbm.pollTimeoutMs()));
        if (ret < 0) {
            log_line("E", "poll failed: %s", strerror(errno));
            sleep(1);
            ++seconds_since_rescan;
            continue;
        }

        if (ret == 0) {
            hbm.handleTimeout();
            ++seconds_since_rescan;
            continue;
        }

        for (size_t i = 0; i < poll_fds.size(); ++i) {
            const short revents = poll_fds[i].revents;
            if (revents == 0) {
                continue;
            }

            const int index = poll_indexes[i];
            if ((revents & (POLLERR | POLLHUP | POLLNVAL)) != 0 ||
                !handle_events(devices[index].fd, &hbm)) {
                close_input_device(&devices[index]);
            }
        }

        hbm.handleTimeout();
    }

    for (auto& device : devices) {
        close_input_device(&device);
    }
    hbm.shutdown();
    return 0;
}
