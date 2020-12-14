/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/// <summary>
/// Resets the OneWireSearchROM data to search for all devices on the OneWire bus.
/// </summary>
void OneWireResetSearch(void);

/// <summary>
/// Resets the OneWireSearchROM data to search for all devices on the OneWire bus
/// matching the specified family identifier.
/// </summary>
/// <param name="familyId">The family identifier (first 8 bits of the ROM ID).</param>
void OneWireTargetSetup(uint8_t familyId);

/// <summary>
/// Verifies the OneWireROM identifer is responding on the OneWire bus.
/// </summary>
/// <returns>true if the device responsed, false otherwise.</returns>
bool OneWireVerifyROM(void);

/// <summary>
/// Modified from https://www.maximintegrated.com/en/design/technical-documents/app-notes/1/187.html
/// --
/// Searches for a OneWire device and set the OneWireROM to the matching device ROM identifer.
/// </summary>
/// <param name="alarmSearch">
/// When set to true, then search will be for devices in the alarm state only.  False will return
/// all devices.
/// </param>
/// <returns>
/// true if a matching OneWire device was found, false if no device found.
/// </returns>
bool OneWireSearchROM(bool alarmSearch);

