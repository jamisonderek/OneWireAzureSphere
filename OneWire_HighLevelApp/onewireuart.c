/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

/*
Copyright (C) 2020 Derek Jamison (@CodeAllNight), All Rights Reserved.
   Licensed under the MIT License.
   See accompanied LICENSE.txt for details.
*/

// Using the UART for OneWire was based on the following tutorial from Maxium Integrated:
// https://www.maximintegrated.com/en/design/technical-documents/tutorials/2/214.html

#include "onewireuart.h"
#include "sleep.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "applibs_versions.h"
#include <applibs/log.h>

static void OneWireEnableStrongPullupGpio(void);
static bool OneWireUartSetSpeed(UART_BaudRate_Type baud);
static bool OneWireUartWriteByte(uint8_t data, bool enableStrongPullup);
static int OneWireUartReadByte(void);

/// <summary>
/// The file descriptor used to access the pin used for pullup GPIO.  This must
/// always be set LOW (open) before sending any data on UART.  When it is set HIGH
/// then it's output will be on the OneWire bus.
/// </summary>
static int gpioPullupFd = -1;

/// <summary>
/// The file descriptor used to access the UART for sending and receiving data.
/// </summary>
static int uartFd = -1;

/// <summary>
/// The UART port to use for communication.
/// </summary>
static int uartId = -1;

/// <summary>
/// The current baud rate for the UART.  Reset pulses use 9600 baud.  Read/write 
/// operations use 115200 baud.
/// </summary>
static UART_BaudRate_Type uartBaud = 0;

/// <summary>
/// Initialize the UART and GPIO ports.
/// </summary>
/// <param name="uart">The UART port to use for communication.</param>
/// <param name="gpio">The GPIO port to use for pullup.</param>
/// <returns></returns>
bool OneWireUartInit(UART_Id uart, GPIO_Id gpio)
{
    // We use OpenSource, so a LOW value is disconnected (high impedance) and a HIGH value is 
    // current source that will be applied to the output pin.  We set the initial state to
    // high impedance.
    gpioPullupFd = GPIO_OpenAsOutput(gpio, GPIO_OutputMode_OpenSource, GPIO_Value_Low);
    if (gpioPullupFd == -1) {
        Log_Debug("ERROR: Could not open pullup [%d] GPIO: %s (%d).\n", gpio, strerror(errno),
                  errno);
        return false;
    }

    // Store the UART port, since we will end up needing it everytime we change the baud.
    uartId = uart;

    // Initialize the baud to 9600 so we are ready to send the reset pulse.
    return OneWireUartSetSpeed(9600);
}

/// <summary>
/// Sets the UART to the specified baud rate.
/// </summary>
/// <param name="baud">The baud rate (9600 or 115200).</param>
/// <returns>true if successfully set the baud rate, otherwise false.</returns>
static bool OneWireUartSetSpeed(UART_BaudRate_Type baud)
{
    // Return an error if the uartId is not set with the UART port to use.
    if (uartId == -1) {
        Log_Debug("ERROR: uartId was not set.  Call OneWireInit method to set value.\n");
        return false;
    }

    // If the UART is already at the specified rate, return true.
    if (uartBaud == baud) {
        return true;
    }

    // Make sure the baud rate is one of the expected values for this library.
    if (baud != 9600 && baud != 115200) {
        Log_Debug("PROGRAM ERROR: The only supported speeds are 9600 and 115200, not %d.\n", baud);
        return false;
    }

    // Close the existing OneWire UART port.
    if (uartFd >= 0) {
        int result = close(uartFd);
        uartFd = -1;
        uartBaud = 0;
        if (result != 0) {
            Log_Debug("ERROR: Could not close UART fd: %s (%d).\n", strerror(errno), errno);
            return false;
        }
    }

    // Open the UART port using <baud>N81.
    UART_Config uartConfig;
    UART_InitConfig(&uartConfig);
    uartConfig.flowControl = UART_FlowControl_None;
    uartConfig.baudRate = baud;
    uartConfig.parity = UART_Parity_None;
    uartConfig.dataBits = UART_DataBits_Eight;
    uartConfig.stopBits = UART_StopBits_One;
    uartFd = UART_Open(uartId, &uartConfig);
    if (uartFd == -1) {
        Log_Debug("ERROR: Could not open UART: %s (%d).\n", strerror(errno), errno);
        return false;
    }

    uartBaud = baud;
    return true;
}

