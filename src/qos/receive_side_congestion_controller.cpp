/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "receive_side_congestion_controller.h"

#include <algorithm>
#include <memory>
#include <utility>

namespace {
static const uint32_t kTimeOffsetSwitchThreshold = 30;
}  // namespace

void ReceiveSideCongestionController::OnRttUpdate(int64_t avg_rtt_ms,
                                                  int64_t max_rtt_ms) {
  std::lock_guard<std::mutex> guard(mutex_);
  rbe_->OnRttUpdate(avg_rtt_ms, max_rtt_ms);
}

void ReceiveSideCongestionController::RemoveStream(uint32_t ssrc) {
  std::lock_guard<std::mutex> guard(mutex_);
  rbe_->RemoveStream(ssrc);
}

int64_t ReceiveSideCongestionController::LatestReceiveSideEstimate() const {
  std::lock_guard<std::mutex> guard(mutex_);
  return rbe_->LatestEstimate();
}

void ReceiveSideCongestionController::PickEstimator(
    bool has_absolute_send_time) {
  if (has_absolute_send_time) {
    // If we see AST in header, switch RBE strategy immediately.
    if (!using_absolute_send_time_) {
      RTC_LOG(LS_INFO)
          << "WrappingBitrateEstimator: Switching to absolute send time RBE.";
      using_absolute_send_time_ = true;
      // rbe_ = std::make_unique<RemoteBitrateEstimatorAbsSendTime>(
      //     env_, &remb_throttler_);
    }
    packets_since_absolute_send_time_ = 0;
  } else {
    // When we don't see AST, wait for a few packets before going back to TOF.
    if (using_absolute_send_time_) {
      ++packets_since_absolute_send_time_;
      if (packets_since_absolute_send_time_ >= kTimeOffsetSwitchThreshold) {
        RTC_LOG(LS_INFO)
            << "WrappingBitrateEstimator: Switching to transmission "
               "time offset RBE.";
        using_absolute_send_time_ = false;
        // rbe_ = std::make_unique<RemoteBitrateEstimatorSingleStream>(
        //     env_, &remb_throttler_);
      }
    }
  }
}

ReceiveSideCongestionController::ReceiveSideCongestionController(
    const Environment& env,
    TransportSequenceNumberFeedbackGenenerator::RtcpSender feedback_sender,
    RembThrottler::RembSender remb_sender,
    absl::Nullable<NetworkStateEstimator*> network_state_estimator)
    : env_(env),
      // remb_throttler_(std::move(remb_sender), &env_.clock()),,
      congestion_control_feedback_generator_(env, feedback_sender),
      // rbe_(std::make_unique<RemoteBitrateEstimatorSingleStream>(
      //     env_, &remb_throttler_)),
      using_absolute_send_time_(false),
      packets_since_absolute_send_time_(0) {
  FieldTrialParameter<bool> force_send_rfc8888_feedback("force_send", false);
  ParseFieldTrial(
      {&force_send_rfc8888_feedback},
      env.field_trials().Lookup("WebRTC-RFC8888CongestionControlFeedback"));
  if (force_send_rfc8888_feedback) {
    EnablSendCongestionControlFeedbackAccordingToRfc8888();
  }
}

void ReceiveSideCongestionController::
    EnablSendCongestionControlFeedbackAccordingToRfc8888() {
  // RTC_DCHECK_RUN_ON(&sequence_checker_);
  send_rfc8888_congestion_feedback_ = true;
}

void ReceiveSideCongestionController::OnReceivedPacket(
    const RtpPacketReceived& packet, MediaType media_type) {
  if (send_rfc8888_congestion_feedback_) {
    // RTC_DCHECK_RUN_ON(&sequence_checker_);
    congestion_control_feedback_generator_.OnReceivedPacket(packet);
    return;
  }
}

void ReceiveSideCongestionController::OnBitrateChanged(int bitrate_bps) {
  // RTC_DCHECK_RUN_ON(&sequence_checker_);
  int64_t send_bandwidth_estimate = int64_t::BitsPerSec(bitrate_bps);
  congestion_control_feedback_generator_.OnSendBandwidthEstimateChanged(
      send_bandwidth_estimate);
}

int64_t ReceiveSideCongestionController::MaybeProcess() {
  int64_t now = env_.clock().CurrentTime();
  if (send_rfc8888_congestion_feedback_) {
    // RTC_DCHECK_RUN_ON(&sequence_checker_);
    return congestion_control_feedback_generator_.Process(now);
  }
}

void ReceiveSideCongestionController::SetMaxDesiredReceiveBitrate(
    int64_t bitrate) {
  // remb_throttler_.SetMaxDesiredReceiveBitrate(bitrate);
}

void ReceiveSideCongestionController::SetTransportOverhead(
    int64_t overhead_per_packet) {
  // RTC_DCHECK_RUN_ON(&sequence_checker_);
  congestion_control_feedback_generator_.SetTransportOverhead(
      overhead_per_packet);
}
