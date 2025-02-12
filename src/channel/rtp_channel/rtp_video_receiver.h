#ifndef _RTP_VIDEO_RECEIVER_H_
#define _RTP_VIDEO_RECEIVER_H_

#include <functional>
#include <map>
#include <queue>
#include <set>

#include "clock.h"
#include "fec_decoder.h"
#include "io_statistics.h"
#include "nack_requester.h"
#include "receive_side_congestion_controller.h"
#include "ringbuffer.h"
#include "rtcp_receiver_report.h"
#include "rtp_packet_av1.h"
#include "rtp_packet_h264.h"
#include "rtp_rtcp_defines.h"
#include "rtp_statistics.h"
#include "thread_base.h"
#include "video_frame.h"

using namespace webrtc;

class RtpVideoReceiver : public ThreadBase,
                         public LossNotificationSender,
                         public KeyFrameRequestSender,
                         public NackSender {
 public:
  RtpVideoReceiver(std::shared_ptr<Clock> clock);
  RtpVideoReceiver(std::shared_ptr<Clock> clock,
                   std::shared_ptr<IOStatistics> io_statistics);
  virtual ~RtpVideoReceiver();

 public:
  void InsertRtpPacket(RtpPacket& rtp_packet);

  void SetSendDataFunc(std::function<int(const char*, size_t)> data_send_func);

  void SetOnReceiveCompleteFrame(
      std::function<void(VideoFrame&)> on_receive_complete_frame) {
    on_receive_complete_frame_ = on_receive_complete_frame;
  }

 private:
  void ProcessAv1RtpPacket(RtpPacketAv1& rtp_packet_av1);
  bool CheckIsAv1FrameCompleted(RtpPacketAv1& rtp_packet_av1);

 private:
  void ProcessH264RtpPacket(RtpPacketH264& rtp_packet_h264);
  bool CheckIsH264FrameCompleted(RtpPacketH264& rtp_packet_h264);

 private:
  bool CheckIsTimeSendRR();
  int SendRtcpRR(RtcpReceiverReport& rtcp_rr);

  void SendCombinedRtcpPacket(
      std::vector<std::unique_ptr<RtcpPacket>> rtcp_packets);

  void SendRemb(int64_t bitrate_bps, std::vector<uint32_t> ssrcs);

 private:
  bool Process() override;
  void RtcpThread();

 private:
  std::map<uint16_t, RtpPacketH264> incomplete_h264_frame_list_;
  std::map<uint16_t, RtpPacketAv1> incomplete_av1_frame_list_;
  std::map<uint16_t, RtpPacket> incomplete_frame_list_;
  uint8_t* nv12_data_ = nullptr;
  std::function<void(VideoFrame&)> on_receive_complete_frame_ = nullptr;
  uint32_t last_complete_frame_ts_ = 0;
  RingBuffer<VideoFrame> compelete_video_frame_queue_;

 private:
  std::unique_ptr<RtpStatistics> rtp_statistics_ = nullptr;
  std::shared_ptr<IOStatistics> io_statistics_ = nullptr;
  uint32_t last_recv_bytes_ = 0;
  uint32_t total_rtp_packets_recv_ = 0;
  uint32_t total_rtp_payload_recv_ = 0;

  uint32_t last_send_rtcp_rr_packet_ts_ = 0;
  std::function<int(const char*, size_t)> data_send_func_ = nullptr;

 private:
  bool fec_enable_ = false;
  FecDecoder fec_decoder_;
  uint64_t last_packet_ts_ = 0;
  // std::map<uint16_t, RtpPacket> incomplete_fec_frame_list_;
  // std::map<uint32_t, std::map<uint16_t, RtpPacket>> fec_source_symbol_list_;
  // std::map<uint32_t, std::map<uint16_t, RtpPacket>> fec_repair_symbol_list_;
  std::set<uint64_t> incomplete_fec_frame_list_;
  std::map<uint64_t, std::map<uint16_t, RtpPacket>> incomplete_fec_packet_list_;

 private:
  std::thread rtcp_thread_;
  std::mutex rtcp_mtx_;
  std::condition_variable rtcp_cv_;
  std::chrono::steady_clock::time_point last_send_rtcp_rr_ts_;
  std::atomic<bool> send_rtcp_rr_triggered_ = false;
  std::atomic<bool> rtcp_stop_ = false;
  int rtcp_rr_interval_ms_ = 5000;
  int rtcp_tcc_interval_ms_ = 200;

 private:
  std::shared_ptr<Clock> clock_;
  ReceiveSideCongestionController receive_side_congestion_controller_;
  RtcpFeedbackSenderInterface* active_remb_module_;
  uint32_t feedback_ssrc_ = 0;

  std::unique_ptr<NackRequester> nack_;

 private:
  class RtcpFeedbackBuffer : public KeyFrameRequestSender,
                             public NackSender,
                             public LossNotificationSender {
   public:
    RtcpFeedbackBuffer(KeyFrameRequestSender* key_frame_request_sender,
                       NackSender* nack_sender,
                       LossNotificationSender* loss_notification_sender);

    ~RtcpFeedbackBuffer() override = default;

    // KeyFrameRequestSender implementation.
    void RequestKeyFrame() override;

    // NackSender implementation.
    void SendNack(const std::vector<uint16_t>& sequence_numbers,
                  bool buffering_allowed) override;

    // LossNotificationSender implementation.
    void SendLossNotification(uint16_t last_decoded_seq_num,
                              uint16_t last_received_seq_num,
                              bool decodability_flag,
                              bool buffering_allowed) override;

    // Send all RTCP feedback messages buffered thus far.
    void SendBufferedRtcpFeedback();

    void ClearLossNotificationState();

   private:
    // LNTF-related state.
    struct LossNotificationState {
      LossNotificationState(uint16_t last_decoded_seq_num,
                            uint16_t last_received_seq_num,
                            bool decodability_flag)
          : last_decoded_seq_num(last_decoded_seq_num),
            last_received_seq_num(last_received_seq_num),
            decodability_flag(decodability_flag) {}

      uint16_t last_decoded_seq_num;
      uint16_t last_received_seq_num;
      bool decodability_flag;
    };

    KeyFrameRequestSender* const key_frame_request_sender_;
    NackSender* const nack_sender_;
    LossNotificationSender* const loss_notification_sender_;

    // Key-frame-request-related state.
    bool request_key_frame_;

    // NACK-related state.
    std::vector<uint16_t> nack_sequence_numbers_;
    std::optional<LossNotificationState> lntf_state_;
  };
  RtcpFeedbackBuffer rtcp_feedback_buffer_;

 private:
  FILE* file_rtp_recv_ = nullptr;
};

#endif
