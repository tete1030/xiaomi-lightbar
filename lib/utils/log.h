#pragma once

#define DEBUG_LOG 1
#include <HardwareSerial.h>

#if DEBUG_LOG
#define debugLog(...) Serial.print(__VA_ARGS__)
#define debugLogln(...) Serial.println(__VA_ARGS__)
#else
#define debugLog(...) do {} while (0)
#define debugLogln(...) do {} while (0)
#endif

#define output(...) \
    do { \
        Serial.print("out,"); \
        Serial.print(__VA_ARGS__); \
        Serial.println(""); \
    } while(0)

#define outputSizedString(str, strSize) \
    do { \
        Serial.print("out,"); \
        Serial.write(str, strSize); \
        Serial.println(""); \
    } while(0)

#define error(...) \
    do { \
        Serial.print("err,"); \
        Serial.print(__VA_ARGS__); \
        Serial.print(" ("); \
        Serial.print(__FILE__); \
        Serial.print(":"); \
        Serial.print(__LINE__); \
        Serial.print(")"); \
        Serial.println(""); \
    } while(0)

#define errorSizedString(str, strSize) \
    do { \
        Serial.print("err,"); \
        Serial.write(str, strSize); \
        Serial.print(" ("); \
        Serial.print(__FILE__); \
        Serial.print(":"); \
        Serial.print(__LINE__); \
        Serial.print(")"); \
        Serial.println(""); \
    } while(0)
