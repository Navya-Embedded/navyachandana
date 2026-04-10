/*
 * tsmon_app.h
 *
 * Public API for the TSMON IoT Temperature Sensor application layer.
 *
 * Overview
 * --------
 * This header is the top-level interface that a host system (RTOS task,
 * bare-metal scheduler, or test harness) uses to drive the temperature
 * monitoring application. It intentionally exposes only three lifecycle
 * functions and one telemetry accessor, keeping the application's internal
 * state fully encapsulated in tsmon_app.c.
 *
 * Typical call sequence
 * ---------------------
 *
 *   1. Call tsmon_app_init() once at startup.
 *      - Initialises the median filter buffer.
 *      - Calls the driver to verify the sensor's JEDEC ID over SPI.
 *      - Returns TSMON_ERR_SUCCESS or an error if the sensor is absent
 *        or mis-identified.
 *
 *   2. Call tsmon_app_tick() periodically at 1 Hz from the main loop or a
 *      1-second timer ISR.
 *      - Reads a raw temperature from the sensor driver.
 *      - Pushes the reading into the 5-sample median filter.
 *      - Evaluates the filtered value against the hysteresis thresholds.
 *      - Drives GPIO 4 (alert output) HIGH or LOW accordingly.
 *      - Returns TSMON_ERR_SUCCESS, or propagates any driver error and
 *        forces the alert pin LOW as a fail-safe.
 *
 *   3. Optionally call tsmon_app_get_telemetry() at any time to retrieve the
 *      most recent snapshot of raw reading, filtered reading, and alert
 *      state for logging or remote dashboard reporting.
 *
 *   4. Call tsmon_app_deinit() during an orderly shutdown sequence.
 *      Currently a no-op, reserved for future resource cleanup.
 *
 * Thread safety
 * -------------
 * The internal state variables in tsmon_app.c are not protected by a mutex.
 * tsmon_app_tick() must be called from a single execution context. If
 * tsmon_app_get_telemetry() is called from a different context (e.g., a
 * logging task), ensure external synchronization to avoid torn reads of
 * the int16_t state variables.
 */

#pragma once
#include "tsmon_errors.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  One-time application initialisation.
 *
 * Clears the median filter history buffer to all zeros and calls
 * tsmon_sensor_init() to validate sensor presence via JEDEC ID.
 *
 * @return TSMON_ERR_SUCCESS      Sensor identified correctly; ready to tick.
 * @return TSMON_ERR_UNKNOWN      Sensor returned an unexpected JEDEC ID.
 * @return (other tsmon_err_t)      SPI or GPIO error propagated from the HAL.
 */
tsmon_err_t tsmon_app_init(void);

/**
 * @brief  Graceful application shutdown.
 *
 * Reserved for future resource cleanup (e.g., disabling the SPI peripheral,
 * releasing DMA channels). Currently returns TSMON_ERR_SUCCESS immediately.
 *
 * @return TSMON_ERR_SUCCESS  Always (in the current implementation).
 */
tsmon_err_t tsmon_app_deinit(void);

/**
 * @brief  Execute one 1 Hz application tick.
 *
 * This is the application's runtime heart-beat function. It must be called
 * at a steady 1 Hz rate; deviations will cause the median filter to operate
 * on non-uniformly-spaced samples, which may degrade noise rejection.
 *
 * On each call the function:
 *  1. Reads one raw temperature sample from the sensor driver.
 *  2. Stores the sample in the circular 5-element median filter buffer.
 *  3. Once the buffer is full, computes the median of the last 5 samples.
 *  4. Applies hysteresis logic to decide whether to assert/de-assert the
 *     thermal alert on GPIO 4:
 *       - Assert   (HIGH) if filtered_temp > 375  (37.5 °C).
 *       - De-assert (LOW) if filtered_temp < 365  (36.5 °C).
 *       - No change if filtered_temp is in the hysteresis zone [365, 375].
 *  5. Drives GPIO 4 to reflect the current alert state.
 *
 * On any error from the driver, GPIO 4 is forced LOW (fail-safe) before
 * returning the error code to the caller.
 *
 * @return TSMON_ERR_SUCCESS      Tick executed without error.
 * @return (other tsmon_err_t)      Driver or HAL error; GPIO 4 forced LOW.
 */
tsmon_err_t tsmon_app_tick(void);

/**
 * @brief  Retrieve the most recent telemetry snapshot.
 *
 * Returns the values captured during the last successful tsmon_app_tick()
 * call. Intended for use by a logging module, a serial debug console, or
 * a remote IoT dashboard uplink task.
 *
 * All pointer arguments are nullable. Pass NULL for any value you do not
 * need; the function will safely skip writing to that pointer.
 *
 * @param[out] raw_temp       Last raw ADC temperature (tenths of °C).
 *                            Reflects the unfiltered sensor reading.
 * @param[out] filtered_temp  Last median-filtered temperature (tenths of °C).
 *                            This is the value used for threshold evaluation.
 * @param[out] gpio_state     Current alert output state.
 *                            true = GPIO 4 HIGH (alert active).
 *                            false = GPIO 4 LOW  (no alert).
 */
void tsmon_app_get_telemetry(int16_t* raw_temp, int16_t* filtered_temp, bool* gpio_state);

#ifdef __cplusplus
}
#endif
