#include <Arduino.h>

#include "log.h"
#include "token.h"

int parseUntil(const char* source, int32_t sourceLen, char* target, uint16_t targetBufSize, const char* delimiters)
{
	if (sourceLen < 0) {
		error(F("Invalid source length"));
		return -1;
	}
	uint16_t index = 0;
	char sourceChar;
	uint16_t delLen = strlen(delimiters);
	while (index < sourceLen)
	{
		sourceChar = source[index];
		if (sourceChar == '\0') break;

		bool foundDelimiter = false;
		for (uint32_t i = 0; i < delLen; i++)
		{
			if (sourceChar == delimiters[i])
			{
				foundDelimiter = true;
				break;
			}
		}
		if (foundDelimiter) break;

		if (target != NULL && index >= targetBufSize - 1) {
			error(F("Buffer overflow"));
			return -1;
		}

		if (target != NULL) {
			target[index] = sourceChar;
		}

		index++;
	}
	if (target != NULL) {
		if (index < targetBufSize) {
			target[index] = '\0';
		} else {
			error(F("Buffer overflow"));
			return -1;
		}
	}
	return index;
}
