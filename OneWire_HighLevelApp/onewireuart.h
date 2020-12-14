/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "applibs_versions.h"
#include <applibs/gpio.h>
#include <applibs/uart.h>

/// <summary>
/// Initialize the UART and GPIO ports.
/// </summary>
/// <param name="uart">The UART port to use for communication.</param>
/// <param name="gpio">The GPIO port to use for pullup.</param>
/// <returns></returns>
bool OneWireUartInit(UART_Id uart, GPIO_Id gpio);

/// <summary>
/// Close the UART and GPIO ports.
/// </summary>
void OneWireUartClose(void);

typedef enum {
    UartImplDevicePresent = 0,
    UartImplNoDevices = 1,
    UartImplNoData = 2,
    UartImplHardwareFailure = 3,
} OneWireUartResetResponse;

/// <summary>
/// Sends a reset pulse on the OneWire bus and checks to see if
/// any devices are present.
/// </summary>
/// <returns>returns UartImplDevicePresent if one or more devices responded,
/// UartImplNoDevices is no devices responsed, UartImplNoData if there was
/// an error during communication.
/// </returns>
OneWireUartResetResponse OneWireUartPulseReset(void);

/// <summary>
/// Sends a data bit on the OneWire bus.  After sending
/// the bit the strong pullup can be enabled to help with parasitic charging.
/// </summary>
/// <param name="bit">The bit to send (either a 0 or a 1).</param>
/// <param name="enableStrongPullup">true if the pullup should be enabled after
/// sending the bit.</param>
/// <returns>true if successful, otherwise false.</returns>
bool OneWireUartPulseWriteBit(int bit, bool enableStrongPullup);

/// <summary>
/// Reads a bit from the OneWire bus.  The Azure Sphere sends a pulse and then reads the
/// line to see if the other device set the bit to 0 or 1.
/// </summary>
/// <returns>returns -1 if error, otherwise returns the bit (0 or 1) value.</returns>
int OneWireUartPulseReadBit(void);

/// <summary>
/// Disables the pullup GPIO on the OneWire bus.  This should be disabled if the OneWire
/// bus might get pulled to ground.  The pullup should be connected via a 680 ohm
/// resistor, so max current provided is 5v/680ohm = 7.4mA of current.
/// </summary>
void OneWireDisableStrongPullupGpio(void);
