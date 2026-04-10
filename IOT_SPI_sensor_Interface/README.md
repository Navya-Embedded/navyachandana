# IoT Temperature Sensor Monitor (`iot_temp_sensor_monitor`)

A fully host-testable embedded C firmware project that reads a SPI temperature
sensor, applies a **5-sample median filter** to reject noise, drives a
**hysteresis-based GPIO alert**, and ships with a **15-test unit test suite**
— all runnable on a development PC with no physical hardware required.

---

## Features

| Feature | Detail |
|---|---|
| **SPI driver** | Full JEDEC command-set implementation (RDID, AWAKE, CONFIG, RDTEMP, SLEEP) |
| **Median filter** | 5-sample sliding window — rejects single-sample spike noise |
| **Hysteresis alert** | Activate >37.5 °C · Deactivate <36.5 °C — prevents GPIO chatter |
| **HAL simulation** | Software-only peripheral emulation — build and test without hardware |
| **Unit tests** | 15 tests covering init, filter, hysteresis, error handling, and boundaries |
| **Portable** | Compiles on Linux, macOS, and Windows (MinGW); C11, no external libs |

---

## Project Structure

```
iot_temp_sensor_monitor/
├── src/
│   ├── main.c              # Entry point — 1 Hz tick loop
│   ├── tsmon_app.c/.h      # Application layer: filter + hysteresis logic
│   ├── tsmon_errors.h      # Centralised error code definitions
│   ├── tsmon_hal.c/.h      # Hardware abstraction layer (simulated peripherals)
│   └── drivers/
│       └── temp_sensor.c/.h  # SPI temperature sensor driver
├── test/
│   ├── test_app.c          # 15 unit tests (Arrange-Act-Assert pattern)
│   └── test_app.h          # Test helper function declarations
└── Makefile
```

---

## Quick Start

**Requirements:** GCC, GNU Make

```bash
# Build and run the main application (10 simulated 1 Hz ticks)
make all
./tsmon_firmware

# Build and run all 15 unit tests
make test

# Clean build artefacts
make clean
```

Expected test output:
```
--- Running All Unit Tests ---
Test 01: App initialization succeeds with correct sensor ID...PASSED
...
--- All 15 tests passed ---
```

---

## Architecture

```
┌─────────────────────────────────────┐
│         Application Layer           │  tsmon_app.c
│  1 Hz tick · median filter · alert  │
└────────────────┬────────────────────┘
                 │
┌────────────────▼────────────────────┐
│          Sensor Driver              │  temp_sensor.c
│  JEDEC ID check · SPI sequences     │
└────────────────┬────────────────────┘
                 │
┌────────────────▼────────────────────┐
│   Hardware Abstraction Layer (HAL)  │  tsmon_hal.c
│  GPIO simulation · SPI simulation   │
└─────────────────────────────────────┘
```

The HAL layer is the **only file that changes** when deploying to real
hardware. Replace `tsmon_hal.c` with your MCU's BSP implementation of
`tsmon_hal_gpio_write()` and `tsmon_hal_spi_transfer()` — everything above
compiles and runs identically.

---

## API Summary

### Application Layer (`tsmon_app.h`)

| Function | Description |
|---|---|
| `tsmon_app_init()` | One-time init; validates sensor JEDEC ID over SPI |
| `tsmon_app_tick()` | Call at 1 Hz; reads sensor, filters, evaluates alert |
| `tsmon_app_deinit()` | Graceful shutdown (reserved for resource cleanup) |
| `tsmon_app_get_telemetry(raw, filtered, gpio)` | Read latest telemetry snapshot |

### Sensor Driver (`temp_sensor.h`)

| Function | Description |
|---|---|
| `tsmon_sensor_init()` | RDID command; validates Mfr ID 0x36, Dev ID 0xC9 |
| `tsmon_sensor_read(int16_t*)` | 4-step sequence: AWAKE → CONFIG → RDTEMP → SLEEP |

### Temperature Encoding

All temperature values are **signed 16-bit integers in tenths of °C**.

```
375  = 37.5 °C   (upper alert threshold — exclusive)
365  = 36.5 °C   (lower hysteresis threshold — exclusive)
300  = 30.0 °C
-100 = -10.0 °C
```

---

## Unit Test Coverage

| Category | Tests | What is verified |
|---|---|---|
| Initialisation | 01–03 | Correct ID pass, wrong ID fail, SPI fault fail |
| Hysteresis | 04–08 | Both threshold directions + band stay-put cases |
| Signal filter | 09–11 | Spike rejection (high + low) + sustained change tracking |
| Error handling | 12–13 | Fail-safe GPIO LOW on SPI error; NULL pointer rejection |
| Integration | 14–15 | Full toggle cycle; exclusive upper boundary |

---

## Porting to Real Hardware

1. Replace `tsmon_hal.c` with your MCU's SPI/GPIO driver.
2. Implement the two function signatures from `tsmon_hal.h`:
   - `tsmon_hal_gpio_write(pin, value)` → drive a GPIO output pin
   - `tsmon_hal_spi_transfer(tx, rx, len)` → full-duplex SPI exchange
3. Do **not** define `UNIT_TESTING` in your production build.
4. Call `tsmon_app_init()` at startup and `tsmon_app_tick()` from your 1 Hz timer ISR.

---

## License

MIT — see [LICENSE](LICENSE)
