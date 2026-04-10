/*
 * main.c
 *
 * Application entry point and main loop for the TSMON IoT Temperature Sensor
 * firmware.
 *
 * Overview
 * --------
 * This file models the execution environment that would be provided by an
 * RTOS scheduler or a bare-metal super-loop on the target MCU. It drives the
 * three-phase application lifecycle:
 *
 *   1. Initialisation  — tsmon_app_init() configures the sensor driver and
 *                        prepares the application state (filter buffer, etc.).
 *                        A failure here is treated as a fatal error: the
 *                        firmware prints a diagnostic and exits rather than
 *                        running with an uninitialised sensor stack.
 *
 *   2. Periodic ticks  — tsmon_app_tick() is called in a loop, separated by a
 *                        platform_sleep(1) delay, giving the application its
 *                        1 Hz heartbeat. In production firmware this loop
 *                        would be driven by a hardware timer interrupt or an
 *                        RTOS tick at precisely 1000 ms intervals.
 *                        For this host-based test the loop runs 10 times
 *                        (10 seconds of simulated operation).
 *
 *   3. Shutdown        — tsmon_app_deinit() is called after the loop to allow
 *                        the application layer to release any resources it
 *                        holds (currently a no-op, reserved for future use).
 *
 * Platform portability
 * --------------------
 * The platform_sleep() helper abstracts the OS sleep call so the same
 * main.c compiles on both Windows (Sleep() in milliseconds) and POSIX
 * systems (sleep() in seconds) without #ifdef clutter in the main logic.
 */

#include <stdio.h>
#include "tsmon_app.h" /* The header for the application you've written */


/* Include for the sleep function, which varies by OS */
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/**
 * @brief  Cross-platform sleep wrapper.
 *
 * Suspends execution for the requested number of seconds. On Windows,
 * Sleep() takes milliseconds, so the argument is scaled by 1000. On POSIX
 * the standard sleep() function is used directly.
 *
 * In production embedded firmware this function would be replaced by an
 * RTOS delay (e.g., vTaskDelayUntil()) or a hardware timer wait to achieve
 * accurate 1 Hz periodicity regardless of tick execution time.
 *
 * @param  seconds  Number of seconds to sleep.
 */
/* Helper function for cross-platform sleep */
void platform_sleep(int seconds) {
#ifdef _WIN32
    Sleep(seconds * 1000);
#else
    sleep(seconds);
#endif
}

/**
 * @brief  Firmware entry point.
 *
 * Initialises the application, runs the 1 Hz tick loop for 10 iterations,
 * then shuts down cleanly. Each tick drives one full sensor read-filter-
 * evaluate-output cycle. Tick errors are treated as non-fatal: a diagnostic
 * is printed and the loop is aborted, but tsmon_app_deinit() is still called
 * to allow orderly cleanup.
 *
 * @return  0 on clean exit, -1 if initialisation fails.
 */
/* This is the main entry point for the program. */
int main(void) {
    printf("Application starting...\n");

    /* Call the application's one-time initialization function.
     * This verifies the sensor is present (JEDEC ID check over SPI) and
     * prepares the median filter state. Abort if it fails — running the
     * tick loop against an uninitialised sensor stack is undefined behaviour. */
    if (tsmon_app_init() != TSMON_ERR_SUCCESS) {
        printf("Error: Application initialization failed!\n");
        return -1;
    }

    printf("Initialization successful. Starting 1 Hz ticks...\n");

    /* Simulate the microcontroller's main loop, calling the tick function
     * periodically. For this test, we'll run it 10 times.
     * In real firmware this loop would be infinite; the exit condition here
     * exists only to allow the host-based simulation to terminate cleanly. */
    for (int i = 0; i < 10; ++i) {
        if (tsmon_app_tick() != TSMON_ERR_SUCCESS) {
            printf("Error: Application tick failed!\n");
            break;
        }
        /* In your main.c file */
    printf("Tick %d\n", i + 1);
    platform_sleep(1); /* Wait for 1 second. */
    }

    /* Call the application's shutdown function.
     * Gives the application layer the opportunity to release resources,
     * flush buffers, or power down peripherals before the process exits. */
    tsmon_app_deinit();
    printf("Application finished.\n");

    return 0;
}
