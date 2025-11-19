#include "daemon.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <process.h>
#include <tchar.h>
#include <windows.h>
#elif __APPLE__
#include <fcntl.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#else
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstring>
#endif

#ifndef _WIN32
Daemon* Daemon::instance_ = nullptr;
#endif

// get executable file path
static std::string GetExecutablePath() {
#ifdef _WIN32
  char path[32768];
  DWORD length = GetModuleFileNameA(nullptr, path, sizeof(path));
  if (length > 0 && length < sizeof(path)) {
    return std::string(path);
  }
#elif __APPLE__
  char path[PATH_MAX];
  uint32_t size = sizeof(path);
  if (_NSGetExecutablePath(path, &size) == 0) {
    char resolved_path[PATH_MAX];
    if (realpath(path, resolved_path) != nullptr) {
      return std::string(resolved_path);
    }
    return std::string(path);
  }
#else
  char path[PATH_MAX];
  ssize_t count = readlink("/proc/self/exe", path, sizeof(path) - 1);
  if (count != -1) {
    path[count] = '\0';
    return std::string(path);
  }
#endif
  return "";
}

Daemon::Daemon(const std::string& name)
    : name_(name)
#ifdef _WIN32
      ,
      running_(false)
#else
      ,
      running_(true)
#endif
{
}

void Daemon::stop() {
#ifdef _WIN32
  running_ = false;
#else
  running_ = false;
#endif
}

bool Daemon::isRunning() const {
#ifdef _WIN32
  return running_;
#else
  return running_;
#endif
}

bool Daemon::start(MainLoopFunc loop) {
#ifdef _WIN32
  running_ = true;
  return runWithRestart(loop);
#elif __APPLE__
  // macOS: Use child process monitoring (like Windows) to preserve GUI
  running_ = true;
  return runWithRestart(loop);
#else
  // Linux: Use traditional daemonization
  instance_ = this;
  runUnix(loop);
  return true;
#endif
}

#ifdef _WIN32
// windows: execute loop and catch C++ exceptions
static int RunLoopCatchCpp(Daemon::MainLoopFunc& loop) {
  try {
    loop();
    return 0;  // normal exit
  } catch (const std::exception& e) {
    std::cerr << "Exception caught: " << e.what() << std::endl;
    return 1;  // c++ exception
  } catch (...) {
    std::cerr << "Unknown exception caught" << std::endl;
    return 1;  // other exception
  }
}

// windows: Use SEH wrapper function to catch system-level crashes
static int RunLoopWithSEH(Daemon::MainLoopFunc& loop) {
  __try {
    return RunLoopCatchCpp(loop);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    // Catch system-level crashes (access violation, divide by zero, etc.)
    DWORD code = GetExceptionCode();
    std::cerr << "System crash detected (SEH exception code: 0x" << std::hex
              << code << std::dec << ")" << std::endl;
    return 2;  // System crash
  }
}
#endif

// run function with restart logic (infinite restart)
// use child process monitoring: parent process creates child process to run
// main program, monitors child process status, restarts on crash
bool Daemon::runWithRestart(MainLoopFunc loop) {
  int restart_count = 0;
  std::string exe_path = GetExecutablePath();
  if (exe_path.empty()) {
    std::cerr
        << "Failed to get executable path, falling back to direct execution"
        << std::endl;
    // fallback to direct execution
    while (isRunning()) {
      try {
        loop();
        break;
      } catch (...) {
        restart_count++;
        std::cerr << "Exception caught, restarting... (attempt "
                  << restart_count << ")" << std::endl;
        std::this_thread::sleep_for(
            std::chrono::milliseconds(DAEMON_DEFAULT_RESTART_DELAY_MS));
      }
    }
    return true;
  }

  while (isRunning()) {
#ifdef _WIN32
    // windows: use CreateProcess to create child process
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi = {0};

    // build command line arguments (add --child flag)
    std::string cmd_line = "\"" + exe_path + "\" --child";
    std::vector<char> cmd_line_buf(cmd_line.begin(), cmd_line.end());
    cmd_line_buf.push_back('\0');

    BOOL success = CreateProcessA(
        nullptr,  // executable file path (specified in command line)
        cmd_line_buf.data(),  // command line arguments
        nullptr,              // process security attributes
        nullptr,              // thread security attributes
        FALSE,                // don't inherit handles
        0,                    // creation flags
        nullptr,              // environment variables (inherit from parent)
        nullptr,              // current directory
        &si,                  // startup info
        &pi                   // process information
    );

    if (!success) {
      std::cerr << "Failed to create child process, error: " << GetLastError()
                << std::endl;
      std::this_thread::sleep_for(
          std::chrono::milliseconds(DAEMON_DEFAULT_RESTART_DELAY_MS));
      restart_count++;
      continue;
    }

    // wait for child process to exit
    DWORD exit_code = 0;
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exit_code == 0) {
      // normal exit
      break;
    } else {
      // abnormal exit, restart
      restart_count++;
      std::cerr << "Child process exited with code " << exit_code
                << ", restarting... (attempt " << restart_count << ")"
                << std::endl;
      std::this_thread::sleep_for(
          std::chrono::milliseconds(DAEMON_DEFAULT_RESTART_DELAY_MS));
    }
#else
    // linux: use fork + exec to create child process
    pid_t pid = fork();
    if (pid == 0) {
      // child process: execute main program, pass --child argument
      execl(exe_path.c_str(), exe_path.c_str(), "--child", nullptr);
      // if exec fails, exit
      _exit(1);
    } else if (pid > 0) {
      // parent process: wait for child process to exit
      int status = 0;
      waitpid(pid, &status, 0);

      if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        if (exit_code == 0) {
          // normal exit
          break;
        } else {
          // abnormal exit, restart
          restart_count++;
          std::cerr << "Child process exited with code " << exit_code
                    << ", restarting... (attempt " << restart_count << ")"
                    << std::endl;
          std::this_thread::sleep_for(
              std::chrono::milliseconds(DAEMON_DEFAULT_RESTART_DELAY_MS));
        }
      } else if (WIFSIGNALED(status)) {
        // child process terminated by signal (crash)
        int sig = WTERMSIG(status);
        restart_count++;
        std::cerr << "Child process crashed with signal " << sig
                  << ", restarting... (attempt " << restart_count << ")"
                  << std::endl;
        std::this_thread::sleep_for(
            std::chrono::milliseconds(DAEMON_DEFAULT_RESTART_DELAY_MS));
      }
    } else {
      // fork failed
      std::cerr << "Failed to fork child process" << std::endl;
      std::this_thread::sleep_for(
          std::chrono::milliseconds(DAEMON_DEFAULT_RESTART_DELAY_MS));
      restart_count++;
    }
