#include <RF24.h>
#include <SPI.h>
//#include <pinout.h>
#include "log.h"

// pinout.h isn't being resolved for some reason so just defining here.
#define PIN_RADIO_CE 9
#define PIN_RADIO_CSN 10

#define SEND_ATTEMPTS 10

// const uint64_t address = 0xAAAAAAAAAAAA;
// const uint64_t address = 0x533914DD1C;

// const uint64_t RECEIVE_ADDRESS = 0x533914DD1C;
#define RECEIVE_ADDRESS 0x55533914DD
#define SEND_ADDRESS 0x5555555555

const unsigned long SYNC_SIZE = 8;
const unsigned long PREAMBLE_SIZE = 8;
const byte PREAMBLE[PREAMBLE_SIZE] = {0x53, 0x39, 0x14, 0xDD, 0x1C, 0x49, 0x34, 0x12};
const unsigned long REMOTE_ID_SIZE = 3;
const byte SEP = 0xFF;
const unsigned long PAYLOAD_SIZE = PREAMBLE_SIZE + REMOTE_ID_SIZE + 1 + 1 + 2;
const unsigned long CRC_SIZE = 2;
const unsigned long PACKET_SIZE = SYNC_SIZE + PAYLOAD_SIZE + CRC_SIZE;


RF24 radio(PIN_RADIO_CE, PIN_RADIO_CSN);
bool radioInitialised = false;
bool radioScannerMode = false;
uint8_t sendingCounter = 0;

void resetRadioState()
{
	radio = RF24(PIN_RADIO_CE, PIN_RADIO_CSN);
	radioInitialised = false;
	radioScannerMode = false;
	sendingCounter = 0;
}

#define PANIC() \
	do { \
		error(F("Panic")); \
		while(1); \
	} while(0)


#define CHECK_RADIO() \
	do { \
		if (!radio.isValid()) { \
			error(F("Radio not valid")); \
			return -1; \
		} \
	} while(0)

#define CHECK_RADIO_INIT() \
	do { \
		if (!radioInitialised) { \
			error(F("Radio not initialised")); \
			return -1; \
		} \
		CHECK_RADIO(); \
		if (!radio.isChipConnected()) { \
			error(F("Radio not connected")); \
			return -1; \
		} \
	} while(0)

#define CHECK_RADIO_SCANNER() \
	do { \
		CHECK_RADIO_INIT(); \
		if (!radioScannerMode) { \
			error(F("Radio not in scanner mode")); \
			return -1; \
		} \
	} while(0)

#define CHECK_RADIO_TRANSMITTER() \
	do { \
		CHECK_RADIO_INIT(); \
		if (radioScannerMode) { \
			error(F("Radio not in transmitter mode")); \
			return -1; \
		} \
	} while(0)


uint16_t crc16(const uint8_t* data_p, uint8_t length){
    uint8_t x;
    uint16_t crc = 0xFFFE;

    while (length--){
        x = crc >> 8 ^ *data_p++;
        x ^= x>>4;
        crc = (crc << 8) ^ ((uint16_t)(x << 12)) ^ ((uint16_t)(x <<5)) ^ ((uint16_t)x);
    }
    return crc;
}

uint8_t getPALevel(uint8_t pa_level)
{
	switch (pa_level) {
		case 0:
			return RF24_PA_MIN;
		case 1:
			return RF24_PA_LOW;
		case 2:
			return RF24_PA_HIGH;
		case 3:
			return RF24_PA_MAX;
		default:
			// print with sprintf
			char error_msg[50];
			sprintf_P(error_msg, PSTR("Invalid PA level: %d"), pa_level);
			error(error_msg);
			PANIC();
			return RF24_PA_LOW;
	}
}

int printRadioDetails()
{
	CHECK_RADIO();
	Serial.println(F("begin_radio_detail"));
	radio.printPrettyDetails();
	Serial.println(F("end_radio_detail"));
	return 0;
}

