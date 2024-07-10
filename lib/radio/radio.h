#ifndef RADIO_H
#define RADIO_H

void resetRadioState();
int printRadioDetails();

int setupRadioScanner(uint8_t channel, uint8_t pa_level);
int updateRadioScanner();

int setupRadioTransmitter(uint8_t channel, uint8_t pa_level);
int sendCustomCommand(uint32_t remote_id, byte command_id, byte command_arg);

#endif