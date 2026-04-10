/*
 * temp_sensor.c
 *
 * TSMON SPI Temperature Sensor Driver — implementation.
 *
 * Overview
 * --------
 * This module implements the two public driver functions declared in
 * temp_sensor.h: tsmon_sensor_init() and tsmon_sensor_read().
 * All communication with the physical (or simulated) sensor goes exclusively
 * through the HAL primitives in tsmon_hal.h — no raw register access
 * or platform-specific code appears here.
 *
 * Private helper
 * --------------
 * execute_command() reduces code duplication for the three single-byte
 * commands (AWAKE, SLEEP, and any future command that has no response
 * payload). It handles the CS assert/transaction/de-assert pattern so that
 * the public functions only need to focus on sequence logic.
 *
 * CS pin protocol
 * ---------------
 * Every SPI transaction must be bracketed by:
 *   tsmon_hal_gpio_write(TSMON_SPI_CS_PIN, TSMON_CS_ASSERT)   // CS LOW
 *   tsmon_hal_spi_transfer(...)
 *   tsmon_hal_gpio_write(TSMON_SPI_CS_PIN, TSMON_CS_DEASSERT)  // CS HIGH
 *
 * The de-assert call after a failed transaction is always made (no early
 * return before de-asserting) to guarantee the CS line is not left LOW,
 * which would permanently select the sensor and corrupt subsequent bus
 * traffic.
 *
 * Error propagation
 * -----------------
 * Errors from the HAL are propagated immediately to the caller without
 * logging or masking. This keeps the driver thin and lets the application
 * layer decide how to handle (retry, report, or fail-safe) each error class.
 */

/* In a new file: temp_sensor.c */

#include "temp_sensor.h"
#include "tsmon_hal.h" /* Include the HAL interface to access GPIO and SPI functions. */
#include <string.h>          /* Needed for memset */

/* =========================================================================
 * Private Helper Functions
 * ========================================================================= */

/**
 * @brief  Execute a simple single-byte SPI command with no response payload.
 *
 * This private helper consolidates the CS-assert / transact / CS-de-assert
 * pattern for commands like AWAKE (0xAB) and SLEEP (0xB9) that carry no
 * additional data bytes. Using this helper avoids duplicating the CS
 * management code in the main functions.
 *
 * The de-assert call is always made, even when spi_transact fails, to
 * prevent the CS line from being left asserted (LOW) on error, which would
 * keep the sensor selected and corrupt subsequent bus traffic.
 *
 * @param  command  The SPI command byte to send (e.g., TSMON_CMD_AWAKE).
 * @return TSMON_ERR_SUCCESS or an error propagated from the HAL.
 */
static tsmon_err_t execute_command(uint8_t command) {
    uint8_t tx_buf[1] = {command};
    uint8_t rx_buf[1] = {0};
    tsmon_err_t err;

    /* Assert the chip select pin (active low). */
    err = tsmon_hal_gpio_write(TSMON_SPI_CS_PIN, TSMON_CS_ASSERT);
    if (err != TSMON_ERR_SUCCESS) {
        return err;
    }

    /* Perform the 1-byte SPI transaction. */
    err = tsmon_hal_spi_transfer(tx_buf, rx_buf, 1);

    /* De-assert the chip select pin.
     * Note: de-assert is unconditional — we must release CS regardless of
     * whether the transaction succeeded, to keep the bus in a clean state. */
    tsmon_hal_gpio_write(TSMON_SPI_CS_PIN, TSMON_CS_DEASSERT);

    return err;
}


/* =========================================================================
 * Public API Implementation
 * ========================================================================= */

/**
 * @brief  Initialises the temperature sensor by reading its JEDEC ID.
 *
 * Sends an RDID (0x9F) command and reads back 3 bytes. The returned
 * Manufacturer ID and Device ID are validated against the expected values
 * from the sensor datasheet. A mismatch returns TSMON_ERR_UNKNOWN,
 * signalling that the device on the bus is not the expected sensor.
 */
tsmon_err_t tsmon_sensor_init(void) {
    uint8_t tx_buf[3] = {TSMON_CMD_RDID}; /* Command to read JEDEC ID. */
    uint8_t rx_buf[3] = {0};
    tsmon_err_t err;

    /* Assert chip select for the transaction. */
    err = tsmon_hal_gpio_write(TSMON_SPI_CS_PIN, TSMON_CS_ASSERT);
    if (err != TSMON_ERR_SUCCESS) {
        return err; /* Propagate GPIO error. */
    }

    /* Perform the 3-byte SPI transaction to read the ID.
     * tx_buf[0] = 0x9F (RDID command); bytes [1:2] are don't-care on TX.
     * rx_buf[0] = ignored (bus is being driven by master during command byte)
     * rx_buf[1] = Manufacturer ID returned by the sensor
     * rx_buf[2] = Device ID returned by the sensor */
    err = tsmon_hal_spi_transfer(tx_buf, rx_buf, 3);
    
    /* De-assert chip select. */
    tsmon_hal_gpio_write(TSMON_SPI_CS_PIN, TSMON_CS_DEASSERT);

    if (err != TSMON_ERR_SUCCESS) {
        return err; /* Propagate SPI error. */
    }

    /* Verify that the manufacturer and device IDs match the expected values
     * from the datasheet. If they don't match, we are talking to the wrong
     * device or it's faulty. */
    if (rx_buf[1] != TSMON_JEDEC_MFR_ID || rx_buf[2] != TSMON_JEDEC_DEV_ID) {
        /* If they don't match, we are talking to the wrong device or it's faulty. */
        return TSMON_ERR_UNKNOWN; 
    }

    return TSMON_ERR_SUCCESS; 
}

