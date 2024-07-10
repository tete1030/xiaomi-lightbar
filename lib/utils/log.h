#pragma once

#include <HardwareSerial.h>

#include "config.h"

#if DEBUG_LOG
#define debugLog(...) Serial.print(__VA_ARGS__)
#define debugLogln(...) Serial.println(__VA_ARGS__)
#else
#define debugLog(...) do {} while (0)
#define debugLogln(...) do {} while (0)
#endif

#define output(...) \
    do { \
        Serial.print(F("out,")); \
        Serial.print(__VA_ARGS__); \
        Serial.println(); \
    } while(0)

#define outputSizedString(str, strSize) \
    do { \
        Serial.print(F("out,")); \
        Serial.write(str, strSize); \
        Serial.println(); \
    } while(0)

#define error(...) \
    do { \
        Serial.print(F("err,")); \
        Serial.print(__VA_ARGS__); \
        Serial.print(F(" (")); \
        Serial.print(F(__FILE__)); \
        Serial.print(F(":")); \
        Serial.print(__LINE__); \
        Serial.print(F(")")); \
        Serial.println(); \
    } while(0)

#define errorSizedString(str, strSize) \
    do { \
        Serial.print(F("err,")); \
        Serial.write(str, strSize); \
        Serial.print(F(" (")); \
        Serial.print(F(__FILE__)); \
        Serial.print(F(":")); \
        Serial.print(__LINE__); \
        Serial.print(F(")")); \
        Serial.println(); \
    } while(0)
