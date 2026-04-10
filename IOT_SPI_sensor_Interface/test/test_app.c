/*
 * test_app.c
 *
 * Unit test suite for the TSMON IoT Temperature Sensor application.
 *
 * Overview
 * --------
 * This file contains 15 white-box unit tests that exercise the three main
 * behavioural domains of the application:
 *
 *   Initialisation (tests 01–03)
 *     Verify that tsmon_app_init() accepts a correctly identified sensor,
 *     rejects a device with a wrong ID, and propagates SPI bus errors.
 *
 *   Hysteresis logic (tests 04–08)
 *     Verify the two-threshold alert comparator: alert activates above
 *     37.5 °C (375), deactivates below 36.5 °C (365), and stays in its
 *     current state when temperature is inside the hysteresis band [365, 375].
 *
 *   Median filter (tests 09–11)
 *     Verify that single-sample noise spikes are rejected, sustained
 *     temperature changes are tracked, and the filter's window behaviour
 *     is correct.
 *
 *   Error handling (tests 12–13)
 *     Verify that SPI failures are handled gracefully (fail-safe GPIO LOW)
 *     and that the driver correctly rejects a NULL output pointer.
 *
 *   Integration / regression (tests 14–15)
 *     Verify multi-cycle GPIO toggle behaviour and exact boundary conditions.
 *
 * Test infrastructure
 * -------------------
 * Each test follows the Arrange-Act-Assert pattern:
 *   - Arrange: call tsmon_app_init() to reset state; use test helpers to
 *              inject the simulated temperature or fault condition.
 *   - Act:     call tsmon_app_tick() the required number of times via run_ticks().
 *   - Assert:  use assert() to verify the expected GPIO state or return code.
 *
 * All assertions use the C standard library assert() macro. A failure halts
 * execution immediately and prints the file, line, and expression that failed.
 *
 * Build requirement
 * -----------------
 * This file must be compiled with -DUNIT_TESTING to enable the test helper
 * functions in tsmon_hal.c. The Makefile's 'make test' target handles
 * this automatically.
 */

#include <stdio.h>
#include <assert.h>
#include "tsmon_app.h"
#include "tsmon_hal.h"
#include "test_app.h"
#include "tsmon_errors.h"
#include "temp_sensor.h"

/* =========================================================================
 * Test Utility
 * ========================================================================= */

/**
 * @brief  Execute count application ticks, asserting each one succeeds.
 *
 * Used by most tests to advance the application state by a known number of
 * steps. Asserts that every tick returns TSMON_ERR_SUCCESS — if a tick
 * fails unexpectedly, the assertion surfaces the failure immediately rather
 * than letting it silently affect subsequent assertions.
 *
 * @param  count  Number of ticks to execute.
 */
static void run_ticks(int count) {
    for (int i = 0; i < count; i++) {
        assert(tsmon_app_tick() == TSMON_ERR_SUCCESS);
    }
}

/* =========================================================================
 * Initialisation Tests
 * =========================================================================
 * These tests verify the startup path: sensor identification over SPI,
 * error propagation on ID mismatch, and error propagation on SPI bus fault.
 * ========================================================================= */

/**
 * @brief  Test 01 — Successful initialisation with correct sensor ID.
 *
 * Verifies that tsmon_app_init() returns TSMON_ERR_SUCCESS when the
 * simulated HAL returns the expected JEDEC Manufacturer ID (0x36) and
 * Device ID (0xC9) in response to the RDID command.
 */
void test_01_init_succeeds() {
    printf("Test 01: App initialization succeeds with correct sensor ID...");
    assert(tsmon_app_init() == TSMON_ERR_SUCCESS);
    printf("PASSED\n");
}

/**
 * @brief  Test 02 — Initialisation fails when Device ID is wrong.
 *
 * Injects an incorrect Device ID (0xFF) via the test helper to simulate a
 * mis-wired sensor or a wrong device being present on the SPI bus. Verifies
 * that tsmon_app_init() returns TSMON_ERR_UNKNOWN. Restores the correct
 * Device ID after the test to prevent cross-test contamination.
 */
void test_02_init_fails_on_wrong_device_id() {
    printf("Test 02: App initialization fails on wrong device ID...");
    tsmon_test_inject_jedec_id(TSMON_JEDEC_MFR_ID, 0xFF);
    assert(tsmon_app_init() == TSMON_ERR_UNKNOWN);
    tsmon_test_inject_jedec_id(TSMON_JEDEC_MFR_ID, TSMON_JEDEC_DEV_ID);
    printf("PASSED\n");
}

