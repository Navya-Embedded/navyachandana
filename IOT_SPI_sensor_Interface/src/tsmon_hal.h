/*
 * tsmon_hal.h
 *
 * Public interface to the microcontroller peripheral abstraction layer (HAL).
 *
 * Overview
 * --------
 * This header declares the two hardware primitives that the entire application
 * stack depends on: GPIO control and SPI bus transactions. On a real target
 * these functions drive MCU registers directly. In this host-based build they
 * are backed by a software simulation in tsmon_hal.c, which lets the
 * full driver and application logic run and be tested on a development PC
 * without any physical hardware attached.
 *
 * Pin assignments
 * ---------------
 *  GPIO 2  - Chip Select (CS) for the SPI temperature sensor (active-LOW).
 *             Assert (drive LOW) before every SPI transaction; de-assert
 *             (drive HIGH) immediately after.
 *  GPIO 4  - Thermal alert output (active-HIGH). Driven HIGH by the
 *             application layer when the filtered temperature exceeds the
 *             upper hysteresis threshold and held HIGH until it drops below
 *             the lower hysteresis threshold.
 *
 * SPI command set
 * ---------------
 * All command opcodes below are JEDEC-style single-byte commands sent as the
 * first byte (tx_buf[0]) of an tsmon_hal_spi_transfer() call. The
 * sensor's state machine enforces the ordering: the device must be awake
 * before CONFIG or RDTEMP will take effect.
 *
 *  Wake / sleep cycle (mandatory around every measurement):
 *      AWAKE  (0xAB) -> CONFIG (0x06) -> RDTEMP (0x03) -> SLEEP (0xB9)
 *
 * CONFIG payload layout (10 bytes total):
 *  [0]  0x06  - CONFIG command opcode
 *  [1]  0x00  - Reserved; must be zero
 *  [2]  0xEE  - TSMON_CFG_MARKER: validity sentinel; sensor ignores the
 *               entire payload if this byte is not 0xEE
 *  [3]  0x00  - Reserved; must be zero
 *  [4]  0x03  - TSMON_CFG_MODE_TEMP: selects temperature measurement
 *  [5]  0x01  - TSMON_CFG_CALIBRATE: set to trigger self-calibration
 *               (optional; 0x00 for a normal measurement cycle)
 *  [6-9] 0x00 - Reserved; must be zero
 *
 * Test helpers (UNIT_TESTING builds only)
 * ----------------------------------------
 * When compiled with -DUNIT_TESTING the HAL exposes four additional functions
 * that allow test code to inject stimuli and read back internal simulation
 * state. These functions are declared at the bottom of this header and
 * implemented under an #ifdef UNIT_TESTING guard in tsmon_hal.c.
 * They must NEVER appear in production firmware.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "tsmon_errors.h"

/* =========================================================================
 * GPIO Pin Assignments
 * =========================================================================
 * Physical pin numbers used to index the GPIO array in the HAL simulation.
 * Replace with the actual MCU port/pin identifiers when porting to hardware.
 * ========================================================================= */

/* GPIO pin 2: Chip Select for the SPI temperature sensor (active-LOW).
 * Must be asserted (LOW = false) before every SPI transaction and
 * de-asserted (HIGH = true) immediately afterwards. */
#define TSMON_SPI_CS_PIN   (uint8_t) 0x2

/* Logic level macros for the CS pin.
 * ON  = false => GPIO driven LOW  => sensor selected (CS asserted).
 * OFF = true  => GPIO driven HIGH => sensor deselected (CS released). */
#define TSMON_CS_DEASSERT   (bool) true
#define TSMON_CS_ASSERT    (bool) false

/* GPIO pin 4: Thermal alert output (active-HIGH).
 * Driven HIGH by tsmon_app_tick() when the median-filtered temperature
 * exceeds the upper alert threshold (>37.5 °C / 375 in tenths-of-°C).
 * Returned LOW once the filtered temperature drops below the lower
 * hysteresis threshold (<36.5 °C / 365). */
#define TSMON_ALERT_GPIO_PIN   (uint8_t) 4

/* =========================================================================
 * SPI Command Opcodes
 * =========================================================================
 * Single-byte command codes sent as tx_buf[0] in each SPI transaction.
 * The sensor's internal state machine enforces that AWAKE precedes CONFIG
 * and RDTEMP, and that CONFIG sets the measurement mode before RDTEMP is
 * issued.
 * ========================================================================= */