/// <summary>
/// Close the UART and GPIO ports.
/// </summary>
void OneWireUartClose(void)
{
    if (uartFd >= 0) {
        int result = close(uartFd);
        if (result != 0) {
            Log_Debug("ERROR: Could not close UART fd: %s (%d).\n", strerror(errno), errno);
        }
        uartFd = -1;
        uartBaud = 0;
    }

    if (gpioPullupFd >= 0) {
        int result = close(gpioPullupFd);
        if (result != 0) {
            Log_Debug("ERROR: Could not close pullup fd: %s (%d).\n", strerror(errno), errno);
        }
        gpioPullupFd = -1;
    }
}

/// <summary>
/// Sends a reset pulse on the OneWire bus and checks to see if
/// any devices are present.
/// </summary>
/// <returns>returns UartImplDevicePresent if one or more devices responded,
/// UartImplNoDevices is no devices responsed, UartImplNoData if there was
/// an error during communication.
/// </returns>
OneWireUartResetResponse OneWireUartPulseReset(void)
{
    OneWireUartResetResponse response;

    // A reset pulse is long, so slow the speed to 9600 baud.
    if (!OneWireUartSetSpeed(9600)) {
        return UartImplHardwareFailure;
    }

    // We write out data from least significant to most significant.
    // So this will send a low pulse of 1 start bit+4 data bits = 5bits x 9600baud 
    // which is 521us (the acutal measured time was 517us.)
    OneWireUartWriteByte(0b11110000, false);
    
    // If a device is present, then it should pull the line low 
    // so we should read at least one more bit on.  If no devices
    // are present then we should read what we sent.  
    // (the actual measured time with a OneWire device was the line 
    // went high for 32uS and then went low for 132uS; this resulted
    // in the data being read as 0b11000000; e.g. the next two bits
    // significant bits were low.)
    int b = OneWireUartReadByte();
    if (b == -1) {
        Log_Debug("ERROR: No data during reset pulse.\n");
        response = UartImplNoData;
    } else if (b == 0b11110000) {
        Log_Debug("WARN: No devices detected.\n");
        response = UartImplNoDevices;
    } else {
        response = UartImplDevicePresent;
    }

    return response;
}

/// <summary>
/// Sends a data bit on the OneWire bus.  After sending
/// the bit the strong pullup can be enabled to help with parasitic charging.
/// </summary>
/// <param name="bit">The bit to send (either a 0 or a 1).</param>
/// <param name="enableStrongPullup">true if the pullup should be enabled after
/// sending the bit.</param>
/// <returns>true if successful, otherwise false.</returns>
bool OneWireUartPulseWriteBit(int bit, bool enableStrongPullup)
{
    // A bit is a small pulse, so set baud rate to 115200.
    if (!OneWireUartSetSpeed(115200)) {
        return false;
    }

    // 1 start bit+8 data bits = 9bits x 115200baud = 78.1us 
    // (measured at 75.1us)
    // 1 start bit+0 data bits = 1bits x 115200baud = 8.68uS
    // (measured at 5.5us)
    int sentData = bit ? 0b11111111 : 0b00000000;
    OneWireUartWriteByte(sentData, enableStrongPullup);

    // We should receive what we sent.
    int receivedData = OneWireUartReadByte();
    if (receivedData == -1) {
        Log_Debug("ERROR: No data.\n");
        return false;
    } else if (receivedData != sentData) {
        Log_Debug("ERROR: Received 0x%02x instead of 0x%02x.\n", receivedData, sentData);
        return false;
    }

    return true;
}