/**
 * @brief  Performs a complete temperature measurement and returns the result.
 *
 * Executes the mandatory 4-step wake-configure-read-sleep sequence. Each
 * step is checked for errors; a failure at any step causes an early return
 * with the relevant error code. The sensor may be left awake if SLEEP fails,
 * but the next call will re-run AWAKE and reconfigure, which is safe.
 */
tsmon_err_t tsmon_sensor_read(int16_t* temp_in_tenths_c) {
    /* Defensive check: ensure the output pointer is not null.
     * Without this guard a null pointer dereference at the result assignment
     * would cause undefined behaviour (segfault on most platforms). */
    if (temp_in_tenths_c == NULL) {
        return TSMON_ERR_INVALID_ARG; 
    }

    tsmon_err_t err;

    /* --- Sequence Step 1: Wake the sensor from sleep -------------------- */
    /* The AWAKE command (0xAB) transitions the sensor from its low-power
     * idle state to active mode. CONFIG and RDTEMP are ignored while the
     * sensor is sleeping, so this step is mandatory. */
    err = execute_command(TSMON_CMD_AWAKE);
    if (err != TSMON_ERR_SUCCESS) return err;

    /* --- Sequence Step 2: Configure the sensor for temperature measurement */
    /* The CONFIG command (0x06) writes the sensor's measurement mode register.
     * The 10-byte payload must include the validity marker (0xEE at byte [2])
     * and the temperature mode selector (0x03 at byte [4]).
     * All unused bytes are left as zero (memset guarantees this). */
    uint8_t config_tx_buf[10];
    uint8_t config_rx_buf[10];
    memset(config_tx_buf, 0, sizeof(config_tx_buf));

    config_tx_buf[0] = TSMON_CMD_CONFIG;       /* CONFIG command opcode */
    config_tx_buf[2] = TSMON_CFG_MARKER;      /* Validity sentinel 0xEE — sensor ignores payload if absent */
    config_tx_buf[4] = TSMON_CFG_MODE_TEMP; /* Set to temperature measurement mode. */

    err = tsmon_hal_gpio_write(TSMON_SPI_CS_PIN, TSMON_CS_ASSERT);
    if (err != TSMON_ERR_SUCCESS) return err;
    err = tsmon_hal_spi_transfer(config_tx_buf, config_rx_buf, sizeof(config_tx_buf));
    tsmon_hal_gpio_write(TSMON_SPI_CS_PIN, TSMON_CS_DEASSERT);
    if (err != TSMON_ERR_SUCCESS) return err;

    /* --- Sequence Step 3: Read the temperature -------------------------- */
    /* The RDTEMP command (0x03) requests the most recent conversion result.
     * A 4-byte transaction is required. The sensor returns a 16-bit signed
     * integer in big-endian order at bytes [2] and [3] of the response:
     *   temperature = ((int16_t)rx_buf[2] << 8) | rx_buf[3]
     * Units are tenths of degrees Celsius (e.g., 375 = 37.5 °C). */
    uint8_t temp_tx_buf[4] = {TSMON_CMD_RDTEMP}; /* RDTEMP command. */
    uint8_t temp_rx_buf[4] = {0};

    err = tsmon_hal_gpio_write(TSMON_SPI_CS_PIN, TSMON_CS_ASSERT);
    if (err != TSMON_ERR_SUCCESS) return err;
    err = tsmon_hal_spi_transfer(temp_tx_buf, temp_rx_buf, sizeof(temp_tx_buf));
    tsmon_hal_gpio_write(TSMON_SPI_CS_PIN, TSMON_CS_DEASSERT);
    if (err != TSMON_ERR_SUCCESS) return err;
    
    /* --- Sequence Step 4: Put the sensor back to sleep to save power ---- */
    /* Returning the sensor to sleep between measurements minimises standby
     * current consumption. This is particularly important in battery-powered
     * IoT nodes where the sensor may otherwise contribute several mA of idle
     * current between 1 Hz readings. */
    err = execute_command(TSMON_CMD_SLEEP);
    if (err != TSMON_ERR_SUCCESS) return err;

    /* --- Process the Result --------------------------------------------- */
    /* The temperature is a 16-bit signed integer, MSB first, in bytes 2 and 3
     * of the response. Combine the two bytes into a single int16_t value.
     * The explicit cast to int16_t ensures sign extension is handled correctly
     * on platforms where (uint8_t << 8) might produce an unsigned intermediate. */
    *temp_in_tenths_c = ((int16_t)temp_rx_buf[2] << 8) | temp_rx_buf[3];

    return TSMON_ERR_SUCCESS;
}
