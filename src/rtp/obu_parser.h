/*
 * @Author: DI JUNKUN
 * @Date: 2024-04-22
 * Copyright (c) 2024 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _OBU_PARSER_H_
#define _OBU_PARSER_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "obu.h"

std::vector<Obu> ParseObus(uint8_t* payload, int payload_size);

const char* ObuTypeToString(OBU_TYPE type);

bool ObuHasExtension(uint8_t obu_header);

bool ObuHasSize(uint8_t obu_header);

int ObuType(uint8_t obu_header);

#endif