/**
 * @brief  Test 03 — Initialisation fails when the SPI bus returns an error.
 *
 * Arms the one-shot fault injector so that the first SPI transaction (RDID)
 * returns TSMON_ERR_UNKNOWN. Verifies that tsmon_app_init() propagates this
 * error rather than proceeding with unverified sensor state. Disarms the
 * injector after the test as a safety measure.
 */
void test_03_init_fails_on_spi_error() {
    printf("Test 03: App initialization fails on SPI error...");
    tsmon_test_inject_spi_fault(true);
    assert(tsmon_app_init() != TSMON_ERR_SUCCESS);
    tsmon_test_inject_spi_fault(false);
    printf("PASSED\n");
}

/* =========================================================================
 * Hysteresis Logic Tests
 * =========================================================================
 * These tests verify the two-threshold alert comparator in tsmon_app_tick().
 * All tests run 10 ticks — enough to fully prime the 5-sample median filter
 * and ensure the hysteresis logic is operating on filtered (not raw) values.
 * ========================================================================= */

/**
 * @brief  Test 04 — Alert GPIO remains LOW for temperatures below the threshold.
 *
 * Simulates a steady temperature of 30.0 °C (300), well below the 37.5 °C
 * upper threshold. Verifies that GPIO 4 stays LOW throughout.
 */
void test_04_gpio_remains_low_below_threshold() {
    printf("Test 04: GPIO alert remains LOW for normal temperatures...");
    tsmon_app_init();
    tsmon_test_inject_temp(300);
    run_ticks(10);
    assert(tsmon_test_gpio_read(TSMON_ALERT_GPIO_PIN) == false);
    printf("PASSED\n");
}

/**
 * @brief  Test 05 — Alert GPIO goes HIGH when temperature exceeds the threshold.
 *
 * Simulates 37.6 °C (376), which is strictly greater than the 37.5 °C (375)
 * upper threshold. After 10 ticks the median filter will have converged on
 * 376, and the alert must be asserted (GPIO 4 HIGH).
 */
void test_05_gpio_goes_high_above_threshold() {
    printf("Test 05: GPIO alert goes HIGH when temperature crosses threshold...");
    tsmon_app_init();
    tsmon_test_inject_temp(376);
    run_ticks(10);
    assert(tsmon_test_gpio_read(TSMON_ALERT_GPIO_PIN) == true);
    printf("PASSED\n");
}

/**
 * @brief  Test 06 — Alert stays HIGH when temperature falls into hysteresis zone.
 *
 * Starts with alert active (37.6 °C for 10 ticks), then drops to 37.0 °C
 * (370) which is inside the hysteresis band [365, 375]. Verifies the alert
 * remains HIGH — the lower de-activation threshold (36.5 °C / 365) has not
 * been crossed, so the comparator must not de-assert.
 */
void test_06_gpio_stays_high_in_hysteresis_zone() {
    printf("Test 06: GPIO alert stays HIGH when temperature is in hysteresis zone...");
    tsmon_app_init();
    tsmon_test_inject_temp(376);
    run_ticks(10);
    assert(tsmon_test_gpio_read(TSMON_ALERT_GPIO_PIN) == true);
    tsmon_test_inject_temp(370);
    run_ticks(10);
    assert(tsmon_test_gpio_read(TSMON_ALERT_GPIO_PIN) == true);
    printf("PASSED\n");
}

/**
 * @brief  Test 07 — Alert goes LOW when temperature drops below the hysteresis point.
 *
 * Starts with alert active (37.6 °C), then drops to 36.4 °C (364) which is
 * strictly below the lower de-activation threshold (36.5 °C / 365). After
 * the filter converges, the alert must be de-asserted (GPIO 4 LOW).
 */
void test_07_gpio_goes_low_after_hysteresis() {
    printf("Test 07: GPIO alert goes LOW when temperature drops below hysteresis point...");
    tsmon_app_init();
    tsmon_test_inject_temp(376);
    run_ticks(10);
    tsmon_test_inject_temp(364);
    run_ticks(10);
    assert(tsmon_test_gpio_read(TSMON_ALERT_GPIO_PIN) == false);
    printf("PASSED\n");
}

/**
 * @brief  Test 08 — Alert stays LOW when temperature rises into hysteresis zone.
 *
 * Starts with alert inactive (30.0 °C), then rises to 37.0 °C (370) which
 * is inside the hysteresis band but below the upper activation threshold
 * (37.5 °C / 375). Verifies the alert remains LOW — the rising edge has not
 * crossed the activation threshold.
 */