/* RDID (0x9F): Read JEDEC device identification.
 * Requires a 3-byte transaction. The sensor returns:
 *   rx_buf[1] = Manufacturer ID (expected: TSMON_JEDEC_MFR_ID = 0x36)
 *   rx_buf[2] = Device ID       (expected: TSMON_JEDEC_DEV_ID = 0xC9)
 * This command is state-independent and can be issued at any time. */
#define TSMON_CMD_RDID   (uint8_t) 0x9F

/* Expected Manufacturer ID returned by the RDID command.
 * If rx_buf[1] != this value, the wrong device is on the bus. */
#define TSMON_JEDEC_MFR_ID (uint8_t) 0x36

/* Expected Device ID returned by the RDID command.
 * If rx_buf[2] != this value, the firmware and hardware are mismatched. */
#define TSMON_JEDEC_DEV_ID (uint8_t) 0xC9

/* AWAKE (0xAB): Exit low-power sleep mode.
 * Must be issued as the first command in every measurement sequence.
 * The sensor requires a 1-byte transaction (command only, no payload).
 * After waking, the sensor will accept CONFIG and RDTEMP commands. */
#define TSMON_CMD_AWAKE (uint8_t) 0xAB

/* SLEEP (0xB9): Enter low-power sleep mode.
 * Should be issued as the final command in every measurement sequence to
 * minimize current consumption between readings. A 1-byte transaction.
 * While sleeping the sensor ignores CONFIG and RDTEMP commands. */
#define TSMON_CMD_SLEEP (uint8_t) 0xB9

/* CONFIG (0x06): Write the sensor configuration register.
 * Requires a 10-byte transaction (see payload layout in file header).
 * The sensor only accepts the configuration when CONFIG_MARKER (0xEE) is
 * present at byte [2]; all other payloads are silently ignored.
 * Must be sent after AWAKE and before RDTEMP in every measurement cycle. */
#define TSMON_CMD_CONFIG (uint8_t) 0x06

/* RDTEMP (0x03): Read the current temperature measurement.
 * Requires a 4-byte transaction. The 16-bit signed result is returned in
 * big-endian order at rx_buf[2] and rx_buf[3]:
 *   temperature_tenths_c = (int16_t)(rx_buf[2] << 8) | rx_buf[3]
 * The value is in units of 0.1 °C (e.g., 375 = 37.5 °C).
 * Only valid when the sensor is awake AND configured for temperature mode. */
#define TSMON_CMD_RDTEMP (uint8_t) 0x03

/* CONFIG_MARKER (0xEE): Payload validity sentinel for the CONFIG command.
 * The sensor's firmware checks that byte [2] of the CONFIG payload equals
 * this value before applying any configuration change. This guards against
 * partially-corrupted SPI transfers silently mis-configuring the device. */
#define TSMON_CFG_MARKER (uint8_t) 0xEE

/* CONFIG_MEASUREMODE (0x03): Temperature measurement mode selector.
 * Place this value at byte [4] of the CONFIG payload to instruct the sensor
 * to perform temperature measurements. RDTEMP will return
 * TSMON_ERR_INVALID_CONFIG_MODE if a different mode is active. */
#define TSMON_CFG_MODE_TEMP (uint8_t) 0x03

/* CONFIG_CALIBRATE (0x01): Self-calibration trigger byte.
 * Place this value at byte [5] of the CONFIG payload to initiate a sensor
 * self-calibration sequence. Use 0x00 for normal measurement cycles.
 * Calibration is recommended after power-on and after large ambient
 * temperature step changes. */
#define TSMON_CFG_CALIBRATE (uint8_t) 0x01

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * HAL Public API
 * =========================================================================
 * These two functions are the only hardware primitives required by the
 * application and driver layers. All sensor communication is expressed
 * exclusively in terms of GPIO state changes and SPI transactions.
 * ========================================================================= */

/**
 * @brief  Drive a GPIO output pin to a given logic level.
 *
 * On the simulation this updates an internal gpio_pins[] array. On real
 * hardware this would write to the MCU output data register for the
 * corresponding port and pin.
 *
 * @param  pin    Pin index (0-15 in the simulation; maps to actual MCU pin
 *                on hardware). Out-of-range values return INVALID_ARG.
 * @param  value  true  = drive HIGH (3.3 V / logic 1)
 *                false = drive LOW  (0 V   / logic 0)
 *
 * @return TSMON_ERR_SUCCESS      Pin state updated successfully.
 * @return TSMON_ERR_INVALID_ARG  pin >= 16 (simulation limit).
 */