int setupRadioScanner(uint8_t channel, uint8_t pa_level)
{
	CHECK_RADIO();
	if (!radioInitialised && !radio.begin())
	{
		error(F("Radio failed to initialise"));
		PANIC();
	}
	radio.openReadingPipe(0, RECEIVE_ADDRESS);

	radio.setChannel(channel);
	radio.setDataRate(RF24_2MBPS);
	uint8_t real_pa_level = getPALevel(pa_level);
	radio.setPALevel(real_pa_level);
	radio.disableCRC();
	radio.disableDynamicPayloads();
	radio.setPayloadSize(13);
	radio.setAutoAck(false);

	radio.startListening();

	#if DEBUG_LOG
	radio.printPrettyDetails();
	#endif
	radioInitialised = true;
	radioScannerMode = true;
	return 0;
}

int updateRadioScanner()
{
	CHECK_RADIO_SCANNER();
	if (radio.available())
	{
		byte data[13] = {0};

		radio.read(&data, sizeof(data));

		// Byte 13 contains the command:
		// 0x20 on/off
		// 0x40 color temperature + (more blue)
		// 0x7F color temperature - (more red)
		// 0x80 brightness +
		// 0xBF brightness -
		
		// 0x533914DD1C493412
		// if (data[0] == 0x53 && data[1] == 39) // Capture all events from the remote
		// if (data[0] == 0x67 && data[1] == 0x22) // Capture all events from the remote
		if (data[0] == 0x1C && data[1] == 0x49 &&
			data[2] == 0x34 && data[3] == 0x12) // Capture all events from the remote
		{
			debugLogln(F("========================="));
			debugLogln(F("Light Bar Packet received"));
			debugLogln(F("========================="));

			uint32_t remote_id = 0;
			uint8_t sequence = 0;
			uint8_t command_id = 0;
			uint8_t command_arg = 0;
			for (unsigned long i = 0; i < 3; i++)
			{
				remote_id |= ((uint32_t)data[4+i]) << (8 * (2 - i));
			}
			sequence = data[8];
			command_id = data[9];
			command_arg = data[10];

			#if DEBUG_LOG
			for (unsigned long i = 4; i < sizeof(data); i++)
			{
				if (i == 4)
					debugLogln(F("------ Remote ID"));
				if (i == 7)
					debugLogln(F("------ Sep"));
				if (i == 8)
					debugLogln(F("------ Sequence"));
				if (i == 9)
					debugLog(F("------ Command ID"));
				if (i == 11)
					debugLogln(F("------ CRC16"));
				if (i == 9) {
					switch (data[i]) {
						case 0x01:
							debugLog(F(" | on/off"));
							break;
						case 0x02:
							debugLog(F(" | temperature - "));
							break;
						case 0x03:
							debugLog(F(" | temperature + "));
							break;
						case 0x04:
							debugLog(F(" | brightness +"));
							break;
						case 0x05:
							debugLog(F(" | brightness -"));
							break;
						default:
							debugLog(F(" | Unknown command"));
							break;
					}
					debugLogln();
				}
				debugLog(i, DEC);
				debugLog(F(": 0x"));
				debugLog(data[i], HEX);
				debugLogln();
			}
			#endif

			byte crc_data[15] = {0};
			crc_data[0] = 0x53;
			crc_data[1] = 0x39;
			crc_data[2] = 0x14;
			crc_data[3] = 0xDD;
			for (unsigned long i = 0; i < sizeof(data) - 2; i++)
			{
				crc_data[4+i] = data[i];
			}
			uint16_t crc_result = crc16(crc_data, (uint8_t)sizeof(crc_data));
			if (crc_result == (((uint16_t)data[11]) << 8 | (uint16_t)data[12])) {
				debugLogln(F("CRC16 matches"));
				char outputResult[100];
				sprintf_P(outputResult, PSTR("remote_id=0x%02X%04X,sequence=0x%02X,command_id=0x%02X,command_arg=0x%02X"), (uint16_t)(remote_id >> 16), (uint16_t)(remote_id & 0xFFFF), sequence, command_id, command_arg);

				output(outputResult);
			} else {
				debugLogln(F("CRC16 does not match"));
			}

			debugLogln(F("========================="));
			debugLogln();
		} else {
			#if DEBUG_LOG
			// Dump
			debugLogln(F("Packet received, unknown:"));
			for (unsigned long i = 0; i < sizeof(data); i++)
			{
				debugLog(F("0x"));
				if (data[i] < 0x10)
					debugLog(F("0"));
				debugLog(data[i], HEX);
				debugLog(F(" "));
			}
			debugLogln();
			#endif
		}
	} else {
		// debugLogln(F("No packet received"));
	}
	return 0;
};



