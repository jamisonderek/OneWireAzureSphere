#pragma once

#include <stdint.h>

/// <summary>
/// Modified from https://www.maximintegrated.com/en/design/technical-documents/app-notes/1/187.html
/// --------------------------------------------------------------------------
/// Calculate the CRC8 of the byte value provided with the current
/// global 'crc8' value. Returns current global crc8 value
/// </summary>
/// <param name="value">The next byte of data to use in the CRC8 calculation.</param>
/// <returns>The current global crc8 value.</returns>
uint8_t DoCrc8(uint8_t value);

/// <summary>
/// Clears the global crc8 value.
/// </summary>
void ClearCrc8(void);

/// <summary>
/// Returns the global crc8 value.
/// </summary>
uint8_t GetCrc8(void);