tsmon_err_t tsmon_hal_gpio_write(uint8_t pin, bool value);

/**
 * @brief  Perform a full-duplex SPI transaction.
 *
 * Simultaneously clocks out len bytes from tx_buf on MOSI and clocks in
 * len bytes on MISO into rx_buf. The Chip Select pin must be asserted
 * (driven LOW via tsmon_hal_gpio_write) by the caller BEFORE invoking
 * this function and de-asserted immediately after it returns.
 *
 * In the simulation the HAL acts as the sensor: it inspects tx_buf[0] to
 * determine which command is being issued and fills rx_buf with the
 * appropriate simulated response.
 *
 * @param  tx_buf  Pointer to the outgoing (MOSI) data buffer. Must not be
 *                 NULL and must be at least len bytes long.
 * @param  rx_buf  Pointer to the incoming (MISO) data buffer. Must not be
 *                 NULL and must be at least len bytes long. Will be zeroed
 *                 at the start of each transaction.
 * @param  len     Number of bytes to clock. Must match the expected
 *                 transaction length for the command in tx_buf[0]:
 *                   RDID   >= 3 bytes
 *                   AWAKE   = 1 byte
 *                   SLEEP   = 1 byte
 *                   CONFIG >= 10 bytes
 *                   RDTEMP >= 4 bytes
 *
 * @return TSMON_ERR_SUCCESS           Transaction completed.
 * @return TSMON_ERR_INVALID_CMD       Unrecognized command byte.
 * @return TSMON_ERR_INVALID_LENGTH    len too short for the command.
 * @return TSMON_ERR_INVALID_CONFIG_MODE RDTEMP issued in wrong mode.
 * @return TSMON_ERR_UNKNOWN           Injected SPI fault (test only).
 */
tsmon_err_t tsmon_hal_spi_transfer(uint8_t* tx_buf, uint8_t* rx_buf, size_t len);


/* =========================================================================
 * Test Helper Functions  (UNIT_TESTING builds only)
 * =========================================================================
 * These functions expose the internal simulation state of tsmon_hal.c
 * to allow the unit test suite to inject stimuli and read back results
 * without modifying any application or driver logic.
 *
 * All four functions are compiled only when -DUNIT_TESTING is defined.
 * They must NEVER be called from application or driver code and must
 * NEVER be linked into production firmware.
 * ========================================================================= */

/**
 * @brief  Inject a simulated raw temperature value into the HAL.
 *
 * Sets the value that the sensor simulation will return in rx_buf[2:3]
 * on the next RDTEMP command. The unit is tenths of degrees Celsius
 * (e.g., pass 375 to simulate 37.5 °C).
 *
 * @param  temp  Signed 16-bit temperature in tenths of °C.
 */
void tsmon_test_inject_temp(int16_t temp);

/**
 * @brief  Read back the current state of a simulated GPIO pin.
 *
 * Allows test assertions to verify that the application layer has driven
 * a GPIO pin to the expected logic level after a sequence of ticks.
 *
 * @param  pin  Pin index (same range as tsmon_hal_gpio_write).
 * @return true  if the pin is currently HIGH; false if LOW or out of range.
 */
bool tsmon_test_gpio_read(uint8_t pin);

/**
 * @brief  Override the JEDEC ID that the HAL returns for the next RDID.
 *
 * Allows tests to simulate a device ID mismatch (wrong device on the
 * bus or SPI wiring fault) without any real hardware change.
 *
 * @param  mfr_id  Manufacturer ID byte to inject into rx_buf[1].
 * @param  dev_id  Device ID byte to inject into rx_buf[2].
 */
void tsmon_test_inject_jedec_id(uint8_t mfr_id, uint8_t dev_id);

/**
 * @brief  Force the next SPI transaction to return an error.
 *
 * When set to true, the very next call to tsmon_hal_spi_transfer()
 * returns TSMON_ERR_UNKNOWN and auto-resets the flag so subsequent
 * transactions behave normally. Used to test error-propagation paths
 * that are unreachable without hardware-level fault injection.
 *
 * @param  fail  true = fail on next transaction; false = normal operation.
 */
void tsmon_test_inject_spi_fault(bool fail);

#ifdef __cplusplus
}
#endif