#endif
  }

  return true;
}

#ifndef _WIN32
void Daemon::runUnix(MainLoopFunc loop) {
  struct sigaction sa;
  sa.sa_handler = [](int) {};
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGPIPE, &sa, nullptr);
  sigaction(SIGCHLD, &sa, nullptr);

  // daemon mode: fork twice, redirect output
  pid_t pid = fork();
  if (pid > 0) _exit(0);
  setsid();
  pid = fork();
  if (pid > 0) _exit(0);
  umask(0);
  chdir("/");
  int fd = open("/dev/null", O_RDWR);
  if (fd >= 0) {
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    if (fd > 2) close(fd);
  }

  // catch signals for exit
  signal(SIGTERM, [](int) { instance_->stop(); });
  signal(SIGINT, [](int) { instance_->stop(); });

  // catch crash signals, use atomic flags to mark crash
  static std::atomic<bool> crash_detected{false};
  static std::atomic<bool> should_restart{false};

  // use safer signal handling: only set flags, don't call longjmp
  struct sigaction sa_crash;
  sa_crash.sa_handler = [](int sig) {
    const char* sig_name = "Unknown";
    switch (sig) {
      case SIGSEGV:
        sig_name = "SIGSEGV";
        break;
      case SIGABRT:
        sig_name = "SIGABRT";
        break;
      case SIGFPE:
        sig_name = "SIGFPE";
        break;
      case SIGILL:
        sig_name = "SIGILL";
        break;
    }
    std::cerr << "Crash signal detected: " << sig_name
              << ", will restart after process exits" << std::endl;
    crash_detected = true;
    should_restart = true;
    // don't call longjmp, let program exit normally, restart by monitoring
    // thread
  };
  sigemptyset(&sa_crash.sa_mask);
  sa_crash.sa_flags = SA_RESETHAND;  // handle only once, avoid recursion

  sigaction(SIGSEGV, &sa_crash, nullptr);
  sigaction(SIGABRT, &sa_crash, nullptr);
  sigaction(SIGFPE, &sa_crash, nullptr);
  sigaction(SIGILL, &sa_crash, nullptr);

  running_ = true;

  // run with restart logic (infinite restart)
  // run main loop in separate thread, main thread monitors thread status
  int restart_count = 0;
  while (running_) {
    crash_detected = false;
    should_restart = false;
    std::atomic<bool> loop_completed{false};
    std::exception_ptr loop_exception = nullptr;

    // run main loop in separate thread
    std::thread loop_thread([&loop, &loop_completed, &loop_exception]() {
      try {
        loop();
        loop_completed = true;
      } catch (const std::exception& e) {
        loop_exception = std::current_exception();
        loop_completed = true;
      } catch (...) {
        loop_exception = std::current_exception();
        loop_completed = true;
      }
    });

    // wait for thread to complete
    loop_thread.join();

    // check exit reason
    if (loop_exception) {
      restart_count++;
      try {
        std::rethrow_exception(loop_exception);
      } catch (const std::exception& e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
      } catch (...) {
        std::cerr << "Unknown exception caught" << std::endl;
      }
      std::cerr << "Restarting... (attempt " << restart_count << ")"
                << std::endl;
      std::this_thread::sleep_for(
          std::chrono::milliseconds(DAEMON_DEFAULT_RESTART_DELAY_MS));
      continue;
    }

    // check if crash signal detected
    if (crash_detected || should_restart) {
      restart_count++;
      std::cerr << "Crash detected, restarting... (attempt " << restart_count
                << ")" << std::endl;
      std::this_thread::sleep_for(
          std::chrono::milliseconds(DAEMON_DEFAULT_RESTART_DELAY_MS));
      continue;
    }

    // normal exit
    if (loop_completed) {
      break;
    }
  }
}
#endif
