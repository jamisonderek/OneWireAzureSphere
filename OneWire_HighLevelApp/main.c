/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

// This sample shows how to use the UART plus additional hardware (resistors and transistors)
// to communicate with OneWire devices.  This example uses a DS18B20 OneWire temperature sensor.
// The LED 2 on the Azure sphere changes color depending on the temperature & the temperature
// information is displayed in the Visual Studio debug window.
//
// It uses the API for the following Azure Sphere application libraries:
// - UART (serial port)
// - GPIO (digital input for button)
// - log (displays messages in the Device Output window during debugging)
// - eventloop (system invokes handlers for timer events)

#include "ds18b20.h"
#include "eventloop_timer_utilities.h"
#include "onewire.h"
#include "onewirerom.h"
#include "onewiresearch.h"
#include "onewireuart.h"

#include "sleep.h"

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "applibs_versions.h"
#include <applibs/uart.h>
#include <applibs/gpio.h>
#include <applibs/log.h>
#include <applibs/eventloop.h>

// The following #include imports a "sample appliance" definition. This app comes with multiple
// implementations of the sample appliance, each in a separate directory, which allow the code to
// run on different hardware.
//
// By default, this app targets hardware that follows the MT3620 Reference Development Board (RDB)
// specification, such as the MT3620 Dev Kit from Seeed Studio.
//
// To target different hardware, you'll need to update CMakeLists.txt. For example, to target the
// Avnet MT3620 Starter Kit, change the TARGET_DIRECTORY argument in the call to
// azsphere_target_hardware_definition to "HardwareDefinitions/avnet_mt3620_sk".
//
// See https://aka.ms/AzureSphereHardwareDefinitions for more details.
#include <hw/sample_appliance.h>

/// <summary>
/// Exit codes for this application. These are used for the application exit code. 
/// They must all be between zero and 255, where zero is reserved for successful termination.
/// </summary>
typedef enum {
    ExitCode_Success = 0,
    ExitCode_TermHandler_SigTerm = 1,
    ExitCode_TemperatureTimer_Consume = 2,
    ExitCode_Init_EventLoop = 3,
    ExitCode_Init_TemperaturePollTimer = 4,
    ExitCode_Init_OpenLed = 5,
    ExitCode_Main_EventLoopFail = 6,
} ExitCode;

/// <summary>
/// File descriptor for the LED2 Red GPIO signal.
/// </summary>
static int gpioRgbRedFd = -1;

/// <summary>
/// File descriptor for the LED2 Green GPIO signal.
/// </summary>
static int gpioRgbGreenFd = -1;

/// <summary>
/// File descriptor for the LED2 Blue GPIO signal.
/// </summary>
static int gpioRgbBlueFd = -1;

/// <summary>
/// The temperature considered a low temperature reading.
/// </summary>
static float tLow = 65.0;

/// <summary>
/// The temperature considered a high temperature reading.
/// </summary>
static float tHigh = 75.0;

/// <summary>
/// Event loop used for dispatching events during the main program.
/// </summary>
EventLoop *eventLoop = NULL;

/// <summary>
/// Event loop timer that triggers periodically to take a temperature reading.
/// </summary>
EventLoopTimer *temperaturePollTimer = NULL;

/// <summary>
/// The exit code for the application.
/// </summary>
static volatile sig_atomic_t exitCode = ExitCode_Success;

static void TerminationHandler(int signalNumber);
static void TemperatureTimerEventHandler(EventLoopTimer *timer);
static void SetTemperatureLED(bool tempLow, bool tempHigh, bool tempNormal);
static ExitCode InitPeripheralsAndHandlers(void);
static void CloseFdAndPrintError(int fd, const char *fdName);
static void ClosePeripheralsAndHandlers(void);

