/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#pragma once

#include <stdint.h>

/// <summary>
/// Returns the byte from the OneWire ROM at the specified index.
/// </summary>
/// <param name="index">The index to read from (0 to 7).</param>
/// <returns></returns>
uint8_t OneWireROMGetByte(int index);

/// <summary>
/// Sets the byte from the OneWire ROM at the specified index.
/// </summary>
/// <param name="index">The index to write to (0 to 7).</param>
/// <param name="data">The byte of data to write.</param>
void OneWireROMSetByte(int index, uint8_t data);

/// <summary>
/// Displays the ROM information in the Log_Debug output.
/// </summary>
void OneWireDebugDumpROM(void);