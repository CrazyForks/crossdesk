/*
 * @Author: DI JUNKUN
 * @Date: 2024-04-22
 * Copyright (c) 2024 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _OBU_H_
#define _OBU_H_

#include <stddef.h>
#include <stdint.h>

#include "aom/aom_codec.h"

class Obu {
 public:
  Obu();
  Obu(const Obu &obu);
  Obu(Obu &&obu);
  Obu &operator=(const Obu &obu);
  Obu &operator=(Obu &&obu);

  ~Obu();

  bool SetPayload(const uint8_t *payload, int size);

  uint8_t header_ = 0;
  uint8_t extension_header_ = 0;  // undefined if (header & kXbit) == 0
  uint8_t *payload_ = nullptr;
  int size_ = 0;  // size of the header and payload combined.
  int payload_size_ = 0;
};

#endif