/// <summary>
/// Reads a bit from the OneWire bus.  The Azure Sphere sends a pulse and then reads the
/// line to see if the other device set the bit to 0 or 1.
/// </summary>
/// <returns>returns -1 if error, otherwise returns the bit (0 or 1) value.</returns>
int OneWireUartPulseReadBit(void)
{
    // A bit is a small pulse, so set baud rate to 115200.
    int data = -1;
    if (!OneWireUartSetSpeed(115200)) {
        return data;
    }

    // 1 start bit+0 data bits = 1bits x 115200baud = 8.68uS
    // (measured pulse at 5.5us.)
    OneWireUartWriteByte(0b11111111, false);

    // When OneWire device sent a high (high impedance) the
    // measured pulse was 5.5us and the UART was 0b11111111.
    // When the OneWire device pulled line low (sent a low) 
    // the measured pulse was extended so it lasted a total 
    // of 31.8uS (the value returned by the UART was 0b11111000.)
    int b = OneWireUartReadByte();
    if (b == -1) {
        Log_Debug("ERROR: No data during read bit.\n");
    } else if (b == 0b11111111) {
        data = 1;
    } else {
        data = 0;
    }

    return data;
}

/// <summary>
/// Disables the pullup GPIO on the OneWire bus.  This should be disabled if the OneWire
/// bus might get pulled to ground.  The pullup should be connected via a 680 ohm 
/// resistor, so max current provided is 5v/680ohm = 7.4mA of current.
/// </summary>
void OneWireDisableStrongPullupGpio(void)
{
    GPIO_SetValue(gpioPullupFd, GPIO_Value_Low);
}

/// <summary>
/// Writes a byte of data on the UART creating a pulse on the OneWire bus.
/// The length of the pulse is the start bit + the data bits.  Each bit
/// lasts (1/baud) seconds.
/// </summary>
/// <param name="data">The data to send.</param>
/// <param name="enableStrongPullup">set to true to enable the GPIO pullup
/// after sending the data.</param>
/// <returns>true if the data we sent, otherwise false.</returns>
static bool OneWireUartWriteByte(uint8_t data, bool enableStrongPullup)
{
    uint8_t buf[1] = {data};

    // Always disable the GPIO before sending data on the OneWire bus.
    OneWireDisableStrongPullupGpio();
    int bytesSent = write(uartFd, buf, 1);
    if (enableStrongPullup) {
        // NOTE: Ideally we would like the pullup to get enabled within 10us
        // *after* the OneWire line went high (which would vary depending on
        // when the most significant zero bit on in the data being sent.  
        // The write call is non-blocking, so what actually happens is the below method 
        // executes during the UART pulling the line low.  The 470 ohm resistor 
        // allows for the pullup to be on at the same time as the UART is sending
        // a low and have the bus remain at a low voltage.
        OneWireEnableStrongPullupGpio();
    }

    return (bytesSent == 1);
}

/// <summary>
/// Reads a byte of data from the UART.  This should read back what was sent unless
/// one of the devices pulled the OneWire bus low, in which case it will read back a
/// different value.
/// </summary>
/// <returns>
/// The byte that was received or -1 if there was an error.
/// </returns>
static int OneWireUartReadByte(void)
{
    int retryCount = 100;
    int data = -1;
    uint8_t receiveBuffer[2];
    while (data == -1 && retryCount--) {
        int bytesRead = read(uartFd, receiveBuffer, 1);
        if (bytesRead == 1) {
            data = receiveBuffer[0];
        } else {
            SleepMilli(1);
        }
    }

    return data;
}

/// <summary>
/// Enables the strong pullup on the OneWire bus, to help with parasitic charging.
/// The pullup should be connected via a 680 ohm resistor, so max current provided 
/// is 5v/680ohm = 7.4mA of current.
/// </summary>
static void OneWireEnableStrongPullupGpio(void)
{
    GPIO_SetValue(gpioPullupFd, GPIO_Value_High);
}
