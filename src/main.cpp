#include <Arduino.h>
#include <radio.h>
#include <printf.h>
#include <avr/wdt.h>

#include "config.h"
#include "log.h"
#include "number.h"
#include "token.h"
#include "cmd.h"

uint8_t gChannel = 6;
uint8_t gPaLevel = 1;

void setup()
{
	// put your setup code here, to run once:
	Serial.begin(9600);
	printf_begin();

	resetRadioState();

	gChannel = 6;
	gPaLevel = 1;
	initCmd();

	// setupRadioScanner();
	setupRadioTransmitter(gChannel, gPaLevel);

	wdt_enable(WDTO_4S);

	Serial.println();
	output(F("reset"));
}

void (* resetFunc) (void) = 0;

// format: in,<cmd>,<arg1>,...,<argn>\n
int receiveCmd(char* buffer, uint16_t bufferSize, char** outCmd, uint16_t* outCmdSize, char** outArgs, uint16_t* outArgsSize)
{
	#if MANUAL_MODE
	int readSize = readLineWithHistory(buffer, bufferSize);
	#else
	int readSize = readLine(buffer, bufferSize);
	#endif

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
	uint32_t prasedUInt32;
	if (parseHexStrToUInt32(bufStr, remoteIdSize, &prasedUInt32) != 0) {
		error(F("Invalid remote id format"));
		return -1;
	}
	if (checkUInt32BitSize(prasedUInt32, 24) != 0) {
		error(F("Invalid remote id value"));
		return -1;
	}
	*remoteId = prasedUInt32;

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
	if (parseHexStrToUInt32(bufStr, commandIdSize, &prasedUInt32) != 0) {
		error(F("Invalid command id format"));
		return -1;
	}
	if (checkUInt32BitSize(prasedUInt32, 8) != 0) {
		error(F("Invalid command id value"));
		return -1;
	}
	*commandId = (byte)prasedUInt32;

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
	if (parseHexStrToUInt32(bufStr, commandArgSize, &prasedUInt32) != 0) {
		error(F("Invalid command arg format"));
		return -1;
	}
	if (checkUInt32BitSize(prasedUInt32, 8) != 0) {
		error(F("Invalid command arg value"));
		return -1;
	}
	*commandArg = (byte)prasedUInt32;
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
	if (parseDecimalStrToUInt8(bufStr, channelSize, channel) != 0) {
		error(F("Invalid channel value"));
		return -1;
	}

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
	if (parseDecimalStrToUInt8(bufStr, paLevelSize, pa_level) != 0) {
		error(F("Invalid pa level value"));
		return -1;
	}
	
	return 0;
}

int parseScanCmdArgs(char* args, const uint16_t argsSize, uint32_t* timeout)
{
	char* argPtr = args;
	int32_t remainSize = argsSize;
	int timeoutSize = parseUntil(argPtr, remainSize, bufStr, sizeof(bufStr), ",");
	if (timeoutSize <= 0) {
		error(F("Invalid timeout size"));
		return -1;
	}
	if (parseDecimalStrToUInt32(bufStr, timeoutSize, timeout) != 0) {
		error(F("Invalid timeout value"));
		return -1;
	}
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

	// If millis() comes to near 0xFFFFFFFF, it will cause a bug
	// We reset the board every 30 days to prevent this bug
	if (millis() > 0x9A7EC800) {
		resetFunc();
	}

	char* cmd;
	uint16_t cmdSize;
	char* args;
	uint16_t argsSize;
	if (receiveCmd(cmdBuf, sizeof(cmdBuf), &cmd, &cmdSize, &args, &argsSize) < 0) {
		output(F("failed"));
		return;
	}

	if (cmdCmp(cmd, cmdSize, "start")) {
		int ret = setupRadioTransmitter(gChannel, gPaLevel);
		if (ret != 0) {
			error(F("Failed to setup transmitter"));
			output(F("failed"));
			return;
		}
		output(F("started"));
	} else if (cmdCmp(cmd, cmdSize, "set")) {
		uint8_t channel;
		uint8_t pa_level;
		int ret = parseSetCmdArgs(args, argsSize, &channel, &pa_level);
		if (ret != 0) {
			error(F("Invalid set command"));
			output(F("failed"));
			return;
		}
		gChannel = channel;
		gPaLevel = pa_level;
		sprintf(responseBuf, "channel=%d,pa_level=%d", gChannel, gPaLevel);
		output(responseBuf);
		ret = setupRadioTransmitter(gChannel, gPaLevel);
		if (ret != 0) {
			error(F("Failed to setup transmitter"));
			output(F("failed"));
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
			output(F("failed"));
			return;
		}
		ret = sendCustomCommand(remoteId, commandId, commandArg);
		if (ret != 0) {
			error(F("Send failed"));
			output(F("failed"));
			return;
		}
		// sendCustomCommand(0xBC4F10, 0x01, 0x01);
		output(F("sent"));
	} else if (cmdCmp(cmd, cmdSize, "print")) {
		printRadioDetails();
	} else if (cmdCmp(cmd, cmdSize, "scan")) {
		uint32_t timeout = 5000;
		if (argsSize > 0) {
			int ret = parseScanCmdArgs(args, argsSize, &timeout);
			if (ret != 0) {
				error(F("Invalid scan command"));
				output(F("failed"));
				return;
			}
		}
		int ret = setupRadioScanner(gChannel, gPaLevel);
		if (ret != 0) {
			error(F("Failed to setup scanner"));
			output(F("failed"));
			return;
		}
		unsigned long startMil = millis();
		while (true) {
			updateRadioScanner();
			wdt_reset();
			
			if (millis() - startMil > timeout) {
				break;
			}
		}
		ret = setupRadioTransmitter(gChannel, gPaLevel);
		if (ret != 0) {
			error(F("Failed to setup transmitter"));
			output(F("failed"));
			return;
		}
		output(F("scan_done"));
	} else if (cmdCmp(cmd, cmdSize, "ping")) {
		output(F("pong"));
	} else if (cmdCmp(cmd, cmdSize, "reset")) {
		resetFunc();
		// wdt_enable(WDTO_15MS);
		// while (true) {}
	} else {
		error(F("Unknown command"));
		errorSizedString(cmd, cmdSize);
		output(F("failed"));
	}
}
