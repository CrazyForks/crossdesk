#include "openh264_decoder.h"

#include <cstring>

#include "libyuv.h"
#include "log.h"

#define SAVE_NV12_STREAM 0
#define SAVE_H264_STREAM 0

static const int YUV420P_BUFFER_SIZE = 1280 * 720 * 3 / 2;

void CopyYUVWithStride(uint8_t *srcY, uint8_t *srcU, uint8_t *srcV, int width,
                       int height, int strideY, int strideU, int strideV,
                       uint8_t *yuv_data_) {
  int actualWidth = width;
  int actualHeight = height;

  int actualStrideY = actualWidth;
  int actualStrideU = actualWidth / 2;
  int actualStrideV = actualWidth / 2;

  for (int row = 0; row < actualHeight; row++) {
    memcpy(yuv_data_, srcY, actualStrideY);
    srcY += strideY;
    yuv_data_ += actualStrideY;
  }

  for (int row = 0; row < actualHeight / 2; row++) {
    memcpy(yuv_data_, srcU, actualStrideU);
    srcU += strideU;
    yuv_data_ += actualStrideU;
  }

  for (int row = 0; row < actualHeight / 2; row++) {
    memcpy(yuv_data_, srcV, actualStrideV);
    srcV += strideV;
    yuv_data_ += actualStrideV;
  }
}

void ConvertYUV420toNV12(const unsigned char *yuvData, unsigned char *nv12Data,
                         int width, int height) {
  int ySize = width * height;
  int uvSize = ySize / 4;
  const unsigned char *yData = yuvData;
  const unsigned char *uData = yData + ySize;
  const unsigned char *vData = uData + uvSize;

  std::memcpy(nv12Data, yData, ySize);

  for (int i = 0; i < uvSize; i++) {
    nv12Data[ySize + i * 2] = uData[i];
    nv12Data[ySize + i * 2 + 1] = vData[i];
  }
}

OpenH264Decoder::OpenH264Decoder() {}
OpenH264Decoder::~OpenH264Decoder() {
  if (openh264_decoder_) {
    openh264_decoder_->Uninitialize();
    WelsDestroyDecoder(openh264_decoder_);
  }

  if (nv12_frame_) {
    delete nv12_frame_;
  }

  if (SAVE_H264_STREAM && h264_stream_) {
    fflush(h264_stream_);
    h264_stream_ = nullptr;
  }

  if (SAVE_NV12_STREAM && nv12_stream_) {
    fflush(nv12_stream_);
    nv12_stream_ = nullptr;
  }
}

int OpenH264Decoder::Init() {
  if (SAVE_NV12_STREAM) {
    nv12_stream_ = fopen("nv12_receive_.yuv", "w+b");
    if (!nv12_stream_) {
      LOG_WARN("Fail to open nv12_receive_.yuv");
    }
  }

  if (SAVE_NV12_STREAM) {
    h264_stream_ = fopen("h264_receive.h264", "w+b");
    if (!h264_stream_) {
      LOG_WARN("Fail to open h264_receive.h264");
    }
  }

  frame_width_ = 1280;
  frame_height_ = 720;

  decoded_frame_size_ = YUV420P_BUFFER_SIZE;
  decoded_frame_ = new uint8_t[YUV420P_BUFFER_SIZE];
  nv12_frame_ = new uint8_t[YUV420P_BUFFER_SIZE];

  if (WelsCreateDecoder(&openh264_decoder_) != 0) {
    LOG_ERROR("Failed to create OpenH264 decoder");
    return -1;
  }

  SDecodingParam sDecParam;

  memset(&sDecParam, 0, sizeof(SDecodingParam));
  sDecParam.uiTargetDqLayer = UCHAR_MAX;
  sDecParam.eEcActiveIdc = ERROR_CON_SLICE_COPY;
  sDecParam.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_DEFAULT;

  openh264_decoder_->Initialize(&sDecParam);

  int trace_level = WELS_LOG_QUIET;
  openh264_decoder_->SetOption(DECODER_OPTION_TRACE_LEVEL, &trace_level);

  return 0;
}

int OpenH264Decoder::Decode(
    const uint8_t *data, int size,
    std::function<void(VideoFrame)> on_receive_decoded_frame) {
  if (!openh264_decoder_) {
    return -1;
  }

  if (SAVE_H264_STREAM) {
    fwrite((unsigned char *)data, 1, size, h264_stream_);
  }

  if ((*(data + 4) & 0x1f) == 0x07) {
    // LOG_WARN("Receive key frame");
  }

  SBufferInfo sDstBufInfo;
  memset(&sDstBufInfo, 0, sizeof(SBufferInfo));

  openh264_decoder_->DecodeFrameNoDelay(data, size, yuv_data_, &sDstBufInfo);

  if (sDstBufInfo.iBufferStatus == 1) {
    if (on_receive_decoded_frame) {
      CopyYUVWithStride(yuv_data_[0], yuv_data_[1], yuv_data_[2],
                        sDstBufInfo.UsrData.sSystemBuffer.iWidth,
                        sDstBufInfo.UsrData.sSystemBuffer.iHeight,
                        sDstBufInfo.UsrData.sSystemBuffer.iStride[0],
                        sDstBufInfo.UsrData.sSystemBuffer.iStride[1],
                        sDstBufInfo.UsrData.sSystemBuffer.iStride[1],
                        decoded_frame_);

      if (SAVE_NV12_STREAM) {
        fwrite((unsigned char *)decoded_frame_, 1,
               frame_width_ * frame_height_ * 3 / 2, nv12_stream_);
      }

      if (0) {
        ConvertYUV420toNV12(decoded_frame_, nv12_frame_, frame_width_,
                            frame_height_);
      } else {
        libyuv::I420ToNV12(
            (const uint8_t *)decoded_frame_, frame_width_,
            (const uint8_t *)decoded_frame_ + frame_width_ * frame_height_,
            frame_width_ / 2,
            (const uint8_t *)decoded_frame_ +
                frame_width_ * frame_height_ * 5 / 4,
            frame_width_ / 2, nv12_frame_, frame_width_,
            nv12_frame_ + frame_width_ * frame_height_, frame_width_,
            frame_width_, frame_height_);
      }

      VideoFrame decoded_frame(nv12_frame_,
                               frame_width_ * frame_height_ * 3 / 2,
                               frame_width_, frame_height_);

      on_receive_decoded_frame(decoded_frame);
      if (SAVE_NV12_STREAM) {
        fwrite((unsigned char *)decoded_frame.Buffer(), 1, decoded_frame.Size(),
               nv12_stream_);
      }
    }
  }

  return 0;
}