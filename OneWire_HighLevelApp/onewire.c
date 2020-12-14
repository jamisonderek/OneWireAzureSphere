/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */
 
#include "onewire.h"
#include "onewireuart.h"
#include "onewirerom.h"
#include "onewiresearch.h"
#include "crc8.h"
#include "sleep.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "applibs_versions.h"
#include <applibs/gpio.h>
#include <applibs/uart.h>
#include <applibs/log.h>

#include <hw/sample_appliance.h>

static bool OneWireSendByteOptionalPullup(uint8_t data, bool enableStrongPullup);

/// <summary>
/// Initialize the UART and GPIO ports and resets the ROM search.
/// </summary>
/// <param name="uart">The UART port to use for communication.</param>
/// <param name="gpio">The GPIO port to use for pullup.</param>
/// <returns>true if the initialization was succesful, otherwise false.</returns>
bool OneWireInit(UART_Id uart, GPIO_Id gpio) 
{
    OneWireResetSearch();
    return OneWireUartInit(uart, gpio);
}

/// <summary>
/// Close the UART and GPIO ports.
/// </summary>
void OneWireClose(void) 
{
    OneWireUartClose();
}

/// <summary>
/// Sends a Reset pulse.  After sending a reset you should send a ROM command 
/// (e.g. Search, Read, Match, Skip, Alarm, Verify)
/// </summary>
/// <returns>DevicePresent if reset was successful and at least one device
/// replied, otherwise an error value from the OneWireResetResponse enum.</returns>
OneWireResetResponse OneWireReset(void) 
{
    return (OneWireResetResponse)OneWireUartPulseReset();
}

/// <summary>
/// Sents a bit of data on the OneWire bus.  Optionally enables the pullup for
/// parasitic charging when complete.
/// </summary>
/// <param name="bit">The data to send (either a 0 or 1).</param>
/// <param name="enableStrongPullup">true to enable pullup, otherwise false.</param>
/// <returns>true if data was successfully sent on the bus, otherwise false.</returns>
bool OneWireWriteBit(int bit, bool enableStrongPullup)
{
    return OneWireUartPulseWriteBit(bit, enableStrongPullup);
}

/// <summary>
/// Reads a bit of data on the OneWire bus.
/// </summary>
/// <returns>-1 if there was an error, otherwsie the bit (0 or 1) value is returned.</returns>
int OneWireReadBit(void)
{
    return OneWireUartPulseReadBit();
}

/// <summary>
/// Disables the pullup used for parasitic charging.  It is recommended to disable
/// the pullup when power is not required, in case the OneWire bus gets grounded.
/// </summary>
void OneWireDisableStrongPullup(void)
{
    OneWireDisableStrongPullupGpio();
}

/// <summary>
/// Sends a byte worth of data on the OneWire bus.  Each bit is transmitted
/// using 1 byte of the UART (which determines the length of the pulse for a 0 or 1).
/// Using the UART ensures the timing of the bit is always the correct duration.
/// </summary>
/// <param name="data">The byte to send on the OneWire bus.</param>
/// <param name="enableStrongPullup">Enables the pullup GPIO after each bit.</param>
/// <returns>true if the byte was successfully transmitted on the OneWire bus, otherwise
/// false.</returns>
static bool OneWireSendByteOptionalPullup(uint8_t data, bool enableStrongPullup)
{
    bool status = true;
    for (int i = 0; i < 8; i++) {
        status &= OneWireWriteBit(data & 1, enableStrongPullup);
        data = data >> 1;
    }

    return status;
}

/// <summary>
/// Sends a byte of data on the OneWire bus.
/// </summary>
/// <param name="data">The data to send.</param>
/// <returns>true if data was successfully sent on the bus, otherwise false.</returns>
bool OneWireSendByte(uint8_t data)
{
    return OneWireSendByteOptionalPullup(data, false);
}

/// <summary>
/// Sends a byte of data on the OneWire bus.  Enables the pullup for parasitic 
/// charging when complete.
/// </summary>
/// <param name="data">The data to send.</param>
/// <returns>true if data was successfully sent on the bus, otherwise false.</returns>
bool OneWireSendByteWithPullup(uint8_t data)
{
    return OneWireSendByteOptionalPullup(data, true);
}

/// <summary>
/// Reads a byte of data on the OneWire bus.
/// </summary>
/// <returns>-1 if there was an error, otherwise the byte value is returned.</returns>
int OneWireReceiveByte(void)
{
    int data = 0;
    for (int i = 0; i < 8; i++) {
        int bitValue = OneWireReadBit();
        if (bitValue == -1) {
            // Set all bits of the data to on.
            data = -1;
        } else if (bitValue == 1) {
            data |= (1 << i);
        }
    }

    return data;
}

/// <summary>
/// Addresses the device with the current OneWireROM identifier.  The next command
/// will only be performed by the device with the matched ROM.
/// </summary>
/// <returns>true if the device was addressed, otherwise false.</returns>
bool OneWireMatchROM(void)
{
    OneWireReset();
    OneWireSendByte(0x55);
    for (int i = 0; i < 8; i++) {
        OneWireSendByte(OneWireROMGetByte(i));
    }

    return true;
}

/// <summary>
/// Addresses all devices on the OneWire bus.
/// </summary>
/// <returns>true if the command was successfully sent, otherwise false.</returns>
bool OneWireSkipROM(void) 
{
    OneWireReset();
    OneWireSendByte(0xCC);
    return true;
}

/// <summary>
/// Set the OneWireROM to the ROM identifer of the device on the OneWire bus.  This command can
/// only be used when there is a single device on the bus.
/// </summary>
/// <returns>true if the OneWire device was found, otherwise false.</returns>
bool OneWireSingleReadROM(void)
{
    OneWireReset();
    OneWireSendByte(0x33);

    uint8_t rom[8];
    ClearCrc8();
    for (int i = 0; i < 8; i++) {
        int b = OneWireReceiveByte();
        if (b == -1) {
            Log_Debug("ERROR: Failed reading OneWireROM[%d].\n", i);
            return false;
        }
        DoCrc8((uint8_t)b);
        rom[i] = (uint8_t)b;
    }

    if (GetCrc8() != 0) {
        Log_Debug(
            "ERROR: CRC did not match expected value. Ensure only one device is connected.\n");
        return false;
    }

    for (int i = 0; i < 8; i++) {
        OneWireROMSetByte(i, rom[i]);
    }

    return true;
}
