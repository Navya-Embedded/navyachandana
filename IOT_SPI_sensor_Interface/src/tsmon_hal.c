/*
 * tsmon_hal.c
 *
 * Emulation layer for microcontroller peripherals. This models the behavior of
 * the target hardware to enable host-based development and testing.
 *
 * Architecture note
 * -----------------
 * In a production build this file is replaced by a BSP (Board Support Package)
 * that maps tsmon_hal_gpio_write() and tsmon_hal_spi_transfer() to
 * the actual MCU peripheral registers (e.g., STM32 HAL, NXP SDK, etc.).
 * The function signatures in tsmon_hal.h remain identical; only this
 * translation-unit changes. This means the application layer and driver need
 * zero modification when moving from simulation to real hardware.
 *
 * Simulation model
 * ----------------
 * The simulation maintains a small set of static state variables that mirror
 * the real sensor's internal registers:
 *
 *   gpio_pins[]          - 16-element array modelling MCU GPIO output states.
 *   sensor_is_sleeping   - Tracks whether the sensor has been woken (AWAKE
 *                          command) or put back to sleep (SLEEP command).
 *                          CONFIG and RDTEMP are no-ops while sleeping.
 *   measurement_mode     - The mode byte last written by a valid CONFIG
 *                          command. RDTEMP only succeeds when this equals
 *                          TSMON_CFG_MODE_TEMP (0x03).
 *   simulated_temperature- The 16-bit signed value (tenths of °C) that the
 *                          simulation returns for RDTEMP. Test code injects
 *                          values via tsmon_test_inject_temp().
 *
 * SPI simulation dispatch
 * -----------------------
 * tsmon_hal_spi_transfer() first checks that the CS pin (GPIO 2) is
 * asserted (LOW = false). If CS is not asserted the function returns SUCCESS
 * immediately — this mirrors real SPI hardware behaviour where a deselected
 * slave ignores all bus traffic.
 *
 * The command byte (tx_buf[0]) is then dispatched to the appropriate handler
 * branch. Each branch enforces the sensor's state preconditions and fills
 * rx_buf according to the datasheet specification.
 *
 * Test helpers
 * ------------
 * Four helper functions are compiled in when -DUNIT_TESTING is defined.
 * They expose write access to the static simulation variables so that the
 * test suite can:
 *   - Inject arbitrary temperature readings
 *   - Override the JEDEC ID response
 *   - Force SPI transaction failures
 *   - Read back GPIO output states
 * These functions must never appear in production firmware.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "tsmon_errors.h"
#include "tsmon_hal.h"


/* =========================================================================
 * Simulated Hardware State
 * =========================================================================
 * These static variables model the physical state of the MCU GPIO bank and
 * the sensor's internal register file. They persist across calls exactly as
 * real hardware state would persist between driver function calls.
 * ========================================================================= */

/* Models the output state of all 16 simulated GPIO pins.
 * Index corresponds to the pin number passed to tsmon_hal_gpio_write().
 * true = HIGH (3.3 V), false = LOW (0 V). Initialised to LOW (all false). */
static bool gpio_pins[16] = {false};

/* Models the sensor's AWAKE / SLEEP power state.
 * true  = sensor is sleeping  (initial state after power-on).
 * false = sensor is awake and ready to accept CONFIG / RDTEMP commands.
 * Transitions are driven by the AWAKE (0xAB) and SLEEP (0xB9) commands. */
static bool sensor_is_sleeping = true;

/* Models the sensor's currently active measurement mode register.
 * Set to 0x00 on power-on (no mode configured).
 * Updated to TSMON_CFG_MODE_TEMP (0x03) by a valid CONFIG transaction.
 * RDTEMP returns TSMON_ERR_INVALID_CONFIG_MODE if this != 0x03. */
static uint8_t measurement_mode = 0x00;

/* The temperature value (in tenths of °C) that the simulation will return
 * in bytes [2:3] of the RDTEMP response. Defaults to 370 (= 37.0 °C).
 * Overridden by tsmon_test_inject_temp() during unit tests. */
static int16_t simulated_temperature = 370; /* Default to 37.0 C */

/* Simulated JEDEC Manufacturer ID. Normally equals TSMON_JEDEC_MFR_ID.
 * Overridden by tsmon_test_inject_jedec_id() to test ID-mismatch paths. */
static uint8_t sim_mfr_id = TSMON_JEDEC_MFR_ID;

/* Simulated JEDEC Device ID. Normally equals TSMON_JEDEC_DEV_ID.
 * Overridden by tsmon_test_inject_jedec_id() to test ID-mismatch paths. */
static uint8_t sim_dev_id = TSMON_JEDEC_DEV_ID;

