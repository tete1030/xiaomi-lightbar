#include <stdint.h>
#include <errno.h>

#include "number.h"
#include "log.h"

int parseDecimalStrToUInt8(const char* str, uint16_t strSize, uint8_t* outValue)
{
	char* endPtr;
	unsigned long parseResult = (uint8_t)strtoul(str, &endPtr, 10);
	if (endPtr != str + strSize) {
		error(F("Invalid format"));
		return -1;
	}
	if (parseResult > UINT8_MAX || errno == ERANGE) {
		error(F("Invalid value"));
		return -1;
	}
	*outValue = (uint8_t)parseResult;
	return 0;
}

int parseDecimalStrToUInt16(const char* str, uint16_t strSize, uint16_t* outValue)
{
	char* endPtr;
	unsigned long parseResult = (uint16_t)strtoul(str, &endPtr, 10);
	if (endPtr != str + strSize) {
		error(F("Invalid format"));
		return -1;
	}
	if (parseResult > UINT16_MAX || errno == ERANGE) {
		error(F("Invalid value"));
		return -1;
	}
	*outValue = (uint16_t)parseResult;
	return 0;
}

int parseDecimalStrToUInt32(const char* str, uint16_t strSize, uint32_t* outValue)
{
	char* endPtr;
	unsigned long parseResult = strtoul(str, &endPtr, 10);
	if (endPtr != str + strSize) {
		error(F("Invalid format"));
		return -1;
	}
	if (errno == ERANGE) {
		error(F("Invalid value"));
		return -1;
	}
	*outValue = (uint32_t)parseResult;
	return 0;
}

int parseHexStrToUInt32(const char* str, uint16_t strSize, uint32_t* outValue)
{
	if (strSize <= 2) {
		error(F("Invalid size"));
		return -1;
	}
	if (str[0] != '0' || str[1] != 'x') {
		error(F("Invalid format"));
		return -1;
	}

	char* endPtr;
	unsigned long parseResult = strtoul(str+2, &endPtr, 16);
	if (endPtr != str + strSize) {
		error(F("Invalid format"));
		return -1;
	}
	if (errno == ERANGE) {
		error(F("Invalid value"));
		return -1;
	}
	*outValue = (uint32_t)parseResult;
	return 0;
}

int checkUInt32BitSize(uint32_t value, uint8_t bitSize)
{
	if (bitSize >= 32) {
		return 0;
	}
	if (value > (((uint32_t)1) << bitSize) - 1) {
		error(F("Value too large"));
		return -1;
	}
	return 0;
}
