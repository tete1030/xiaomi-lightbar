#pragma once

#include <stdint.h>

int parseUntil(const char* source, int32_t sourceLen, char* target, uint16_t targetBufSize, const char* delimiters);
