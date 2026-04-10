/*
 * tsmon_errors.h
 *
 * Centralized error code definitions for the TSMON IoT Temperature Sensor
 * application. All modules (application layer, driver, and HAL) return
 * values of type tsmon_err_t so that error propagation is uniform
 * throughout the call stack.
 *
 * Design rationale
 * ----------------
 *  - Using a single typedef for all error returns makes it trivial to
 *    add new peripheral-specific codes without touching existing APIs.
 *  - Codes 0-2 are intentionally kept generic so they can be reused by
 *    any module in the system.
 *  - Codes 3-8 are peripheral / communication specific and correspond
 *    to observable hardware fault conditions on the SPI bus or GPIO
 *    subsystem.
 *
 * Usage
 * -----
 *  Always compare against TSMON_ERR_SUCCESS rather than 0 directly
 *  to keep the code portable if the success sentinel ever changes:
 *
 *      if (tsmon_sensor_init() != TSMON_ERR_SUCCESS) { ... }
 *
 */

#pragma once
#include "stdint.h"

/* Define a common type for all error codes for consistency. */
typedef int tsmon_err_t;

/* =========================================================================
 * Standard Application Error Codes
 * =========================================================================
 * These codes are module-agnostic and may be returned by any function
 * in the system.
 * ========================================================================= */

/* The operation completed successfully. This is the only "happy path" value;
 * all other codes indicate a problem that the caller must handle. */
#define TSMON_ERR_SUCCESS             (uint32_t) 0

/* A generic, unspecified error occurred. Also used when the sensor returns an
 * unexpected JEDEC ID during initialization, indicating a device mismatch or
 * a corrupted SPI response. */
#define TSMON_ERR_UNKNOWN             (uint32_t) 1

/* A function was called with an invalid argument (e.g., a null pointer or an
 * out-of-range value). The operation was aborted before any hardware was
 * touched, so no peripheral state was changed. */
#define TSMON_ERR_INVALID_ARG         (uint32_t) 2


/* =========================================================================
 * Custom Peripheral and Communication Error Codes
 * =========================================================================
 * These codes map directly to observable fault conditions on the embedded
 * hardware peripherals. They are intended to aid field diagnostics and
 * automated test validation.
 * ========================================================================= */

/* An invalid or unsupported command opcode was sent to a peripheral. This
 * usually means the driver issued a byte that is not in the sensor's command
 * set as defined in the datasheet. */
#define TSMON_ERR_INVALID_CMD         (uint32_t) 3

/* An error occurred while trying to drive a GPIO pin HIGH (set). This may
 * indicate a pin conflict, an out-of-range pin number, or a hardware latch
 * fault on the MCU GPIO peripheral. */
#define TSMON_ERR_GPIO_SET            (uint32_t) 4

/* An error occurred while trying to drive a GPIO pin LOW (clear / unset).
 * Mirrors TSMON_ERR_GPIO_SET for the opposite drive direction so that
 * diagnostics can distinguish between set and clear failures. */
#define TSMON_ERR_GPIO_UNSET          (uint32_t) 5

/* A provided buffer length or SPI transaction length was incorrect for the
 * requested operation. For example, the RDID response requires at least 3
 * bytes and the RDTEMP response requires at least 4 bytes; shorter lengths
 * trigger this error. */
#define TSMON_ERR_INVALID_LENGTH      (uint32_t) 6

/* An operation was attempted while the peripheral was in an incompatible
 * state or measurement mode. The most common cause is issuing RDTEMP before
 * the sensor has been configured for temperature measurement mode via the
 * CONFIG command. */
#define TSMON_ERR_INVALID_CONFIG_MODE (uint32_t) 7

/* The sensor has indicated that a self-calibration cycle is required before
 * reliable measurements can be obtained. Trigger calibration by setting byte
 * [5] of the CONFIG payload to TSMON_CFG_CALIBRATE (0x01). */
#define TSMON_ERR_NEED_CALIBRATION (uint32_t) 8
