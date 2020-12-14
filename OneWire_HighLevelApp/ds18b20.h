/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    /// <summary>
    /// 9 bit, 0.5°C resolution, 93.75ms sample time
    /// </summary>
    ThermometerResolution9bits = 0,

    /// <summary>
    /// 10 bit, 0.25°C resolution, 187.5ms sample time 
    /// </summary>
    ThermometerResolution10bits = 1,
    
    /// <summary>
    /// 11 bit, 0.125°C resolution, 375ms sample time
    /// </summary>
    ThermometerResolution11bits = 2,

    /// <summary>
    /// 12 bit, 0.0625°C resolution, 750ms sample time
    /// </summary>
    ThermometerResolution12bits = 3,
} ThermometerResolution;

/// <summary>
/// Determines if the device is using VCC or parasitic power from the OneWire bus.  You must be
/// sure to select a device prior to using this command.  If the device is using parasitic power
/// (e.g. this function results false) then you should enable the strong pullup during temperature 
/// conversions and copy scratchpad operations, since they require a higher current than typically
/// provided on the bus.
/// </summary>
/// <returns>true if the device is using VCC power, otherwise returns false.</returns>
bool Ds18b20ReadPowerSupplyVCC(void);

/// <summary>
/// Performs a temperature conversion, storing the result in the devices internal
/// scratchpad & setting the alert state based on the Temp LSB and Th and Tl registers.
/// You must be sure to select a device (or all devices) prior to using this command.
/// Enable the strong pullup if parasitic power is required.
/// </summary>
/// <param name="enableStrongPullUp">true to enable the parasitic power, otherwise false.</param>
/// <param name="currentResolution">The resolution the device is configured for.  You can
/// use GetScratchpadResolution to determine the resolution, or you can use use 12 bit
/// to allow the maximum time for conversion.</param>
/// <returns>true if no error was detected, otherwise false.</returns>
bool Ds18b20ConvertT(bool enableStrongPullUp, ThermometerResolution currentResolution);

/// <summary>
/// Writes data to the scratchpad on the selected device. You must be sure to select a device
/// (or all devices) prior to using this command.  If you are not going to use the alert function
/// then tHigh and tLow can be used as two bytes of temporary user storage.  If you choose to
/// persist the scratchpad to EEPROM, use the CopyScratchpad method afterwards.
/// </summary>
/// <param name="tHigh">The high temperature in celsius (or a user defined byte)</param>
/// <param name="tLow">The low temperature in celsius (or a user defined byte)</param>
/// <param name="resolution">The resolution of temperature sampling.</param>
/// <returns>true if no error was detected, otherwise false.</returns>
bool Ds18b20WriteScratchpad(int8_t tHigh, int8_t tLow, ThermometerResolution resolution);

/// <summary>
/// Copies data from the device scratchpad to the EEPROM, which will be read on power-up, and has
/// a 10 year+ data retenion.  You must be sure to select a device prior to using this command.
/// Enable the strong pullup if parasitic power is required.
/// </summary>
/// <param name="enableStrongPullUp">true to enable the parasitic power, otherwise false.</param>
/// <returns>true if no error was detected, otherwise false.</returns>
bool Ds18b20CopyScratchpad(bool enableStrongPullUp);

/// <summary>
/// Reads data from the scratchpad of the selected device into the scratchpad variable.  You must
/// be sure to select a device prior to using this command.
/// </summary>
/// <returns>true if no error was detected, otherwise false.</returns>
bool Ds18b20ReadScratchpad(void);

/// <summary>
/// Returns the tHigh (or user defined byte) from the last read scratchpad.  You
/// must call <see src="Ds18b20ReadScratchpad"/> to populate the scratchpad first.
/// </summary>
/// <returns></returns>
uint8_t GetScratchpadtHigh(void);

/// <summary>
/// Returns the tLow (or user defined byte) from the last read scratchpad.  You
/// must call <see src="Ds18b20ReadScratchpad"/> to populate the scratchpad first.
/// </summary>
/// <returns>returns tLow (or user defined byte)</returns>
uint8_t GetScratchpadtLow(void);

/// <summary>
/// Returns the temperature resolution from the last read scratchpad.  You
/// must call <see src="Ds18b20ReadScratchpad"/> to populate the scratchpad first.
/// </summary>
/// <returns>returns temperature resolution</returns>
ThermometerResolution GetScratchpadResolution(void);

/// <summary>
/// Returns the temperature in celsius from the last read scratchpad.  You
/// must call <see src="Ds18b20ReadScratchpad"/> to populate the scratchpad first.
/// </summary>
/// <returns>returns temperature in celsius</returns>
float GetScratchpadCelsius(void);

/// <summary>
/// Returns the temperature in fahrenheit from the last read scratchpad.  You
/// must call <see src="Ds18b20ReadScratchpad"/> to populate the scratchpad first.
/// </summary>
/// <returns>returns temperature in fahrenheit</returns>
float GetScratchpadFahrenheit(void);
