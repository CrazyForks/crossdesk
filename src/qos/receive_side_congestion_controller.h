/*
 * @Author: DI JUNKUN
 * @Date: 2024-12-12
 * Copyright (c) 2024 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _RECEIVE_SIDE_CONGESTION_CONTROLLER_H_
#define _RECEIVE_SIDE_CONGESTION_CONTROLLER_H_

class ReceiveSideCongestionController {
 public:
  enum MediaType { VIDEO, AUDIO, DATA };

 public:
  ReceiveSideCongestionController();
  ~ReceiveSideCongestionController() override = default;

 public:
  void OnReceivedPacket(const RtpPacketReceived& packet, MediaType media_type);

  // Implements CallStatsObserver.
  void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) override;

  // This is send bitrate, used to control the rate of feedback messages.
  void OnBitrateChanged(int bitrate_bps);

  // Ensures the remote party is notified of the receive bitrate no larger than
  // `bitrate` using RTCP REMB.
  void SetMaxDesiredReceiveBitrate(DataRate bitrate);

  void SetTransportOverhead(DataSize overhead_per_packet);

  // Returns latest receive side bandwidth estimation.
  // Returns zero if receive side bandwidth estimation is unavailable.
  DataRate LatestReceiveSideEstimate() const;

  // Removes stream from receive side bandwidth estimation.
  // Noop if receive side bwe is not used or stream doesn't participate in it.
  void RemoveStream(uint32_t ssrc);

  // Runs periodic tasks if it is time to run them, returns time until next
  // call to `MaybeProcess` should be non idle.
  int64_t MaybeProcess();

 private:
  void PickEstimator(bool has_absolute_send_time);

  RembThrottler remb_throttler_;

  // TODO: bugs.webrtc.org/42224904 - Use sequence checker for all usage of
  // ReceiveSideCongestionController. At the time of
  // writing OnReceivedPacket and MaybeProcess can unfortunately be called on an
  // arbitrary thread by external projects.
  SequenceChecker sequence_checker_;

  bool send_rfc8888_congestion_feedback_ = false;
  TransportSequenceNumberFeedbackGenenerator
      transport_sequence_number_feedback_generator_;
  CongestionControlFeedbackGenerator congestion_control_feedback_generator_
      RTC_GUARDED_BY(sequence_checker_);

  mutable Mutex mutex_;
  std::unique_ptr<RemoteBitrateEstimator> rbe_ RTC_GUARDED_BY(mutex_);
  bool using_absolute_send_time_ RTC_GUARDED_BY(mutex_);
  uint32_t packets_since_absolute_send_time_ RTC_GUARDED_BY(mutex_);
};

#endif