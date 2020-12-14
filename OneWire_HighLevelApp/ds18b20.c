/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

//
// The values used throughout this code are based on the following data sheet 
// https://datasheets.maximintegrated.com/en/ds/DS18B20.pdf
//

#include "ds18b20.h"
#include "onewire.h"
#include "sleep.h"
#include "crc8.h"

#include "applibs_versions.h"
#include <applibs/log.h>

// The scratchpad of the DS18B20 device.  Figure 9 of DS18B20.pdf shows the
// memory map defined as the following:  
// 0 - Temp LSB (0x50 default power on)
// 1 - Temp HSB (0x05 default power on)
// 2 - Th register (high temp)
// 3 - Tl register (low temp)
// 4 - configuration (bits 5 and 6 for resolution)
// 5 - reserved (0xFF)
// 6 - reserved
// 7 - reserved (0x10)
// 8 - CRC8 value
static uint8_t Ds18b20ScratchPad[9];

/// <summary>
/// Determines if the device is using VCC or parasitic power from the OneWire bus.  You must be
/// sure to select a device prior to using this command.  If the device is using parasitic power
/// then you should enable the strong pullup during temperature conversions and copy scratchpad
/// operations.
/// </summary>
/// <returns>true if the device is using VCC power, otherwise returns false.</returns>
bool Ds18b20ReadPowerSupplyVCC(void)
{
    bool status = OneWireSendByte(0xB4);
    if (status) {
        int b = OneWireReceiveByte();
        status = (b == 0xFF);
    } else {
        Log_Debug("ERROR: Failed to send 0xB4 command.\n");
    }
    return status;
}

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
bool Ds18b20ConvertT(bool enableStrongPullUp, ThermometerResolution currentResolution)
{
    bool status = enableStrongPullUp ? OneWireSendByteWithPullup(0x44) : OneWireSendByte(0x44);    

    int delay;
    switch (currentResolution) {
    case ThermometerResolution9bits:
        delay = 94;
        break;
    case ThermometerResolution10bits:
        delay = 188;
        break;
    case ThermometerResolution11bits:
        delay = 375;
        break;
    case ThermometerResolution12bits:
    default:
        delay = 750;
        break;
    }

    if (enableStrongPullUp) {
        SleepMilli(delay);
        OneWireDisableStrongPullup();
    } else {
        // TODO: PERF: You can poll to see when you receive a 1, which indicates the conversion 
        // completed instead of waiting the full duration.
        SleepMilli(delay);
    }

    return status;
}

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
bool Ds18b20WriteScratchpad(int8_t tHigh, int8_t tLow, ThermometerResolution resolution) 
{
    bool status = OneWireSendByte(0x4E);
    if (status) {
        status &= OneWireSendByte((uint8_t)tHigh);
        status &= OneWireSendByte((uint8_t)tLow);
        status &= OneWireSendByte((uint8_t)(resolution << 5));    
    }

    return status;
}

/// <summary>
/// Copies data from the device scratchpad to the EEPROM, which will be read on power-up, and has
/// a 10 year+ data retenion.  You must be sure to select a device prior to using this command.
/// Enable the strong pullup if parasitic power is required.
/// </summary>
/// <param name="enableStrongPullUp">true to enable the parasitic power, otherwise false.</param>
/// <returns>true if no error was detected, otherwise false.</returns>
bool Ds18b20CopyScratchpad(bool enableStrongPullUp)
{
    bool status = enableStrongPullUp ? OneWireSendByteWithPullup(0x44) : OneWireSendByte(0x44);
    
    // The documentation says to wait 10 milliseconds after sending the command to ensure the 
    // EEPROM is written to.
    SleepMilli(10);
    return status;
}

/// <summary>
/// Reads data from the scratchpad of the selected device into the scratchpad variable.  You must
/// be sure to select a device prior to using this command.  
/// </summary>
/// <returns>true if no error was detected, otherwise false.</returns>
bool Ds18b20ReadScratchpad(void)
{
    bool status = OneWireSendByte(0xBE);
    if (status) {
        ClearCrc8();
        for (int i = 0; i < 9; i++) {
            int b = OneWireReceiveByte();
            if (b == -1) {
                Ds18b20ScratchPad[i] = 0xFF; 
                status = false;
            } else {
                Ds18b20ScratchPad[i] = (uint8_t)b;
                DoCrc8((uint8_t)b);
            }
        }

        if (GetCrc8() != 0) {
            Log_Debug("WARN: CRC mismatch reading scratchpad.\n");
            status = false;
        }
    }

    return status;
}

/// <summary>
/// Returns the tHigh (or user defined byte) from the last read scratchpad.  You
/// must call <see src="Ds18b20ReadScratchpad"/> to populate the scratchpad first.
/// </summary>
/// <returns>returns tHigh (or user defined byte)</returns>
uint8_t GetScratchpadtHigh(void)
{
    return Ds18b20ScratchPad[2];
}

/// <summary>
/// Returns the tLow (or user defined byte) from the last read scratchpad.  You
/// must call <see src="Ds18b20ReadScratchpad"/> to populate the scratchpad first.
/// </summary>
/// <returns>returns tLow (or user defined byte)</returns>
uint8_t GetScratchpadtLow(void)
{
    return Ds18b20ScratchPad[3];
}

/// <summary>
/// Returns the temperature resolution from the last read scratchpad.  You
/// must call <see src="Ds18b20ReadScratchpad"/> to populate the scratchpad first.
/// </summary>
/// <returns>returns temperature resolution</returns>
ThermometerResolution GetScratchpadResolution(void)
{
    return (Ds18b20ScratchPad[4]>>5) & 3;
}

/// <summary>
/// Returns the temperature in celsius from the last read scratchpad.  You
/// must call <see src="Ds18b20ReadScratchpad"/> to populate the scratchpad first.
/// </summary>
/// <returns>returns temperature in celsius</returns>
float GetScratchpadCelsius(void)
{
    int t = ((Ds18b20ScratchPad[1] << 8) | Ds18b20ScratchPad[0]);
    switch (GetScratchpadResolution()) {
        case ThermometerResolution9bits:
        t &= 0xFFF8; // clear bottom 3 bits.
            break;
        case ThermometerResolution10bits:
            t &= 0xFFFC; // clear bottom 2 bits.
            break;

        case ThermometerResolution11bits:
            t &= 0xFFFE; // clear bottom 1 bits.
            break;

        case ThermometerResolution12bits:
            // don't clear any bits.
            break;
    }

    return ((float)t / 16.0F);
}

/// <summary>
/// Returns the temperature in fahrenheit from the last read scratchpad.  You
/// must call <see src="Ds18b20ReadScratchpad"/> to populate the scratchpad first.
/// </summary>
/// <returns>returns temperature in fahrenheit</returns>
float GetScratchpadFahrenheit(void)
{
    return GetScratchpadCelsius() * 1.8F + 32.0F;
}