/* One-shot SPI fault injection flag. When true the next call to
 * tsmon_hal_spi_transfer() returns TSMON_ERR_UNKNOWN and resets
 * this flag back to false, so only a single transaction is affected.
 * Set via tsmon_test_inject_spi_fault(). */
static bool sim_spi_should_fail = false;


/* =========================================================================
 * HAL Implementation
 * ========================================================================= */

/**
 * @brief Simulates setting a GPIO pin's output state.
 *
 * Validates the pin index against the simulation's 16-pin limit then
 * stores the requested logic level in gpio_pins[]. On real hardware
 * this function would write to the MCU's GPIO output data register.
 */
tsmon_err_t tsmon_hal_gpio_write(uint8_t pin, bool value) {
  /* Validate pin number against the simulated hardware limits. */
  if (pin >= 16) {
    return TSMON_ERR_INVALID_ARG;
  }
  /* Update the state of the simulated pin. */
  gpio_pins[pin] = value;
  return TSMON_ERR_SUCCESS;
}

/**
 * @brief Simulates a full SPI transaction, acting as the temperature sensor.
 *
 * This function is the core of the hardware simulation. It first checks the
 * CS pin to honour the SPI chip-select protocol, then dispatches on the
 * command byte to the appropriate sensor behaviour handler.
 *
 * Each branch mirrors the sensor datasheet specification:
 *  RDID   - Returns the simulated JEDEC ID bytes regardless of sleep state.
 *  AWAKE  - Clears the sensor_is_sleeping flag.
 *  SLEEP  - Sets   the sensor_is_sleeping flag.
 *  CONFIG - Validates the marker byte and updates measurement_mode.
 *  RDTEMP - Checks state and mode then encodes simulated_temperature.
 *  Other  - Returns TSMON_ERR_INVALID_CMD.
 */
tsmon_err_t tsmon_hal_spi_transfer(uint8_t* tx_buf, uint8_t* rx_buf, size_t len) {
  /* Per SPI protocol, the peripheral is ignored if Chip Select is not
   * asserted (active-low). gpio_pins[CS] == false means CS is driven LOW,
   * which means the sensor IS selected. Return early if not selected. */
  if (gpio_pins[TSMON_SPI_CS_PIN] != false) {
    return TSMON_ERR_SUCCESS;
  }

  /* Ensure a clean slate for the receive buffer before filling it.
   * This prevents stale data from a previous transaction leaking through. */
  memset(rx_buf, 0, len);
  uint8_t command = tx_buf[0];

  /* -------------------------------------------------------------------
   * RDID (0x9F): Read JEDEC identification — state-independent.
   * Returns Manufacturer ID at rx_buf[1] and Device ID at rx_buf[2].
   * A minimum of 3 bytes must be exchanged for valid ID capture.
   * ------------------------------------------------------------------- */
  if (command == TSMON_CMD_RDID) {
    if (sim_spi_should_fail) {
        sim_spi_should_fail = false; /* Reset after one use */
        return TSMON_ERR_UNKNOWN;
    }
    if (len < 3) return TSMON_ERR_INVALID_LENGTH;
    rx_buf[1] = sim_mfr_id; /* Use the test variable */
    rx_buf[2] = sim_dev_id; /* Use the test variable */
  }
  /* -------------------------------------------------------------------
   * AWAKE (0xAB): Wake the sensor from its low-power sleep state.
   * Clears sensor_is_sleeping to allow CONFIG and RDTEMP to proceed.
   * ------------------------------------------------------------------- */
  else if (command == TSMON_CMD_AWAKE) {
    sensor_is_sleeping = false;
  }
  /* -------------------------------------------------------------------
   * SLEEP (0xB9): Put the sensor back into low-power sleep mode.
   * Sets sensor_is_sleeping; subsequent CONFIG / RDTEMP will be ignored.
   * ------------------------------------------------------------------- */
  else if (command == TSMON_CMD_SLEEP) {
    sensor_is_sleeping = true;
  }
  /* -------------------------------------------------------------------
   * CONFIG (0x06): Write the sensor configuration register.
   * Only processed while the sensor is awake. The payload must be at
   * least 10 bytes and must contain the validity marker (0xEE) at [2].
   * If the marker is present, measurement_mode is updated from byte [4].
   * The optional calibration flag at byte [5] is parsed but reserved for
   * future implementation.
   * ------------------------------------------------------------------- */
  else if (command == TSMON_CMD_CONFIG) {
    /* Command is state-dependent; ignore if the sensor is not awake. */
    if (sensor_is_sleeping) return TSMON_ERR_SUCCESS;
    if (len < 10) return TSMON_ERR_INVALID_LENGTH;

    uint8_t config_marker       = tx_buf[2];
    uint8_t config_measure_mode = tx_buf[4];
	uint8_t calibration_flag    = tx_buf[5];
    if (calibration_flag == TSMON_CFG_CALIBRATE) {
		/* Calibration triggered: reserved for future implementation. */
	}
    /* The sensor's state is only updated upon receiving a valid configuration.
     * An invalid marker causes the entire payload to be silently discarded,
     * protecting against partially-corrupted SPI frames. */
    if (config_marker == TSMON_CFG_MARKER) {
        measurement_mode = config_measure_mode;
    }
	
  }
  /* -------------------------------------------------------------------
   * RDTEMP (0x03): Read the current temperature measurement.
   * Only valid when the sensor is awake AND configured for temperature
   * measurement mode. Encodes simulated_temperature as a big-endian
   * signed 16-bit integer into rx_buf[2] (MSB) and rx_buf[3] (LSB).
   * ------------------------------------------------------------------- */
  else if (command == TSMON_CMD_RDTEMP) {
    /* Command is state-dependent; ignore if the sensor is not awake. */
    if (sensor_is_sleeping) return TSMON_ERR_SUCCESS;

    /* Reading is only valid if the sensor is in the correct measurement mode.
     * Returning an error here forces the driver to re-issue CONFIG before
     * each read sequence, which prevents stale mode state. */
    if (measurement_mode != TSMON_CFG_MODE_TEMP) {
      return TSMON_ERR_INVALID_CONFIG_MODE;
    }
    if (sim_spi_should_fail) {
        sim_spi_should_fail = false; /* Reset after one use */
        return TSMON_ERR_UNKNOWN;
    }
    /* ... other checks for state and mode ... */
    if (len < 4) return TSMON_ERR_INVALID_LENGTH;

    /* Encode the simulated temperature as a big-endian 16-bit signed integer.
     * MSB lands in rx_buf[2], LSB in rx_buf[3] — matching the driver's
     * decode logic: (rx_buf[2] << 8) | rx_buf[3]. */
    /* Use the simulated temperature variable */
    rx_buf[2] = (uint8_t)(simulated_temperature >> 8);
    rx_buf[3] = (uint8_t)(simulated_temperature & 0xFF);
  }
  else {
    /* An unrecognized command was sent by the driver.
     * Returning INVALID_CMD surfaces any opcode bugs in the driver layer
     * early during development rather than silently doing nothing. */
    return TSMON_ERR_INVALID_CMD;
  }

  return TSMON_ERR_SUCCESS;
}