int setupRadioTransmitter(uint8_t channel, uint8_t pa_level)
{
	CHECK_RADIO();
	debugLogln(F("Setup radio transmitter"));
	if (!radioInitialised && !radio.begin())
	{
		error(F("Radio failed to initialise"));
		PANIC();
	}
	// radio.openReadingPipe(0, SEND_ADDRESS);
	if (radioScannerMode)
	{
		debugLogln(F("Stop listening..."));
		radio.stopListening();
	}

	debugLogln(F("Set to channel..."));
	radio.setChannel(channel);

	debugLogln(F("Set data rate to 2MBPS..."));
	radio.setDataRate(RF24_2MBPS);

	debugLogln(F("Disable CRC..."));
	radio.disableCRC();

	debugLogln(F("Disable dynamic payloads..."));
	radio.disableDynamicPayloads();

	debugLogln(F("Disable auto-ACK..."));
	radio.setAutoAck(false);

	debugLogln(F("Set PA Level..."));
	uint8_t real_pa_level = getPALevel(pa_level);
	radio.setPALevel(real_pa_level);

	debugLogln(F("Set payload size"));
	radio.setPayloadSize(PACKET_SIZE);

	debugLogln(F("Set retries"));
	radio.setRetries(0, 0);

	debugLogln(F("Open writing pipe"));
	radio.openWritingPipe(SEND_ADDRESS); // always uses pipe 0

	#if DEBUG_LOG
	radio.printPrettyDetails();
	#endif

	radioInitialised = true;
	radioScannerMode = false;
	return 0;
}

// 0xBC4F10
int sendCustomCommand(uint32_t remote_id, byte command_id, byte command_arg) {
	CHECK_RADIO_TRANSMITTER();
	byte sequence = sendingCounter ++;

	byte data[PACKET_SIZE] = {0};
	for (unsigned long i = 0; i < SYNC_SIZE; i++)
	{
		data[i] = 0x55;
	}
	for (unsigned long i = 0; i < PREAMBLE_SIZE; i++)
	{
		data[SYNC_SIZE+i] = PREAMBLE[i];
	}
	for (unsigned long i = 0; i < REMOTE_ID_SIZE; i++)
	{
		data[SYNC_SIZE+PREAMBLE_SIZE+i] = (remote_id >> (8 * (REMOTE_ID_SIZE - i - 1))) & 0xFF;
	}
	data[SYNC_SIZE+PREAMBLE_SIZE+3] = SEP;
	data[SYNC_SIZE+PREAMBLE_SIZE+4] = sequence;
	data[SYNC_SIZE+PREAMBLE_SIZE+5] = command_id;
	data[SYNC_SIZE+PREAMBLE_SIZE+6] = command_arg;
	uint16_t crc_result = crc16(data + SYNC_SIZE, (uint8_t)PAYLOAD_SIZE);
	data[SYNC_SIZE+PAYLOAD_SIZE] = (crc_result >> 8) & 0xFF;
	data[SYNC_SIZE+PAYLOAD_SIZE+1] = (crc_result) & 0xFF;
	
	for (int i = 0; i < SEND_ATTEMPTS; i++)
	{
		if (!radio.write(&data, sizeof(data), true)) {
			error(F("Write fail"));
		}
		delay(20);
	}
	return 0;
}
