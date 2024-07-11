#include <Arduino.h>
#include <stdint.h>
#include <avr/wdt.h>

#include "log.h"

char previousLine[128];

void initCmd()
{
    previousLine[0] = '\0';
}

void backspace()
{
    Serial.print(F("\b \b"));
}

void backspace(uint16_t count)
{
    for (uint16_t i = 0; i < count; i++)
    {
        backspace();
    }
}

void clearline()
{
    Serial.print(F("\r"));
    for (uint16_t i = 0; i < 80; i++)
    {
        Serial.print(F(" "));
    }
    Serial.print(F("\r"));
}

int readLine(char *buffer, uint32_t bufferSize, char *initial = NULL)
{
    if (bufferSize == 0)
    {
        error(F("Buffer size is 0"));
        return -1;
    }
    uint32_t readCounter = 0;
    char readByte;

    if (initial != NULL)
    {
        strncpy(buffer, initial, bufferSize);
        readCounter = strlen(initial);
        Serial.print(initial);
    }

    // start time
    uint64_t startTime = 0;
    while (true)
    {
        if (readCounter + 1 >= bufferSize)
        {
            error(F("Buffer overflow"));
            return -1;
        }
        while (Serial.available() == 0)
        {
            wdt_reset();
#if MANUAL_MODE == 0
            if (readCounter >= 1 && millis() - startTime > INPUT_TIMEOUT_MS)
            {
                error(F("Line Timeout"));
                return -1;
            }
#endif
        }
        readByte = Serial.read();
        if (startTime == 0)
        {
            startTime = millis();
        }

#if MANUAL_MODE == 1
        // if backspace
        if (readByte == 0x08)
        {
            if (readCounter > 0)
                backspace();
        }
        else if (readByte == 0x1B)
        {
            while (Serial.available() == 0)
            {
            }
            char c = Serial.read();
            if (c == '[')
            {
                while (Serial.available() == 0)
                {
                }
                c = Serial.read(); // skip '['
                if (c == 'A')
                {
                    // up
                    return -2;
                }
                else if (c == 'B')
                {
                    // down
                    return -3;
                }
                else
                {
                    // Silently ignore
                    continue;
                }
            }
            error(F("Invalid escape sequence"));
            continue;
        }
        else
        {
            Serial.print(readByte);
        }
#endif

        if (readByte == '\r')
            continue;
        if (readByte == '\n')
            break;

        if (readByte == 0x08)
        {
            if (readCounter > 0)
            {
                readCounter--;
            }
            continue;
        }
        buffer[readCounter++] = readByte;
    }
    buffer[readCounter] = '\0';
    return readCounter;
}

int readLineWithHistory(char *buffer, uint32_t bufferSize)
{
    char *initial = NULL;
    while (true)
    {
        clearline();
        Serial.print(F("> "));
        int readSize = readLine(buffer, bufferSize, initial);
        if (readSize == -2)
        {
            if (previousLine[0] == '\0')
            {
                continue;
            }
            initial = previousLine;
            continue;
        }
        else if (readSize == -3)
        {
            initial = NULL;
            continue;
        }
        else if (readSize < 0)
        {
            return readSize;
        }
        strncpy(previousLine, buffer, sizeof(previousLine));
        return readSize;
    }
}