/// <summary>
/// Signal handler for termination requests. This handler must be async-signal-safe.
/// </summary>
/// <param name="signalNumber">The number of the signal that caused invocation of the handler.</param>
static void TerminationHandler(int signalNumber)
{
    // Don't use Log_Debug here, as it is not guaranteed to be async-signal-safe.
    exitCode = ExitCode_TermHandler_SigTerm;
}

/// <summary>
/// Requests the latest temperature from all connected DS18B20 devices and
/// sets LED2 based on the temperature ranges.
/// </summary>
/// <param name="timer">The timer that invoked the handler.</param>
static void TemperatureTimerEventHandler(EventLoopTimer *timer)
{
    bool status;

    if (ConsumeEventLoopTimerEvent(timer) != 0) {
        exitCode = ExitCode_TemperatureTimer_Consume;
        return;
    }

    // Using SkipROM will cause the next command will go to all devices connected on the OneWire bus.
    status = OneWireSkipROM();
    Log_Debug("INFO: OneWireSkipROM returned %s.\n", status ? "true" : "false");

    // Request the devices to a temperature conversion.  We pass true for enabling the strong pullup, so
    // the device doesn't need to have a separate VCC wire connected (it can use parasitic power).  We pass
    // the maximum resolution of 12bits so that all devices have a chance to do conversion.  If you know all
    // devices are using a different bit resolution, you can use that value and conversion will happen quicker.
    status = Ds18b20ConvertT(true, ThermometerResolution12bits);
    Log_Debug("INFO: Ds18b20ConvertT returned %s.\n", status ? "true" : "false");

    // The DS18B20 uses a family code of 0x28.  We will only search for OneWire devices starting with that ID,
    // skiping any other device families that are on the OneWire bus. If you want to search for all OneWire 
    // devices use OneWireResetSearch instead of OneWireTargetSetup.
    OneWireTargetSetup(0x28);

    bool tempNormal = false;
    bool tempHigh = false;
    bool tempLow = false;

    bool searchStatus;
    do {
        // We pass false here, because we want to search fo all OneWire devices (within our family code) not
        // just alerted devices.
        searchStatus = OneWireSearchROM(false);
        Log_Debug("INFO: OneWireSearchROM returned %s.\n", status ? "true" : "false");
        if (!searchStatus) {
            // No more devices were found, so exit the loop.
            break;
        }
    
        // Display the ROM number in the debug console.
        OneWireDebugDumpROM();

        // The next command is for the device with the matching ROM identifier.
        status = OneWireMatchROM();
        Log_Debug("INFO: OneWireMatchROM returned %s.\n", status ? "true" : "false");

        if (status) {
            // Returns true if the device is connected to VCC, false if it is using single wire.
            status = Ds18b20ReadPowerSupplyVCC();
            Log_Debug("INFO: Ds18b20ReadPowerSupplyVCC returned %s.\n",
                      status ? "VCC powered" : "OneWire powered");
        }

        // The next command is for the device with the matching ROM identifier.
        status = OneWireMatchROM();
        Log_Debug("INFO: OneWireMatchROM returned %s.\n", status ? "true" : "false");

        if (status) {
            // Read the scratchpad (it has the temperature, resolution, tLow and tHigh values.)
            status = Ds18b20ReadScratchpad();
            Log_Debug("INFO: Ds18b20ReadScratchpad returned %s.\n", status ? "true" : "false");
        }

        if (status) {
            Log_Debug("INFO: Resolution is %d bits.\n", 9 + GetScratchpadResolution());

            Log_Debug("INFO: tLow is %d.\n", GetScratchpadtLow());

            Log_Debug("INFO: tHigh is %d.\n", GetScratchpadtHigh());

            float temp = GetScratchpadFahrenheit();
            Log_Debug("INFO: Temp is %gF.\n", temp);

            if (temp < tLow) {
                tempLow = true;
            } else if (temp > tHigh) {
                tempHigh = true;
            } else {
                tempNormal = true;
            }
        } else {
            Log_Debug("WARN: Read scratchpad failed; so this device data will not be used.\n");
        }
    } while (searchStatus);

    SetTemperatureLED(tempLow, tempHigh, tempNormal);
}

