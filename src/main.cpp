#include <Arduino.h>
#include <radio.h>
#include <printf.h>
#include <avr/wdt.h>

uint8_t gChannel = 6;
uint8_t gPaLevel = 1;

#include "config.h"
#include "log.h"

void setup()
{
	// put your setup code here, to run once:
	Serial.begin(9600);
	printf_begin();

	resetRadioState();

	gChannel = 6;
	gPaLevel = 1;

	// setupRadioScanner();
	// setupRadioTransmitter(gChannel, gPaLevel);

	wdt_enable(WDTO_8S);

	Serial.println();
	output(F("setup"));
}

void (* resetFunc) (void) = 0;

int readLine(char* buffer, uint32_t bufferSize)
{
	if (bufferSize == 0) {
		error(F("Buffer size is 0"));
		return -1;
	}
	uint32_t readCounter = 0;
	char readByte;
	
	// start time
	uint64_t startTime = 0;
	while (true)
	{
		if (readCounter + 1 >= bufferSize) {
			error(F("Buffer overflow"));
			return -1;
		}
		while (Serial.available() == 0) {
			wdt_reset();
			#if MANUAL_MODE == 0
			if (readCounter >= 1 && millis() - startTime > INPUT_TIMEOUT_MS) {
				error(F("Line Timeout"));
				return -1;
			}
			#endif
		}
		readByte = Serial.read();
		if (startTime == 0) {
			startTime = millis();
		}

		#if MANUAL_MODE == 1
		Serial.print(readByte);
		// if backspace
		if (readByte == 0x08) {
			Serial.print(F(" "));
			Serial.print(readByte);
		}
		#endif

		if (readByte == '\r') continue;
		if (readByte == '\n') break;

		if (readByte == 0x08) {
			if (readCounter > 0) {
				readCounter--;
			}
			continue;
		}
		buffer[readCounter++] = readByte;
	}
	buffer[readCounter] = '\0';
	return readCounter;
}

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

// format: in,<cmd>,<arg1>,...,<argn>\n
int receiveCmd(char* buffer, uint16_t bufferSize, char** outCmd, uint16_t* outCmdSize, char** outArgs, uint16_t* outArgsSize)
{
	int readSize = readLine(buffer, bufferSize);
	if (readSize <= 0) {
		return -1;
	}

	if (readSize < 3 || buffer[0] != 'i' || buffer[1] != 'n' || buffer[2] != ',')
	{
		error(F("Invalid command format"));
		return -1;
	}
	
	char* bufPtr = buffer + 3;
	int32_t remainSize = readSize - 3;
	if (remainSize <= 0) {
		error(F("No command"));
		return -1;
	}

	int cmdSize = parseUntil(bufPtr, remainSize, NULL, 0, ",\r\n");
	if (cmdSize <= 0) {
		error(F("No command"));
		return -1;
	}
	*outCmd = bufPtr;
	*outCmdSize = cmdSize;

	#if DEBUG_LOG
	debugLog(F("Received command: "));
	for (int i = 0; i < cmdSize; i++)
	{
		debugLog(*(*outCmd + i));
	}
	debugLogln();
	#endif

	bufPtr += cmdSize + 1;
	remainSize -= cmdSize + 1;
	if (remainSize <= 0) {
		*outArgs = NULL;
		*outArgsSize = 0;
		return 0;
	}
	int argsSize = parseUntil(bufPtr, remainSize, NULL, 0, "\r\n");
	if (argsSize < 0) {
		error(F("Arg parsing error"));
		return -1;
	}
	*outArgs = bufPtr;
	*outArgsSize = argsSize;
	return 0;
}

static char bufStr[9];

int parseSendCmdArgs(const char* args, const uint16_t argsSize, uint32_t* remoteId, byte* commandId, byte* commandArg)
{
	const char* argPtr = args;
	int32_t remainSize = argsSize;

	uint32_t remoteIdSize = parseUntil(argPtr, remainSize, bufStr, sizeof(bufStr), ",");
	if (remoteIdSize != 8) {
		error(F("Invalid remote id size"));
		return -1;
	}
	if (bufStr[0] != '0' || bufStr[1] != 'x') {
		error(F("Invalid remote id format"));
		return -1;
	}
	*remoteId = strtoul(bufStr+2, NULL, 16);
	argPtr += remoteIdSize + 1;
	remainSize -= remoteIdSize + 1;
	if (remainSize <= 0) {
		error(F("No command id"));
		return -1;
	}
	uint32_t commandIdSize = parseUntil(argPtr, remainSize, bufStr, sizeof(bufStr), ",");
	if (commandIdSize != 4) {
		error(F("Invalid command id size"));
		return -1;
	}
	if (bufStr[0] != '0' || bufStr[1] != 'x') {
		error(F("Invalid command id format"));
		return -1;
	}
	*commandId = (byte)strtoul(bufStr+2, NULL, 16);
	argPtr += commandIdSize + 1;
	remainSize -= commandIdSize + 1;
	if (remainSize <= 0) {
		error(F("No command arg"));
		return -1;
	}
	uint32_t commandArgSize = parseUntil(argPtr, remainSize, bufStr, sizeof(bufStr), ",");
	if (commandArgSize != 4) {
		error(F("Invalid command arg size"));
		return -1;
	}
	if (bufStr[0] != '0' || bufStr[1] != 'x') {
		error(F("Invalid command arg format"));
		return -1;
	}
	*commandArg = (byte)strtoul(bufStr+2, NULL, 16);
	return 0;
}