void test_08_gpio_stays_low_in_hysteresis_zone_from_low() {
    printf("Test 08: GPIO alert stays LOW when rising into hysteresis zone...");
    tsmon_app_init();
    tsmon_test_inject_temp(300);
    run_ticks(10);
    assert(tsmon_test_gpio_read(TSMON_ALERT_GPIO_PIN) == false);
    tsmon_test_inject_temp(370);
    run_ticks(10);
    assert(tsmon_test_gpio_read(TSMON_ALERT_GPIO_PIN) == false);
    printf("PASSED\n");
}

/* =========================================================================
 * Signal Filter Tests
 * =========================================================================
 * These tests verify the 5-sample sliding window median filter's ability to
 * reject transient spike noise while tracking genuine sustained changes.
 * ========================================================================= */

/**
 * @brief  Test 09 — Median filter rejects a single high noise spike.
 *
 * Primes the filter with 10 ticks of 30.0 °C (below threshold), injects one
 * tick of 50.0 °C (well above threshold), then returns to 30.0 °C for 10
 * more ticks. The single outlier at index 2 of the sorted window should
 * remain the minority; the median should stay near 30.0 °C and the alert
 * must not activate.
 */
void test_09_filter_rejects_high_spike() {
    printf("Test 09: Signal filter rejects a single high noise spike...");
    tsmon_app_init();
    tsmon_test_inject_temp(300);
    run_ticks(10);
    tsmon_test_inject_temp(500);
    tsmon_app_tick();
    tsmon_test_inject_temp(300);
    run_ticks(10);
    assert(tsmon_test_gpio_read(TSMON_ALERT_GPIO_PIN) == false);
    printf("PASSED\n");
}

/**
 * @brief  Test 10 — Median filter rejects a single low noise spike.
 *
 * Symmetric to test 09: starts with alert active (40.0 °C for 10 ticks),
 * injects one tick of 0 °C, then returns to 40.0 °C. The single low outlier
 * must not de-activate the alert; the median of the window remains above
 * the lower hysteresis threshold.
 */
void test_10_filter_rejects_low_spike() {
    printf("Test 10: Signal filter rejects a single low noise spike...");
    tsmon_app_init();
    tsmon_test_inject_temp(400);
    run_ticks(10);
    assert(tsmon_test_gpio_read(TSMON_ALERT_GPIO_PIN) == true);
    tsmon_test_inject_temp(0);
    tsmon_app_tick();
    tsmon_test_inject_temp(400);
    run_ticks(10);
    assert(tsmon_test_gpio_read(TSMON_ALERT_GPIO_PIN) == true);
    printf("PASSED\n");
}

/**
 * @brief  Test 11 — Median filter responds correctly to a sustained temperature change.
 *
 * Fills the buffer with 30.0 °C values (5 ticks), then injects 37.6 °C for
 * 3 more ticks. After 3 ticks the 5-element window contains [300, 300, 376,
 * 376, 376] — sorted median = 376, which exceeds the 375 threshold. Verifies
 * that a sustained genuine change causes the alert to activate.
 */
void test_11_filter_responds_to_sustained_change() {
    printf("Test 11: Signal filter responds to a sustained temperature change...");
    tsmon_app_init();
    tsmon_test_inject_temp(300);
    run_ticks(5); /* Fill buffer */
    tsmon_test_inject_temp(376);
    run_ticks(3); /* Should be enough for median to cross threshold */
    assert(tsmon_test_gpio_read(TSMON_ALERT_GPIO_PIN) == true);
    printf("PASSED\n");
}

/* =========================================================================
 * Error Handling Tests
 * =========================================================================
 * These tests verify that the application and driver handle fault conditions
 * safely without undefined behaviour or latched incorrect GPIO state.
 * ========================================================================= */

/**
 * @brief  Test 12 — Application handles mid-tick SPI failure gracefully.
 *
 * Initialises the application then arms the one-shot SPI fault injector
 * before calling tsmon_app_tick(). Verifies that:
 *   1. tsmon_app_tick() returns a non-SUCCESS error code.
 *   2. GPIO 4 is forced LOW (fail-safe) rather than staying at whatever
 *      state it was in before the fault.
 * Disarms the injector after the test.
 */