/// <summary>
/// Changes the LED based on the values provided:
/// Yellow - no readings present.
/// Green - all readings are normal.
/// Red - some readings are high.
/// Blue - some readings are blue.
/// Purple - some readings are high and some are low.
/// White - readings are a mix of high, low and normal values.
/// </summary>
/// <param name="tempLow">true if one or more probes are below the tLow value.</param>
/// <param name="tempHigh">true if one or more probes are above the tHigh value.</param>
/// <param name="tempNormal">true if one or more probes are within the [tLow, tHigh] range.</param>
static void SetTemperatureLED(bool tempLow, bool tempHigh, bool tempNormal)
{
    // Turn off the RGB led.
    GPIO_SetValue(gpioRgbRedFd, GPIO_Value_High);
    GPIO_SetValue(gpioRgbGreenFd, GPIO_Value_High);
    GPIO_SetValue(gpioRgbBlueFd, GPIO_Value_High);

    // Turn on green and red (makes yellow) when no readings present.
    if (!tempLow && !tempHigh && !tempNormal) {
        GPIO_SetValue(gpioRgbRedFd, GPIO_Value_Low);
        GPIO_SetValue(gpioRgbGreenFd, GPIO_Value_Low);
        return;
    }

    // Turn on green when all probes are tempNormal.
    if (tempNormal && !tempHigh && !tempLow) {
        GPIO_SetValue(gpioRgbGreenFd, GPIO_Value_Low);
        return;
    }

    // Turn on red when a probe is tempHigh (and not tempLow).
    // Note: Some probes may be tempNormal.
    if (tempHigh && !tempLow) {
        GPIO_SetValue(gpioRgbRedFd, GPIO_Value_Low);    
        return;
    }

    // Turn on blue when tempLow (and not tempHigh).
    // Note: some probes might be tempNormal.
    if (tempLow && !tempHigh) {
        GPIO_SetValue(gpioRgbBlueFd, GPIO_Value_Low);   
        return;
    }

    // Turn on red and blue (makes purple) when some probes tempHigh and tempLow (and none are normal).
    if (tempHigh && tempLow && !tempNormal) {
        GPIO_SetValue(gpioRgbRedFd, GPIO_Value_Low);
        GPIO_SetValue(gpioRgbBlueFd, GPIO_Value_Low);
        return;
    }

    // Turn on all three LED colors (makes white) when tempHigh, tempLow and tempNormal.
    if (tempHigh && tempLow && tempNormal) {
        GPIO_SetValue(gpioRgbRedFd, GPIO_Value_Low);
        GPIO_SetValue(gpioRgbGreenFd, GPIO_Value_Low);
        GPIO_SetValue(gpioRgbBlueFd, GPIO_Value_Low);
        return;
    }
}

