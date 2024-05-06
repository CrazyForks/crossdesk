#include "obu.h"

#include <string>

#include "log.h"

Obu::Obu() {}

Obu::Obu(const Obu &obu) {
  if (obu.size_ > 0) {
    data_ = (uint8_t *)malloc(obu.size_);
    memcpy(data_, obu.data_, obu.size_);
    size_ = obu.size_;
  }

  if (obu.payload_size_ > 0) {
    payload_ = (uint8_t *)malloc(obu.payload_size_);
    memcpy(payload_, obu.payload_, obu.payload_size_);
    payload_size_ = obu.payload_size_;
  }

  header_ = obu.header_;
  extension_header_ = obu.extension_header_;
}

Obu::Obu(Obu &&obu)
    : data_(std::move(obu.data_)),
      size_(obu.size_),
      payload_((uint8_t *)std::move(obu.payload_)),
      payload_size_(obu.payload_size_),
      header_(obu.header_),
      extension_header_(obu.extension_header_) {
  obu.data_ = nullptr;
  obu.size_ = 0;
  obu.payload_ = nullptr;
  obu.payload_size_ = 0;
}

Obu &Obu::operator=(const Obu &obu) {
  if (&obu != this) {
    if (obu.size_ > 0) {
      data_ = (uint8_t *)realloc(data_, obu.size_);
      memcpy(data_, obu.data_, obu.size_);
      size_ = obu.size_;
    }

    if (obu.payload_size_ > 0) {
      payload_ = (uint8_t *)realloc(payload_, obu.payload_size_);
      memcpy(payload_, obu.payload_, obu.payload_size_);
      payload_size_ = obu.payload_size_;
    }

    header_ = obu.header_;
    extension_header_ = obu.extension_header_;
  }
  return *this;
}

Obu &Obu::operator=(Obu &&obu) {
  if (&obu != this) {
    data_ = std::move(obu.data_);
    obu.data_ = nullptr;
    size_ = obu.size_;
    obu.size_ = 0;

    payload_ = std::move(obu.payload_);
    obu.payload_ = nullptr;
    payload_size_ = obu.payload_size_;
    obu.payload_size_ = 0;

    header_ = obu.header_;
    obu.header_ = 0;
    extension_header_ = obu.extension_header_;
    obu.extension_header_ = 0;
  }
  return *this;
}

Obu::~Obu() {
  if (data_) {
    free(data_);
    data_ = nullptr;
  }
  size_ = 0;

  if (payload_) {
    free(payload_);
    payload_ = nullptr;
  }
  payload_size_ = 0;

  header_ = 0;
  extension_header_ = 0;
}

bool Obu::SetPayload(const uint8_t *payload, int size) {
  if (payload_) {
    free(payload_);
    payload_ = nullptr;
  }
  payload_ = (uint8_t *)malloc(size);
  memcpy(payload_, payload, size);
  payload_size_ = size;

  if (payload_)
    return true;
  else
    return false;
}