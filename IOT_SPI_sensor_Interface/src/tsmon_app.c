/*
 * tsmon_app.c
 *
 * Application layer for the TSMON IoT Temperature Sensor system.
 *
 * Responsibilities
 * ----------------
 * This module owns all business logic that sits above the raw sensor driver:
 *
 *   1. Median filter  — A 5-sample sliding-window median filter removes
 *      transient noise spikes from the raw temperature stream before any
 *      threshold decision is made. The filter uses a copy-and-sort approach:
 *      on each tick the circular buffer is copied, sorted with qsort(), and
 *      the centre element (index 2 of 5) is selected as the filtered value.
 *      Until the buffer is full (first 4 ticks) the raw value is used
 *      directly to avoid an artificial ramp-up delay.
 *
 *   2. Hysteresis alert logic — The alert GPIO is controlled by a two-
 *      threshold hysteresis comparator to prevent rapid toggling when the
 *      temperature hovers near the set point:
 *
 *        State         Condition              Action
 *        ----------    --------------------   ----------------------
 *        Inactive      filtered_temp > 375    Activate alert (GPIO HIGH)
 *        Active        filtered_temp < 365    Deactivate alert (GPIO LOW)
 *        Either        365 <= temp <= 375     No change (hysteresis zone)
 *
 *   3. Fail-safe output — If the driver returns any error, GPIO 4 is driven
 *      LOW (de-asserted) before the error is propagated to the caller. This
 *      ensures the system downstream of the alert pin sees a safe state on
 *      communication faults rather than a latched or stale HIGH.
 *
 *   4. Telemetry snapshot — The last raw reading, last filtered reading, and
 *      current alert state are stored as static variables so that an external
 *      logging or reporting module can read them at any time via
 *      tsmon_app_get_telemetry() without re-running the sensor read cycle.
 */

#include "tsmon_app.h"
#include "temp_sensor.h"
#include "tsmon_hal.h"
#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * Application State
 * =========================================================================
 * All state is file-scoped (static) so that it cannot be accessed or
 * corrupted by other modules. External reads go through tsmon_app_get_telemetry().
 * ========================================================================= */

/* Current alert activation state.
 * true  = alert is active; GPIO 4 is driven HIGH.
 * false = alert is inactive; GPIO 4 is driven LOW.
 * Persists across ticks to implement the two-threshold hysteresis. */
static bool alert_is_active = false;

/* Size of the median filter sliding window.
 * Must be an odd number so there is always a unique middle element.
 * Increasing this value improves spike rejection at the cost of higher
 * latency in tracking genuine temperature trends. */
#define FILTER_WINDOW_SIZE 5

/* Circular buffer storing the last FILTER_WINDOW_SIZE raw temperature
 * readings (tenths of °C). New readings overwrite the oldest entry.
 * Initialised to all-zeros by tsmon_app_init(). */
static int16_t temp_history[FILTER_WINDOW_SIZE];

/* Write index into temp_history[]. Points to the slot where the next
 * reading will be written. Wraps back to 0 after reaching FILTER_WINDOW_SIZE. */
static uint8_t history_index = 0;

/* Flag that becomes true once FILTER_WINDOW_SIZE readings have been
 * collected. Before this flag is set the raw reading is used directly
 * as the filtered value (no partial-buffer median computation). */
static bool history_is_full = false;

/* Telemetry snapshot variables. Updated on every successful tick so that
 * tsmon_app_get_telemetry() always returns the most recent data without
 * needing to re-read the sensor. */
static int16_t last_raw_temp      = 0;
static int16_t last_filtered_temp = 0;

/* =========================================================================
 * Private Helpers
 * ========================================================================= */

/**
 * @brief  Comparator for qsort() operating on int16_t arrays.
 *
 * Returns negative, zero, or positive to indicate the ordering of two
 * int16_t values. Required by qsort() to sort the median filter copy
 * in ascending order before selecting the centre element.
 *
 * @param  a  Pointer to the first  int16_t value.
 * @param  b  Pointer to the second int16_t value.
 * @return    < 0 if *a < *b, 0 if *a == *b, > 0 if *a > *b.
 */
static int compare_int16(const void *a, const void *b) {
    int16_t val_a = *(const int16_t*)a;
    int16_t val_b = *(const int16_t*)b;
    if (val_a < val_b) return -1;
    if (val_a > val_b) return 1;
    return 0;
}

/* =========================================================================
 * Public API Implementation
 * ========================================================================= */

/**
 * @brief  One-time application initialisation.
 *
 * Zeroes the filter state so that stale readings from a previous run (e.g.,
 * after a soft-reset) cannot influence the first median calculation. Then
 * delegates to the sensor driver to confirm the sensor is present and
 * correctly identified via its JEDEC ID.
 */
