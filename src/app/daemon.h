/*
 * @Author: DI JUNKUN
 * @Date: 2025-11-19
 * Copyright (c) 2025 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _DAEMON_H_
#define _DAEMON_H_

#include <functional>
#include <string>

// default restart delay (milliseconds)
#define DAEMON_DEFAULT_RESTART_DELAY_MS 1000

class Daemon {
 public:
  using MainLoopFunc = std::function<void()>;

  Daemon(const std::string& name);

  // start daemon (restart after 1 second by default)
  bool start(MainLoopFunc loop);

  // request exit
  void stop();

  // check if running
  bool isRunning() const;

 private:
  std::string name_;
  bool runWithRestart(MainLoopFunc loop);

#ifdef _WIN32
  bool running_;
#else
  static Daemon* instance_;
  void runUnix(MainLoopFunc loop);
  volatile bool running_;
#endif
};

#endif