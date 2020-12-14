/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#include "onewirerom.h"

#include "applibs_versions.h"
#include <applibs/log.h>

/// <summary>
/// This is the unique 64 bit identifier for the OneWire device.
/// </summary>
static uint8_t OneWireROM[8];

/// <summary>
/// Returns the byte from the OneWire ROM at the specified index.
/// </summary>
/// <param name="index">The index to read from (0 to 7).</param>
/// <returns></returns>
uint8_t OneWireROMGetByte(int index) 
{
    return OneWireROM[index];
}

/// <summary>
/// Sets the byte from the OneWire ROM at the specified index.
/// </summary>
/// <param name="index">The index to write to (0 to 7).</param>
/// <param name="data">The byte of data to write.</param>
void OneWireROMSetByte(int index, uint8_t data) 
{
    OneWireROM[index] = data;
}

/// <summary>
/// Displays the current ROM information in the Log_Debug output.
/// </summary>
void OneWireDebugDumpROM(void)
{
    Log_Debug("ROM: ");
    for (int i = 0; i < 8; i++) {
        Log_Debug("%02x ", OneWireROM[i]);
    }
    Log_Debug("\n");
}