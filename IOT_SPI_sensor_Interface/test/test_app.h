/*
 * test_app.h
 *
 * Declarations for the HAL simulation test helper functions.
 *
 * Overview
 * --------
 * This header is included exclusively by unit test source files. It exposes
 * the four helper functions that allow the test harness to control the
 * HAL simulation layer (tsmon_hal.c) without modifying any production
 * application or driver code.
 *
 * These helpers are compiled into the HAL only when the Makefile passes the
 * -DUNIT_TESTING preprocessor flag. They must never be included or called
 * from tsmon_app.c, temp_sensor.c, or main.c.
 *
 * Typical usage pattern (Arrange-Act-Assert)
 * ------------------------------------------
 *
 *   // Arrange: set the simulated environment
 *   tsmon_app_init();
 *   tsmon_test_inject_temp(376);   // 37.6 °C — above alert threshold
 *
 *   // Act: run the application for enough ticks to fill the filter buffer
 *   for (int i = 0; i < 10; i++) tsmon_app_tick();
 *
 *   // Assert: verify the expected hardware output
 *   assert(tsmon_test_gpio_read(TSMON_ALERT_GPIO_PIN) == true);
 *
 * After each test that modifies the simulated IDs or arms the fault injector,
 * restore the default state to prevent cross-test contamination:
 *
 *   tsmon_test_inject_jedec_id(TSMON_JEDEC_MFR_ID, TSMON_JEDEC_DEV_ID);
 *   tsmon_test_inject_spi_fault(false);
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>

/* --- Declarations for Test-Only Helper Functions --- */

/**
 * @brief  Inject a temperature value into the HAL simulation.
 *
 * Sets the value that tsmon_hal_spi_transfer() will return in
 * rx_buf[2:3] on the next RDTEMP (0x03) command. Unit is tenths of °C.
 *
 * @param  temp  16-bit signed temperature in tenths of °C.
 *               Examples: 300 = 30.0 °C, 375 = 37.5 °C, 376 = 37.6 °C.
 */
/* Injects a temperature value into the HAL simulation. */
void tsmon_test_inject_temp(int16_t temp);

/**
 * @brief  Read the current state of a simulated GPIO output pin.
 *
 * Reads back the logic level that the application has most recently written
 * to the specified GPIO pin via tsmon_hal_gpio_write(). Used by test
 * assertions to verify the alert pin state after a sequence of ticks.
 *
 * @param  pin  Pin index (0–15). Returns false for out-of-range values.
 * @return true  = pin is currently HIGH.
 *         false = pin is currently LOW or pin index is out of range.
 */
/* Gets the current state of a simulated GPIO pin for verification. */
bool tsmon_test_gpio_read(uint8_t pin);

/**
 * @brief  Override the JEDEC ID bytes returned by the next RDID command.
 *
 * Forces the HAL simulation to return specific Manufacturer and Device ID
 * bytes in response to an RDID (0x9F) command. Use this to test the sensor
 * initialisation path when the device ID does not match expectations.
 *
 * Restore the correct IDs after the test with:
 *   tsmon_test_inject_jedec_id(TSMON_JEDEC_MFR_ID, TSMON_JEDEC_DEV_ID);
 *
 * @param  mfr_id  Byte to place in rx_buf[1] (Manufacturer ID).
 * @param  dev_id  Byte to place in rx_buf[2] (Device ID).
 */
/* Forces the HAL to simulate a specific JEDEC ID on the next RDID command. */
void tsmon_test_inject_jedec_id(uint8_t mfr_id, uint8_t dev_id);

/**
 * @brief  Arm or disarm the one-shot SPI transaction fault injector.
 *
 * When armed (fail = true), the next call to tsmon_hal_spi_transfer()
 * returns TSMON_ERR_UNKNOWN and automatically disarms the injector. This
 * lets a test exercise a single-transaction fault without permanently
 * disrupting the simulation state.
 *
 * @param  fail  true  = fail on the very next SPI transaction then auto-reset.
 *               false = normal operation (disarms if previously armed).
 */
/* Forces the next SPI transaction in the HAL to return an error. */
void tsmon_test_inject_spi_fault(bool fail);
