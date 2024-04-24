#include "obu.h"

#include <string>

#include "log.h"

Obu::Obu() {}

Obu::Obu(const Obu &obu) {
  if (obu.payload_size_ > 0) {
    payload_ = (uint8_t *)malloc(obu.payload_size_);
    if (NULL == payload_) {
      LOG_ERROR("Malloc failed");
    } else {
      memcpy(payload_, obu.payload_, obu.payload_size_);
    }
    payload_size_ = obu.payload_size_;
    size_ = obu.size_;
    header_ = obu.header_;
    extension_header_ = obu.extension_header_;
  } else {
    size_ = obu.size_;
    header_ = obu.header_;
    extension_header_ = obu.extension_header_;
  }
}

Obu::Obu(Obu &&obu)
    : payload_((uint8_t *)std::move(obu.payload_)),
      payload_size_(obu.payload_size_),
      size_(obu.size_),
      header_(obu.header_),
      extension_header_(obu.extension_header_) {
  obu.payload_ = nullptr;
  obu.payload_size_ = 0;
}

Obu &Obu::operator=(const Obu &obu) {
  if (&obu != this) {
    if (obu.payload_size_ > 0) {
      payload_ = (uint8_t *)realloc(payload_, obu.payload_size_);
      memcpy(payload_, obu.payload_, obu.payload_size_);
      payload_size_ = obu.payload_size_;
      size_ = obu.size_;
      header_ = obu.header_;
      extension_header_ = obu.extension_header_;
    } else {
      header_ = obu.header_;
      extension_header_ = obu.extension_header_;
    }
  }
  return *this;
}

Obu &Obu::operator=(Obu &&obu) {
  if (&obu != this) {
    payload_ = std::move(obu.payload_);
    obu.payload_ = nullptr;
    payload_size_ = obu.payload_size_;
    obu.payload_size_ = 0;
    size_ = obu.size_;
    obu.size_ = 0;
    header_ = obu.header_;
    obu.header_ = 0;
    extension_header_ = obu.extension_header_;
    obu.extension_header_ = 0;
  }
  return *this;
}

Obu::~Obu() {
  if (payload_) {
    free(payload_);
    payload_ = nullptr;
  }
  payload_size_ = 0;
  size_ = 0;
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