/// <summary>
/// Set up SIGTERM termination handler, initialize peripherals, and set up event handlers.
/// </summary>
/// <returns>
/// ExitCode_Success if all resources were allocated successfully; otherwise another
/// ExitCode value which indicates the specific failure.
/// </returns>
static ExitCode InitPeripheralsAndHandlers(void)
{
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = TerminationHandler;
    sigaction(SIGTERM, &action, NULL);

    // The Red LED will be off when the value is High and will be on when the value is Low.
    gpioRgbRedFd = GPIO_OpenAsOutput(SAMPLE_RGBLED_RED, GPIO_OutputMode_PushPull, GPIO_Value_High);
    if (gpioRgbRedFd == -1) {
        Log_Debug("ERROR: Could not open Red LED GPIO: %s (%d).\n", strerror(errno), errno);
        return ExitCode_Init_OpenLed;
    }

    // The Green LED will be off when the value is High and will be on when the value is Low.
    gpioRgbGreenFd = GPIO_OpenAsOutput(SAMPLE_RGBLED_GREEN, GPIO_OutputMode_PushPull, GPIO_Value_High);
    if (gpioRgbGreenFd == -1) {
        Log_Debug("ERROR: Could not open Green LED GPIO: %s (%d).\n", strerror(errno), errno);
        return ExitCode_Init_OpenLed;
    }

    // The Blue LED will be off when the value is High and will be on when the value is Low.
    gpioRgbBlueFd = GPIO_OpenAsOutput(SAMPLE_RGBLED_BLUE, GPIO_OutputMode_PushPull, GPIO_Value_High);
    if (gpioRgbBlueFd == -1) {
        Log_Debug("ERROR: Could not open Blue led GPIO: %s (%d).\n", strerror(errno), errno);
        return ExitCode_Init_OpenLed;
    }

    // Header3, pin 1 = VCC
    // Header3, pin 2 = GND
    // Header2, pin 1 = RX  (SAMPLE_NRF52_UART)
    // Header2, pin 2 = TX  (SAMPLE_NRF52_UART)
    // Header2, pin 4 = 680 ohm resistor connected to RX (one wire bus).   (SAMPLE_NRF52_RESET)
    OneWireInit(SAMPLE_NRF52_UART, SAMPLE_NRF52_RESET);

    eventLoop = EventLoop_Create();
    if (eventLoop == NULL) {
        Log_Debug("Could not create event loop.\n");
        return ExitCode_Init_EventLoop;
    }

    // Take a temperature reading every 3 seconds.
    struct timespec checkPeriod1Second = {.tv_sec = 3, .tv_nsec = 0};
    temperaturePollTimer = CreateEventLoopPeriodicTimer(eventLoop, TemperatureTimerEventHandler, &checkPeriod1Second);
    if (temperaturePollTimer == NULL) {
        return ExitCode_Init_TemperaturePollTimer;
    }

    return ExitCode_Success;
}

/// <summary>
/// Closes a file descriptor and prints an error on failure.
/// </summary>
/// <param name="fd">File descriptor to close</param>
/// <param name="fdName">File descriptor name to use in error message</param>
static void CloseFdAndPrintError(int fd, const char *fdName)
{
    if (fd >= 0) {
        int result = close(fd);
        if (result != 0) {
            Log_Debug("ERROR: Could not close fd %s: %s (%d).\n", fdName, strerror(errno), errno);
        }
    }
}

/// <summary>
/// Close peripherals and handlers.
/// </summary>
static void ClosePeripheralsAndHandlers(void)
{
    DisposeEventLoopTimer(temperaturePollTimer);
    EventLoop_Close(eventLoop);

    Log_Debug("Closing file descriptors.\n");
    CloseFdAndPrintError(gpioRgbRedFd, "GpioRgbRed");
    CloseFdAndPrintError(gpioRgbGreenFd, "GpioRgbGreen");
    CloseFdAndPrintError(gpioRgbBlueFd, "GpioRgbBlue");
    OneWireClose();
}

/// <summary>
/// Main entry point for this application.
/// </summary>
/// <returns>
/// ExitCode value which indicates the specific failure.
/// </returns>
int main(int argc, char *argv[])
{
    Log_Debug("OneWire application starting.\n");
    exitCode = InitPeripheralsAndHandlers();

    // Use event loop to wait for events and trigger handlers, until an error or SIGTERM happens
    while (exitCode == ExitCode_Success) {
        EventLoop_Run_Result result = EventLoop_Run(eventLoop, -1, true);

        // Continue if interrupted by signal, e.g. due to breakpoint being set.
        if (result == EventLoop_Run_Failed && errno != EINTR) {
            exitCode = ExitCode_Main_EventLoopFail;
        }
    }

    ClosePeripheralsAndHandlers();
    Log_Debug("Application exiting.\n");
    return exitCode;
}