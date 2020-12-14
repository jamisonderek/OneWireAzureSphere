/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

/*
The OneWireSearchROM method was based on the example from
https://www.maximintegrated.com/en/design/technical-documents/app-notes/1/187.html

Copyright (C) 2002 Dallas Semiconductor Corporation, All Rights Reserved.
   Licensed under the MIT License. 
   See accompanied LICENSE.txt for details.

Except as contained in this notice, the name of Dallas Semiconductor shall not be 
used except as stated in the Dallas Semiconductor Branding Policy.
*/

#include "crc8.h"
#include "onewire.h"
#include "onewirerom.h"
#include "onewiresearch.h"

#include <stdbool.h>
#include <stdint.h>

// Flags used by the OneWireSearchROM routine.
static uint8_t OneWireLastDeviceFlag = 0;
static uint8_t OneWireLastDiscrepancy = 0;
static uint8_t OneWireLastFamilyDiscrepancy = 0;

/// <summary>
/// Resets the OneWireSearchROM data to search for all devices on the OneWire bus.
/// </summary>
void OneWireResetSearch(void)
{
    OneWireLastDeviceFlag = 0;
    OneWireLastDiscrepancy = 0;
    OneWireLastFamilyDiscrepancy = 0;
    for (int i = 0; i < 8; i++) {
        OneWireROMSetByte(i, 0);
    }
}

/// <summary>
/// Resets the OneWireSearchROM data to search for all devices on the OneWire bus
/// matching the specified family identifier.
/// </summary>
/// <param name="familyId">The family identifier (first 8 bits of the ROM ID).</param>
void OneWireTargetSetup(uint8_t familyId)
{
    OneWireResetSearch();
    OneWireROMSetByte(0, familyId);
    // Set this to 0x40 per "Target Setup" section of Table 4 in application note 187.
    OneWireLastDiscrepancy = 0x40;
}

/// <summary>
/// Verifies the OneWireROM identifer is responding on the OneWire bus.
/// </summary>
/// <returns>true if the device responsed, false otherwise.</returns>
bool OneWireVerifyROM(void)
{
    // Save the previous state, so we can restore after verification.
    uint8_t lastDeviceFlag = OneWireLastDeviceFlag;
    uint8_t lastDiscrepancy = OneWireLastDiscrepancy;
    uint8_t lastFamilyDiscrepancy = OneWireLastFamilyDiscrepancy;
    uint8_t rom[8];
    for (int i = 0; i < 8; i++) {
        rom[i] = OneWireROMGetByte(i);
    }

    // Set this to 0x40 and 0x0 per "Verify" section of Table 4 in application note 187.
    OneWireLastDiscrepancy = 0x40;
    OneWireLastDeviceFlag = 0;

    // NOTE: Table 4 says to set this to 0, but the "Verify" paragraph in application note 187 
    // does not mention changing this flag.
    OneWireLastFamilyDiscrepancy = 0; 

    // Searching should return the same OneWireROM value.
    bool isVerified = OneWireSearchROM(false);

    // Restore the OneWireROM to the previous value before the search.  If it is not already the 
    // same value set isVerified to false.
    for (int i = 0; i < 8; i++) {
        if (rom[i] != OneWireROMGetByte(i)) {
            isVerified = false;
            OneWireROMSetByte(i, rom[i]);
        }
    }

    // Restore the flags to the previous values before the search.
    OneWireLastDeviceFlag = lastDeviceFlag;
    OneWireLastDiscrepancy = lastDiscrepancy;
    OneWireLastFamilyDiscrepancy = lastFamilyDiscrepancy;

    return isVerified;
}

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
bool OneWireSearchROM(bool alarmSearch)
{
    uint8_t id_bit_number;
    int last_zero, rom_byte_number, search_result;
    int id_bit, cmp_id_bit;
    uint8_t rom_byte_mask, search_direction;

    // initialize for search
    id_bit_number = 1;
    last_zero = 0;
    rom_byte_number = 0;
    rom_byte_mask = 1;
    search_result = 0;
    ClearCrc8();

    // if the last call was not the last one
    if (!OneWireLastDeviceFlag) {
        // 1-Wire reset
        if (OneWireReset() != DevicePresent) {
            // reset the search
            OneWireLastDiscrepancy = 0;
            OneWireLastDeviceFlag = 0;
            OneWireLastFamilyDiscrepancy = 0;
            return false;
        }

        // issue the search command
        OneWireSendByte(alarmSearch ? 0xEC : 0xF0);

        // loop to do the search
        do {
            // read a bit and its complement
            id_bit = OneWireReadBit();
            cmp_id_bit = OneWireReadBit();

            if (id_bit == -1 || cmp_id_bit == -1)
                break;

            // check for no devices on 1-wire
            if ((id_bit == 1) && (cmp_id_bit == 1))
                break;
            else {
                // all devices coupled have 0 or 1
                if (id_bit != cmp_id_bit)
                    search_direction = id_bit; // bit write value for search
                else {
                    // if this discrepancy if before the Last Discrepancy
                    // on a previous next then pick the same as last time
                    if (id_bit_number < OneWireLastDiscrepancy)
                        search_direction = ((OneWireROMGetByte(rom_byte_number) & rom_byte_mask) > 0);
                    else
                        // if equal to last pick 1, if not then pick 0
                        search_direction = (id_bit_number == OneWireLastDiscrepancy);

                    // if 0 was picked then record its position in LastZero
                    if (search_direction == 0) {
                        last_zero = id_bit_number;

                        // check for Last discrepancy in family
                        if (last_zero < 9)
                            OneWireLastFamilyDiscrepancy = last_zero;
                    }
                }

                // set or clear the bit in the ROM byte rom_byte_number
                // with mask rom_byte_mask
                if (search_direction == 1)
                    OneWireROMSetByte(rom_byte_number,
                                      OneWireROMGetByte(rom_byte_number) | rom_byte_mask);
                else
                    OneWireROMSetByte(rom_byte_number,
                                      OneWireROMGetByte(rom_byte_number) & ~rom_byte_mask);

                // serial number search direction write bit
                if (!OneWireWriteBit(search_direction, false))
                    break;

                // increment the byte counter id_bit_number
                // and shift the mask rom_byte_mask
                id_bit_number++;
                rom_byte_mask <<= 1;

                // if the mask is 0 then go to new SerialNum byte rom_byte_number and reset mask
                if (rom_byte_mask == 0) {
                    DoCrc8(OneWireROMGetByte(rom_byte_number)); // accumulate the CRC
                    rom_byte_number++;
                    rom_byte_mask = 1;
                }
            }
        } while (rom_byte_number < 8); // loop until through all ROM bytes 0-7

        // if the search was successful then
        if (!((id_bit_number < 65) || (GetCrc8() != 0))) {
            // search successful so set LastDiscrepancy,LastDeviceFlag,search_result
            OneWireLastDiscrepancy = last_zero;

            // check for last device
            if (OneWireLastDiscrepancy == 0)
                OneWireLastDeviceFlag = 1;

            search_result = true;
        }
    }

    // if no device found then reset counters so next 'search' will be like a first
    if (!search_result || !OneWireROMGetByte(0)) {
        OneWireLastDiscrepancy = 0;
        OneWireLastDeviceFlag = 0;
        OneWireLastFamilyDiscrepancy = 0;
        search_result = false;
    }

    return search_result;
}