tsmon_err_t tsmon_app_init(void) {
    memset(temp_history, 0, sizeof(temp_history));
    return tsmon_sensor_init();
}

/**
 * @brief  Graceful application shutdown. Currently a no-op.
 *
 * Placeholder for future resource cleanup such as disabling the SPI
 * peripheral or releasing a DMA channel.
 */
tsmon_err_t tsmon_app_deinit(void) {
    return TSMON_ERR_SUCCESS;
}

/**
 * @brief  Execute one 1 Hz application tick.
 *
 * Reads a raw temperature, runs it through the median filter, evaluates the
 * hysteresis comparator, and drives the alert GPIO accordingly. See the
 * module header for the full algorithmic description.
 */
tsmon_err_t tsmon_app_tick(void) {
    int16_t raw_temp;

    /* Read one raw temperature sample from the sensor driver.
     * The driver handles the full SPI sequence: AWAKE -> CONFIG -> RDTEMP -> SLEEP. */
    tsmon_err_t err = tsmon_sensor_read(&raw_temp);

    /* Store the raw reading for telemetry regardless of success/failure, so
     * the last attempted value is always accessible via get_telemetry(). */
    last_raw_temp = raw_temp;

    if (err != TSMON_ERR_SUCCESS) {
        /* Fail-safe: de-assert the alert output on any driver error to prevent
         * a latched HIGH from persisting when the sensor is unreachable. */
        tsmon_hal_gpio_write(TSMON_ALERT_GPIO_PIN, false);
        return err;
    }

    /* --- Median Filter: push new reading into circular buffer ------------ */

    temp_history[history_index++] = raw_temp;

    /* Wrap the write index and mark the buffer as full once all slots have
     * been written at least once. */
    if (history_index >= FILTER_WINDOW_SIZE) {
        history_index = 0;
        history_is_full = true;
    }

    /* Default filtered value: use raw until the buffer is fully primed.
     * This avoids computing a median over a mix of real and zero-padded data
     * during the first FILTER_WINDOW_SIZE-1 ticks after initialisation. */
    int16_t filtered_temp = raw_temp;

    if (history_is_full) {
        /* Copy the history buffer before sorting so the circular structure is
         * not disturbed. qsort() operates on the copy in-place. */
        int16_t sorted_history[FILTER_WINDOW_SIZE];
        memcpy(sorted_history, temp_history, sizeof(temp_history));
        qsort(sorted_history, FILTER_WINDOW_SIZE, sizeof(int16_t), compare_int16);

        /* Select the middle element of the sorted copy as the median.
         * For FILTER_WINDOW_SIZE = 5 this is index 2, which is robust to
         * up to 2 outlier samples in each window. */
        filtered_temp = sorted_history[FILTER_WINDOW_SIZE / 2];
    }

    /* Store the filtered reading for telemetry reporting. */
    last_filtered_temp = filtered_temp;

    /* --- Hysteresis Alert Logic ------------------------------------------ */

    /* Two separate thresholds prevent chattering when temperature oscillates
     * near a single set-point:
     *   Upper threshold: >375 (37.5 °C) -> activate alert
     *   Lower threshold: <365 (36.5 °C) -> deactivate alert
     * The 10-count (1 °C) hysteresis band [365, 375] causes no state change.
     *
     * State-dependent evaluation ensures we only check the relevant threshold
     * for the current alert state. */
    if (alert_is_active) {
        /* Alert is currently ON: only de-assert if temperature drops below
         * the lower hysteresis threshold. */
        if (filtered_temp < 365) alert_is_active = false;
    } else {
        /* Alert is currently OFF: only assert if temperature rises above
         * the upper hysteresis threshold. */
        if (filtered_temp > 375) alert_is_active = true;
    }

    /* Drive the alert GPIO to reflect the updated alert state and propagate
     * any GPIO fault back to the caller. */
    return tsmon_hal_gpio_write(TSMON_ALERT_GPIO_PIN, alert_is_active);
}

/**
 * @brief  Retrieve the most recent telemetry snapshot.
 *
 * Writes the last captured raw reading, filtered reading, and alert state
 * to the caller-provided pointers. NULL pointers are silently skipped so
 * the caller can selectively retrieve only the values it needs.
 */
void tsmon_app_get_telemetry(int16_t* raw_temp, int16_t* filtered_temp, bool* gpio_state) {
    if(raw_temp)      *raw_temp      = last_raw_temp;
    if(filtered_temp) *filtered_temp = last_filtered_temp;
    if(gpio_state)    *gpio_state    = alert_is_active;
}