/* =========================================================================
 * Test Helper Implementations  (UNIT_TESTING builds only)
 * =========================================================================
 * Compiled in only when the Makefile passes -DUNIT_TESTING. Each helper
 * provides controlled write or read access to one internal simulation
 * variable, keeping the test API minimal and explicit.
 * ========================================================================= */
/* In tsmon_hal.c */
#ifdef UNIT_TESTING

/**
 * @brief Inject a temperature value into the HAL simulation.
 *
 * After this call, the next successful RDTEMP transaction will return this
 * value encoded as a big-endian int16_t in rx_buf[2:3].
 */
void tsmon_test_inject_temp(int16_t temp) {
    simulated_temperature = temp;
}

/**
 * @brief Read back the current state of a simulated GPIO pin.
 *
 * Used by test assertions to verify that the application layer has correctly
 * driven GPIO 4 (alert output) HIGH or LOW after a sequence of tick calls.
 * Returns false for out-of-range pin numbers rather than triggering UB.
 */
bool tsmon_test_gpio_read(uint8_t pin) {
    if (pin >= 16) {
        return false;
    }
    return gpio_pins[pin];
}

/**
 * @brief Override the simulated JEDEC ID returned by the next RDID command.
 *
 * Allows tests to simulate a device identity mismatch without changing any
 * wiring. After the test, restore the correct IDs by calling this function
 * again with TSMON_JEDEC_MFR_ID and TSMON_JEDEC_DEV_ID.
 */
void tsmon_test_inject_jedec_id(uint8_t mfr_id, uint8_t dev_id) {
    sim_mfr_id = mfr_id;
    sim_dev_id = dev_id;
}

/**
 * @brief Arm or disarm the one-shot SPI fault injector.
 *
 * When armed (fail = true), the next call to tsmon_hal_spi_transfer()
 * returns TSMON_ERR_UNKNOWN and automatically disarms. This lets tests
 * exercise single-transaction failure paths without permanent fault state.
 */
void tsmon_test_inject_spi_fault(bool fail) {
    sim_spi_should_fail = fail;
}
#endif /* UNIT_TESTING */