int parseSetCmdArgs(char* args, const uint16_t argsSize, uint8_t* channel, uint8_t* pa_level)
{
	char* argPtr = args;
	int32_t remainSize = argsSize;
	int channelSize = parseUntil(argPtr, remainSize, bufStr, sizeof(bufStr), ",");
	if (channelSize <= 0) {
		error(F("Invalid channel size"));
		return -1;
	}
	*channel = (uint8_t)strtoul(bufStr, NULL, 10);
	argPtr += channelSize + 1;
	remainSize -= channelSize + 1;
	if (remainSize <= 0) {
		error(F("No pa level"));
		return -1;
	}
	int paLevelSize = parseUntil(argPtr, remainSize, bufStr, sizeof(bufStr), ",");
	if (paLevelSize <= 0) {
		error(F("Invalid pa level size"));
		return -1;
	}
	*pa_level = (uint8_t)strtoul(bufStr, NULL, 10);
	return 0;
}

static char cmdBuf[128];

bool cmdCmp(const char* cmd, uint16_t cmdSize, const char* target)
{
	for (uint16_t i = 0; i < cmdSize; i++)
	{
		if (target[i] == '\0') {
			return false;
		}
		if (cmd[i] != target[i]) {
			return false;
		}
	}
	if (target[cmdSize] != '\0') {
		return false;
	}

	return true;
}

char responseBuf[32];

void loop()
{
	// updateRadioScanner();
	wdt_reset();

	char* cmd;
	uint16_t cmdSize;
	char* args;
	uint16_t argsSize;
	if (receiveCmd(cmdBuf, sizeof(cmdBuf), &cmd, &cmdSize, &args, &argsSize) < 0) {
		return;
	}
	debugLogln(F("Start exec"));

	if (cmdCmp(cmd, cmdSize, "start")) {
		int ret = setupRadioTransmitter(gChannel, gPaLevel);
		if (ret != 0) {
			error(F("Failed to setup transmitter"));
			return;
		}
		output(F("started"));
	} else if (cmdCmp(cmd, cmdSize, "set")) {
		uint8_t channel;
		uint8_t pa_level;
		int ret = parseSetCmdArgs(args, argsSize, &channel, &pa_level);
		if (ret != 0) {
			error(F("Invalid set command"));
			return;
		}
		gChannel = channel;
		gPaLevel = pa_level;
		sprintf(responseBuf, "channel=%d,pa_level=%d", gChannel, gPaLevel);
		output(responseBuf);
		ret = setupRadioTransmitter(gChannel, gPaLevel);
		if (ret != 0) {
			error(F("Failed to setup transmitter"));
			return;
		}
	} else if (cmdCmp(cmd, cmdSize, "get")) {
		sprintf(responseBuf, "channel=%d,pa_level=%d", gChannel, gPaLevel);
		output(responseBuf);
	} else if (cmdCmp(cmd, cmdSize, "send")) {
		uint32_t remoteId;
		byte commandId;
		byte commandArg;
		int ret = parseSendCmdArgs(args, argsSize, &remoteId, &commandId, &commandArg);
		if (ret != 0) {
			error(F("Invalid send command"));
			return;
		}
		ret = sendCustomCommand(remoteId, commandId, commandArg);
		if (ret != 0) {
			error(F("Send failed"));
			return;
		}
		// sendCustomCommand(0xBC4F10, 0x01, 0x01);
		output(F("sent"));
	} else if (cmdCmp(cmd, cmdSize, "print")) {
		printRadioDetails();
	} else if (cmdCmp(cmd, cmdSize, "scan")) {
		int ret = setupRadioScanner(gChannel, gPaLevel);
		if (ret != 0) {
			error(F("Failed to setup scanner"));
			return;
		}
		unsigned long startMil = millis();
		while (true) {
			updateRadioScanner();
			
			if (millis() - startMil > 5000) {
				break;
			}
		}
		ret = setupRadioTransmitter(gChannel, gPaLevel);
		if (ret != 0) {
			error(F("Failed to setup transmitter"));
			return;
		}
		output(F("scan_done"));
	} else if (cmdCmp(cmd, cmdSize, "ping")) {
		output(F("pong"));
	} else if (cmdCmp(cmd, cmdSize, "reset")) {
		output(F("resetting"));
		resetFunc();
	} else {
		error(F("Unknown command"));
		errorSizedString(cmd, cmdSize);
	}
}
