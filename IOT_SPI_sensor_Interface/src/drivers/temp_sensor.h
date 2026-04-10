/*
 * temp_sensor.h
 *
 * Public interface for the TSMON SPI temperature sensor driver.
 *
 * Overview
 * --------
 * This header defines the two public functions that form the sensor driver
 * API. The driver abstracts the complete SPI communication protocol required
 * to read a temperature value from the JEDEC-compliant TSMON temperature
 * sensor, so that the application layer deals only with initialisation and
 * typed temperature readings — not with raw command bytes or bus timing.
 *
 * Dependencies
 * ------------
 * The driver depends on the HAL functions declared in tsmon_hal.h:
 *   - tsmon_hal_gpio_write()     to assert and de-assert the CS pin
 *   - tsmon_hal_spi_transfer() to exchange bytes with the sensor
 *
 * No other hardware dependencies exist, making the driver fully portable
 * to any MCU that provides these two HAL primitives.
 *
 * Measurement sequence (executed by tsmon_sensor_read)
 * ---------------------------------------------------------------
 *  Step 1 — AWAKE  (0xAB, 1 byte):  Wake the sensor from low-power sleep.
 *  Step 2 — CONFIG (0x06, 10 bytes): Set measurement mode to temperature
 *                                    (TSMON_CFG_MODE_TEMP = 0x03).
 *  Step 3 — RDTEMP (0x03, 4 bytes):  Read the 16-bit signed result.
 *                                    Temperature = (rx[2] << 8) | rx[3]
 *                                    Units: tenths of °C (375 = 37.5 °C).
 *  Step 4 — SLEEP  (0xB9, 1 byte):  Return sensor to sleep to save power.
 *
 * The Chip Select (CS) pin (GPIO 2) is asserted before each individual SPI
 * transaction and de-asserted immediately after.
 *
 * Temperature encoding
 * --------------------
 * All temperature values are expressed as signed 16-bit integers in units
 * of 0.1 °C. To convert to floating-point degrees Celsius:
 *
 *     float temp_c = (float)temp_in_tenths_c / 10.0f;
 *
 * Example values:
 *   -100 = -10.0 °C
 *      0 =   0.0 °C
 *    365 =  36.5 °C  (lower hysteresis threshold)
 *    375 =  37.5 °C  (upper hysteresis threshold)
 *    850 =  85.0 °C  (typical sensor maximum)
 */

/* In a new file: temp_sensor.h */

#pragma once
#include "tsmon_errors.h"
#include <stdint.h>

/**
 * @brief  Initialise and verify the temperature sensor.
 *
 * Issues an RDID (0x9F) command and reads 3 bytes. The returned Manufacturer
 * ID (rx_buf[1]) and Device ID (rx_buf[2]) are compared against the expected
 * values defined in tsmon_hal.h (0x36 and 0xC9 respectively).
 *
 * If either ID byte does not match, TSMON_ERR_UNKNOWN is returned,
 * indicating that the device on the SPI bus is not the expected sensor
 * (wrong device, wrong orientation, or faulty connection).
 *
 * This function should be called once during system startup, before any
 * call to tsmon_sensor_read(). It is safe to call again as a
 * re-initialisation or connectivity self-test.
 *
 * @return TSMON_ERR_SUCCESS      Sensor present and correctly identified.
 * @return TSMON_ERR_UNKNOWN      JEDEC ID mismatch — wrong or absent device.
 * @return TSMON_ERR_INVALID_LENGTH  Transaction length violation in HAL.
 * @return (other tsmon_err_t)      GPIO or SPI HAL error propagated upward.
 */
tsmon_err_t tsmon_sensor_init(void);

/**
 * @brief  Perform a complete temperature measurement and return the result.
 *
 * Executes the full 4-step measurement sequence (AWAKE, CONFIG, RDTEMP,
 * SLEEP) and decodes the big-endian 16-bit signed result from the SPI
 * response buffer into *temp_in_tenths_c.
 *
 * Each step in the sequence is checked for errors. If any step fails, the
 * error is returned immediately and the sensor may be left in an
 * intermediate state (e.g., awake). The next call to this function will
 * re-issue AWAKE and re-configure the sensor, which is safe.
 *
 * @param[out] temp_in_tenths_c  Pointer to the variable that will receive
 *                               the temperature reading in tenths of °C.
 *                               Must not be NULL.
 *
 * @return TSMON_ERR_SUCCESS            Temperature read and decoded.
 * @return TSMON_ERR_INVALID_ARG        temp_in_tenths_c is NULL.
 * @return TSMON_ERR_INVALID_CONFIG_MODE Sensor not in temperature mode.
 * @return (other tsmon_err_t)            HAL error propagated from any step.
 */
tsmon_err_t tsmon_sensor_read(int16_t* temp_in_tenths_c);
