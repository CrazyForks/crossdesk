#include "thread_base.h"

#include "log.h"

ThreadBase::ThreadBase() {}

ThreadBase::~ThreadBase() {
  if (!stop_) {
    Stop();
  }
}

void ThreadBase::Start() {
  std::thread t(&ThreadBase::Run, this);
  thread_ = std::move(t);
  stop_ = false;
}

void ThreadBase::Stop() {
  if (thread_.joinable()) {
    stop_ = true;
    thread_.join();
  }
}

void ThreadBase::Pause() { pause_ = true; }

void ThreadBase::Resume() { pause_ = false; }

void ThreadBase::Run() {
  while (!stop_ && Process()) {
  }
}