void test_12_app_handles_spi_failure_gracefully() {
    printf("Test 12: App handles SPI transaction failure gracefully...");
    tsmon_app_init();
    tsmon_test_inject_spi_fault(true);
    assert(tsmon_app_tick() != TSMON_ERR_SUCCESS);
    assert(tsmon_test_gpio_read(TSMON_ALERT_GPIO_PIN) == false);
    tsmon_test_inject_spi_fault(false);
    printf("PASSED\n");
}

/**
 * @brief  Test 13 — Driver rejects a NULL pointer for the temperature output.
 *
 * Directly calls tsmon_sensor_read(NULL) to verify that the driver
 * performs the defensive null-pointer check and returns
 * TSMON_ERR_INVALID_ARG without attempting to dereference the pointer
 * (which would cause a segfault on most platforms).
 */
void test_13_driver_handles_null_pointer() {
    printf("Test 13: Driver rejects NULL pointer for temperature output...");
    assert(tsmon_sensor_read(NULL) == TSMON_ERR_INVALID_ARG);
    printf("PASSED\n");
}

/* =========================================================================
 * Integration / Regression Tests
 * =========================================================================
 * These tests exercise multi-cycle behaviour and exact boundary conditions
 * to guard against regressions in the hysteresis state machine.
 * ========================================================================= */

/**
 * @brief  Test 14 — GPIO toggles correctly through a complete high-low-high cycle.
 *
 * Verifies that the hysteresis state machine correctly resets and re-triggers
 * after a full de-activation cycle. Runs three phases:
 *   Phase 1 (40.0 °C, 10 ticks): alert activates — GPIO 4 HIGH.
 *   Phase 2 (30.0 °C, 10 ticks): alert deactivates — GPIO 4 LOW.
 *   Phase 3 (40.0 °C, 10 ticks): alert re-activates — GPIO 4 HIGH.
 * If the comparator latched or lost its state between phases, one of the
 * assertions would fail.
 */
void test_14_gpio_toggles_correctly() {
    printf("Test 14: GPIO toggles correctly multiple times...");
    tsmon_app_init();
    /* Go high */
    tsmon_test_inject_temp(400);
    run_ticks(10);
    assert(tsmon_test_gpio_read(TSMON_ALERT_GPIO_PIN) == true);
    /* Go low */
    tsmon_test_inject_temp(300);
    run_ticks(10);
    assert(tsmon_test_gpio_read(TSMON_ALERT_GPIO_PIN) == false);
    /* Go high again */
    tsmon_test_inject_temp(400);
    run_ticks(10);
    assert(tsmon_test_gpio_read(TSMON_ALERT_GPIO_PIN) == true);
    printf("PASSED\n");
}

/**
 * @brief  Test 15 — GPIO does not trigger at exactly the upper threshold (37.5 °C).
 *
 * The activation condition is (filtered_temp > 375), i.e., strictly greater
 * than. A sustained reading of exactly 375 (37.5 °C) must NOT activate the
 * alert. Verifies that the boundary is exclusive, not inclusive.
 */
void test_15_gpio_at_upper_boundary() {
    printf("Test 15: GPIO does not trigger exactly at 37.5C...");
    tsmon_app_init();
    tsmon_test_inject_temp(375);
    run_ticks(10);
    assert(tsmon_test_gpio_read(TSMON_ALERT_GPIO_PIN) == false);
    printf("PASSED\n");
}

/* =========================================================================
 * Test Runner
 * ========================================================================= */

/**
 * @brief  Entry point for the test binary.
 *
 * Calls each test function in sequence. Because assert() aborts on the first
 * failure, the output will show all "PASSED" messages for tests that ran
 * before the failure and then stop at the failing test. All 15 passing prints
 * and the final summary line indicate a completely clean test run.
 */
int main(void) {
    printf("--- Running All Unit Tests ---\n");
    test_01_init_succeeds();
    test_02_init_fails_on_wrong_device_id();
    test_03_init_fails_on_spi_error();
    test_04_gpio_remains_low_below_threshold();
    test_05_gpio_goes_high_above_threshold();
    test_06_gpio_stays_high_in_hysteresis_zone();
    test_07_gpio_goes_low_after_hysteresis();
    test_08_gpio_stays_low_in_hysteresis_zone_from_low();
    test_09_filter_rejects_high_spike();
    test_10_filter_rejects_low_spike();
    test_11_filter_responds_to_sustained_change();
    test_12_app_handles_spi_failure_gracefully();
    test_13_driver_handles_null_pointer();
    test_14_gpio_toggles_correctly();
    test_15_gpio_at_upper_boundary();
    printf("--- All 15 tests passed ---\n");
    return 0;
}
