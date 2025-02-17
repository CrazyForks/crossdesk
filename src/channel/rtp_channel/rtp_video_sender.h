#ifndef _RTP_VIDEO_SENDER_H_
#define _RTP_VIDEO_SENDER_H_

#include <functional>

#include "io_statistics.h"
#include "ringbuffer.h"
#include "rtcp_sender_report.h"
#include "rtp_packet.h"
#include "rtp_packet_history.h"
#include "rtp_packet_to_send.h"
#include "rtp_statistics.h"
#include "thread_base.h"

class RtpVideoSender : public ThreadBase {
 public:
  RtpVideoSender();
  RtpVideoSender(std::shared_ptr<webrtc::Clock> clock,
                 std::shared_ptr<IOStatistics> io_statistics);
  virtual ~RtpVideoSender();

 public:
  void Enqueue(std::vector<std::shared_ptr<RtpPacket>> &rtp_packets);
  void SetSendDataFunc(std::function<int(const char *, size_t)> data_send_func);
  void SetOnSentPacketFunc(
      std::function<void(const webrtc::RtpPacketToSend &)> on_sent_packet_func);
  uint32_t GetSsrc() { return ssrc_; }

 private:
  int SendRtpPacket(std::shared_ptr<RtpPacket> rtp_packet);
  int SendRtcpSR(RtcpSenderReport &rtcp_sr);

  bool CheckIsTimeSendSR();

 private:
  bool Process() override;

 private:
  std::function<int(const char *, size_t)> data_send_func_ = nullptr;
  std::function<void(const webrtc::RtpPacketToSend &)> on_sent_packet_func_ =
      nullptr;
  RingBuffer<std::shared_ptr<RtpPacket>> rtp_packe_queue_;

 private:
  uint32_t ssrc_ = 0;
  std::shared_ptr<webrtc::Clock> clock_ = nullptr;
  std::unique_ptr<RtpStatistics> rtp_statistics_ = nullptr;
  std::shared_ptr<IOStatistics> io_statistics_ = nullptr;
  std::unique_ptr<RtpPacketHistory> rtp_packet_history_ = nullptr;
  uint32_t last_send_bytes_ = 0;
  uint32_t last_send_rtcp_sr_packet_ts_ = 0;
  uint32_t total_rtp_payload_sent_ = 0;
  uint32_t total_rtp_packets_sent_ = 0;

 private:
  int64_t transport_seq_ = 0;

 private:
  FILE *file_rtp_sent_ = nullptr;
};

#endif