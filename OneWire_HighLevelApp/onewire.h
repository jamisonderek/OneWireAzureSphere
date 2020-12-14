/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "applibs_versions.h"
#include <applibs/gpio.h>
#include <applibs/uart.h>

/// <summary>
/// Initialize the UART and GPIO ports and resets the ROM search.
/// </summary>
/// <param name="uart">The UART port to use for communication.</param>
/// <param name="gpio">The GPIO port to use for pullup.</param>
/// <returns>true if the initialization was succesful, otherwise false.</returns>
bool OneWireInit(UART_Id uart, GPIO_Id gpio);

/// <summary>
/// Close the UART and GPIO ports.
/// </summary>
void OneWireClose(void);

typedef enum {
    DevicePresent = 0,
    NoDevices = 1,
    NoData = 2,
    HardwareFailure = 3,
} OneWireResetResponse;

/// <summary>
/// Sends a Reset pulse.  After sending a reset you should send a ROM command
/// (e.g. Search, Read, Match, Skip, Alarm, Verify)
/// </summary>
/// <returns>DevicePresent if reset was successful and at least one device
/// replied, otherwise an error value from the OneWireResetResponse enum.</returns>
OneWireResetResponse OneWireReset(void);

/// <summary>
/// Sents a bit of data on the OneWire bus.  Optionally enables the pullup for
/// parasitic charging when complete.
/// </summary>
/// <param name="bit">The data to send (either a 0 or 1).</param>
/// <param name="enableStrongPullup">true to enable pullup, otherwise false.</param>
/// <returns>true if data was successfully sent on the bus, otherwise false.</returns>
bool OneWireWriteBit(int bit, bool enableStrongPullup);

/// <summary>
/// Reads a bit of data on the OneWire bus.
/// </summary>
/// <returns>-1 if there was an error, otherwsie the bit (0 or 1) value is returned.</returns>
int OneWireReadBit(void);

/// <summary>
/// Disables the pullup used for parasitic charging.  It is recommended to disable
/// the pullup when power is not required, in case the OneWire bus gets grounded.
/// </summary>
void OneWireDisableStrongPullup(void);

/// <summary>
/// Sends a byte of data on the OneWire bus.
/// </summary>
/// <param name="data">The data to send.</param>
/// <returns>true if data was successfully sent on the bus, otherwise false.</returns>
bool OneWireSendByte(uint8_t data);

/// <summary>
/// Sends a byte of data on the OneWire bus.  Enables the pullup for parasitic
/// charging when complete.
/// </summary>
/// <param name="data">The data to send.</param>
/// <returns>true if data was successfully sent on the bus, otherwise false.</returns>
bool OneWireSendByteWithPullup(uint8_t data);

/// <summary>
/// Reads a byte of data on the OneWire bus.
/// </summary>
/// <returns>-1 if there was an error, otherwise the byte value is returned.</returns>
int OneWireReceiveByte(void);

/// <summary>
/// Addresses the device with the current OneWireROM identifier.  The next command
/// will only be performed by the device with the matched ROM.
/// </summary>
/// <returns>true if the device was addressed, otherwise false.</returns>
bool OneWireMatchROM(void);

/// <summary>
/// Addresses all devices on the OneWire bus.
/// </summary>
/// <returns>true if the command was successfully sent, otherwise false.</returns>
bool OneWireSkipROM(void);

/// <summary>
/// Set the OneWireROM to the ROM identifer of the device on the OneWire bus.  This command can
/// only be used when there is a single device on the bus.
/// </summary>
/// <returns>true if the OneWire device was found, otherwise false.</returns>
bool OneWireSingleReadROM(void);