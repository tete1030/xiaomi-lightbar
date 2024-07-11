#pragma once

#include <avr/wdt.h>

int parseDecimalStrToUInt8(const char* str, uint16_t strSize, uint8_t* outValue);
int parseDecimalStrToUInt16(const char* str, uint16_t strSize, uint16_t* outValue);
int parseDecimalStrToUInt32(const char* str, uint16_t strSize, uint32_t* outValue);
int parseHexStrToUInt32(const char* str, uint16_t strSize, uint32_t* outValue);
int checkUInt32BitSize(uint32_t value, uint8_t bitSize);
