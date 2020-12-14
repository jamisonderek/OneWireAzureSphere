/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#include "sleep.h"
#include <time.h>

/// <summary>
/// Suspends the thread for the specified number of milliseconds.
/// </summary>
/// <param name="durationMs">The duration to sleep for.</param>
void SleepMilli(long durationMs)
{
    long durationSec = durationMs / 1000;
    durationMs = durationMs - (durationSec * 1000);
    const struct timespec sleepTime = {.tv_sec = durationSec, .tv_nsec = durationMs * 1000000};
    nanosleep(&sleepTime, NULL);
}
