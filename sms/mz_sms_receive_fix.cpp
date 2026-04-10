#include <errno.h>
#include <android/log.h>
#include <android-base/properties.h>
#include <android-base/strings.h>
#include <binder/IBinder.h>
#include <binder/IResultReceiver.h>
#include <binder/IServiceManager.h>
#include <binder/IShellCallback.h>
#include <binder/ProcessState.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <vector>

namespace {

constexpr const char kConnectivityServiceName[] = "connectivity";
constexpr const char kSettingsServiceName[] = "settings";
constexpr const char kAirplaneModeSetting[] = "airplane_mode_on";
constexpr const char kRestoreProp[] = "persist.sys.mz.sms_receive_fix_restore";
constexpr const char kLogTag[] = "mz_sms_receive_fix";
constexpr int kShutdownMaxAttempts = 5;
constexpr int kBootPrepareWaitMaxAttempts = 240;
constexpr int kBootPrepareSetMaxAttempts = 20;
constexpr int kBootCompleteMaxAttempts = 40;
constexpr useconds_t kBootCompleteDelayUs = 1000000;
constexpr int kShellCommandTimeoutMs = 2000;
constexpr useconds_t kRetryDelayUs = 250000;

using android::IBinder;
using android::OK;
using android::ProcessState;
using android::String16;
using android::Vector;
using android::defaultServiceManager;
using android::sp;

void log_line(int priority, const char* level, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    __android_log_vprint(priority, kLogTag, fmt, ap);
    va_end(ap);

    va_start(ap, fmt);
    fprintf(stderr, "mz_sms_receive_fix[%s]: ", level);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

class ResultReceiver final : public android::BnResultReceiver {
  public:
    void send(int32_t resultCode) override {
        std::lock_guard<std::mutex> lock(mutex_);
        result_code_ = resultCode;
        have_result_ = true;
        cv_.notify_all();
    }

    bool waitForResult(int timeout_ms, int32_t* result_code) {
        std::unique_lock<std::mutex> lock(mutex_);
        const bool ok = cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                     [this]() { return have_result_; });
        if (!ok) {
            return false;
        }
        *result_code = result_code_;
        return true;
    }

  private:
    std::mutex mutex_;
    std::condition_variable cv_;
    bool have_result_ = false;
    int32_t result_code_ = -1;
};

enum class AirplaneModeState {
    kDisabled,
    kEnabled,
    kUnknown,
};

struct ShellCommandResult {
    android::status_t binder_status = android::UNKNOWN_ERROR;
    int32_t result_code = -1;
    std::string stdout_text;
    std::string stderr_text;
};

bool read_fd_to_string(int fd, std::string* out) {
    out->clear();

    char buf[256];
    for (;;) {
        const ssize_t n = TEMP_FAILURE_RETRY(read(fd, buf, sizeof(buf)));
        if (n == 0) {
            return true;
        }
        if (n < 0) {
            log_line(ANDROID_LOG_ERROR, "E", "read fd %d failed: %s", fd, strerror(errno));
            return false;
        }
        out->append(buf, static_cast<size_t>(n));
    }
}

bool close_fd(int* fd) {
    if (*fd < 0) {
        return true;
    }

    const bool ok = TEMP_FAILURE_RETRY(close(*fd)) == 0;
    if (!ok) {
        log_line(ANDROID_LOG_ERROR, "E", "close fd %d failed: %s", *fd, strerror(errno));
    }
    *fd = -1;
    return ok;
}

ShellCommandResult run_service_shell_command(const char* service_name,
                                             const std::vector<std::string>& args) {
    ShellCommandResult result;

    int stdin_fd = TEMP_FAILURE_RETRY(open("/dev/null", O_RDONLY | O_CLOEXEC));
    if (stdin_fd < 0) {
        log_line(ANDROID_LOG_ERROR, "E", "open /dev/null failed: %s", strerror(errno));
        return result;
    }

    int stdout_pipe[2] = {-1, -1};
    int stderr_pipe[2] = {-1, -1};
    if (pipe(stdout_pipe) != 0) {
        log_line(ANDROID_LOG_ERROR, "E", "pipe stdout failed: %s", strerror(errno));
        close_fd(&stdin_fd);
        return result;
    }
    if (pipe(stderr_pipe) != 0) {
        log_line(ANDROID_LOG_ERROR, "E", "pipe stderr failed: %s", strerror(errno));
        close_fd(&stdin_fd);
        close_fd(&stdout_pipe[0]);
        close_fd(&stdout_pipe[1]);
        return result;
    }

    const auto sm = defaultServiceManager();
    if (sm == nullptr) {
        log_line(ANDROID_LOG_ERROR, "E", "servicemanager unavailable for %s", service_name);
        close_fd(&stdin_fd);
        close_fd(&stdout_pipe[0]);
        close_fd(&stdout_pipe[1]);
        close_fd(&stderr_pipe[0]);
        close_fd(&stderr_pipe[1]);
        return result;
    }

    const sp<IBinder> service = sm->checkService(String16(service_name));
    if (service == nullptr) {
        log_line(ANDROID_LOG_ERROR, "E", "%s service unavailable", service_name);
        close_fd(&stdin_fd);
        close_fd(&stdout_pipe[0]);
        close_fd(&stdout_pipe[1]);
        close_fd(&stderr_pipe[0]);
        close_fd(&stderr_pipe[1]);
        return result;
    }

    Vector<String16> binder_args;
    for (const auto& arg : args) {
        binder_args.add(String16(arg.c_str()));
    }

    sp<ResultReceiver> result_receiver = new ResultReceiver();
    sp<android::IShellCallback> shell_callback;

    result.binder_status = IBinder::shellCommand(service, stdin_fd, stdout_pipe[1],
                                                 stderr_pipe[1], binder_args, shell_callback,
                                                 result_receiver);

    close_fd(&stdin_fd);
    close_fd(&stdout_pipe[1]);
    close_fd(&stderr_pipe[1]);

    if (result.binder_status == OK) {
        if (!result_receiver->waitForResult(kShellCommandTimeoutMs, &result.result_code)) {
            log_line(ANDROID_LOG_ERROR, "E", "timed out waiting for %s shellCommand result",
                     service_name);
            result.binder_status = android::TIMED_OUT;
        }
    } else {
        log_line(ANDROID_LOG_ERROR, "E", "%s shellCommand transact failed: %d", service_name,
                 result.binder_status);
    }

    read_fd_to_string(stdout_pipe[0], &result.stdout_text);
    read_fd_to_string(stderr_pipe[0], &result.stderr_text);
    close_fd(&stdout_pipe[0]);
    close_fd(&stderr_pipe[0]);

    if (result.result_code != 0) {
        log_line(ANDROID_LOG_ERROR, "E", "%s shellCommand returned %d, out='%s', err='%s'",
                 service_name,
                 result.result_code, android::base::Trim(result.stdout_text).c_str(),
                 android::base::Trim(result.stderr_text).c_str());
    }

    return result;
}

ShellCommandResult run_connectivity_shell_command(const std::vector<std::string>& args) {
    return run_service_shell_command(kConnectivityServiceName, args);
}

bool set_airplane_mode_with_retry(bool enabled, int max_attempts) {
    const char* command = enabled ? "enable" : "disable";
    for (int i = 0; i < max_attempts; ++i) {
        const ShellCommandResult result =
                run_connectivity_shell_command({"airplane-mode", command});
        if (result.binder_status == OK && result.result_code == 0) {
            return true;
        }
        usleep(kRetryDelayUs);
    }

    return false;
}

bool set_global_setting_with_retry(const char* key, const char* value, int max_attempts) {
    for (int i = 0; i < max_attempts; ++i) {
        const ShellCommandResult result =
                run_service_shell_command(kSettingsServiceName, {"put", "global", key, value});
        if (result.binder_status == OK && result.result_code == 0) {
            return true;
        }
        usleep(kRetryDelayUs);
    }

    return false;
}

AirplaneModeState query_airplane_mode_once() {
    const ShellCommandResult result = run_connectivity_shell_command({"airplane-mode"});
    if (result.binder_status != OK || result.result_code != 0) {
        return AirplaneModeState::kUnknown;
    }

    const std::string state = android::base::Trim(result.stdout_text);
    if (state == "enabled") {
        return AirplaneModeState::kEnabled;
    }
    if (state == "disabled") {
        return AirplaneModeState::kDisabled;
    }

    log_line(ANDROID_LOG_ERROR, "E", "unexpected airplane-mode output: '%s'", state.c_str());
    return AirplaneModeState::kUnknown;
}

AirplaneModeState query_airplane_mode_with_retry(int max_attempts) {
    for (int i = 0; i < max_attempts; ++i) {
        const AirplaneModeState state = query_airplane_mode_once();
        if (state != AirplaneModeState::kUnknown) {
            return state;
        }
        usleep(kRetryDelayUs);
    }

    return AirplaneModeState::kUnknown;
}

bool wait_for_service(const char* service_name, int max_attempts) {
    for (int i = 0; i < max_attempts; ++i) {
        const auto sm = defaultServiceManager();
        if (sm != nullptr && sm->checkService(String16(service_name)) != nullptr) {
            return true;
        }
        usleep(kRetryDelayUs);
    }

    log_line(ANDROID_LOG_ERROR, "E", "%s service unavailable after %d attempts", service_name,
             max_attempts);
    return false;
}

bool set_restore_flag(bool enabled) {
    if (!android::base::SetProperty(kRestoreProp, enabled ? "1" : "0")) {
        log_line(ANDROID_LOG_ERROR, "E", "failed to set %s=%d", kRestoreProp,
                 enabled ? 1 : 0);
        return false;
    }
    return true;
}

bool should_restore_after_boot() {
    return android::base::GetBoolProperty(kRestoreProp, false);
}

int handle_shutdown() {
    const AirplaneModeState state = query_airplane_mode_with_retry(kShutdownMaxAttempts);
    if (state == AirplaneModeState::kUnknown) {
        log_line(ANDROID_LOG_ERROR, "E", "failed to query airplane mode during shutdown");
        return 1;
    }

    if (state == AirplaneModeState::kEnabled) {
        log_line(ANDROID_LOG_INFO, "I", "airplane mode already enabled before shutdown");
        set_restore_flag(false);
        return 0;
    }

    if (!set_restore_flag(true)) {
        return 1;
    }

    if (!set_airplane_mode_with_retry(true, kShutdownMaxAttempts)) {
        log_line(ANDROID_LOG_ERROR, "E", "failed to enable airplane mode during shutdown");
        set_restore_flag(false);
        return 1;
    }

    log_line(ANDROID_LOG_INFO, "I", "enabled airplane mode for shutdown");
    return 0;
}

int handle_boot_prepare() {
    if (!should_restore_after_boot()) {
        return 0;
    }

    if (!wait_for_service(kSettingsServiceName, kBootPrepareWaitMaxAttempts)) {
        return 1;
    }

    if (!set_global_setting_with_retry(kAirplaneModeSetting, "1", kBootPrepareSetMaxAttempts)) {
        log_line(ANDROID_LOG_ERROR, "E", "failed to restore airplane mode during boot prepare");
        return 1;
    }

    log_line(ANDROID_LOG_INFO, "I", "restored airplane mode during boot prepare");
    return 0;
}

int handle_boot_complete() {
    if (!should_restore_after_boot()) {
        return 0;
    }

    log_line(ANDROID_LOG_INFO, "I",
             "waiting %u ms before powering radio on after boot completed",
             kBootCompleteDelayUs / 1000);
    usleep(kBootCompleteDelayUs);

    if (!set_airplane_mode_with_retry(false, kBootCompleteMaxAttempts)) {
        log_line(ANDROID_LOG_ERROR, "E", "failed to disable airplane mode after boot completed");
        return 1;
    }

    if (!set_restore_flag(false)) {
        return 1;
    }

    log_line(ANDROID_LOG_INFO, "I", "disabled airplane mode after boot completed");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    ProcessState::self()->startThreadPool();

    if (argc != 2) {
        fprintf(stderr, "usage: %s <shutdown|boot-prepare|boot-complete>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "shutdown") == 0) {
        return handle_shutdown();
    }
    if (strcmp(argv[1], "boot-prepare") == 0) {
        return handle_boot_prepare();
    }
    if (strcmp(argv[1], "boot-complete") == 0) {
        return handle_boot_complete();
    }

    fprintf(stderr, "unknown mode: %s\n", argv[1]);
    return 1;
}
