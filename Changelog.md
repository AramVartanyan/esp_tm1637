# Changelog

All notable changes to this library are documented in this file.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the
versions follow [Semantic Versioning](https://semver.org/).

## [1.1.1] — tested on hardware

Tested on ESP32-S3 (ESP-IDF 5.5.4), 4-digit module driven as 3 digits, TM1637 at 3.3 V: answers with
the default 3 µs half bit time (1.1.0 with open-drain needed 40 µs on the same module), segment test,
unused 4th digit dark. A missing display does not block the application.

### Fixed
- CLK and DIO back to push-pull. With open-drain the rising edges depend on the pull-up and the
  capacitors of the module: a common module at 3.3 V (10 kΩ on board ∥ internal pull-up) needed a
  half bit time of 40 µs instead of 3 µs. Now the edges are fast again, as in 1.0.0.
- No drive conflict on DIO at the ACK: after a last bit 1 the line is released while CLK is still
  high (it stays high), after a last bit 0 only after CLK falls (both chips pull low) — the line
  never rises while CLK is high, which would be a STOP condition. ESP8266 / esp-open-rtos use the
  same sequence (before, DIO stayed driven for a few µs into the ACK).
- `tm1637_init()` clears all 6 display addresses: the TM1637 RAM is not cleared at power-on, so an
  unused position (e.g. the 4th digit of a 4-digit module driven as 3 digits) showed random segments.

## [1.1.0] — tested on hardware (open-drain timing issue, fixed in 1.1.1)

### Added
- Support for 1 to 6 digits (3-digit displays included); `TM1637_MAX_DIGITS`.
- `tm1637_print_symbols()` — raw segments for any number of digits.
- ACK timeout (`CONFIG_TM1637_ACK_TIMEOUT_US`) with an offline state: `tm1637_is_online()`,
  writes to an offline display return at once, retry every `CONFIG_TM1637_RETRY_MS`.
- `tm1637_delete()`; `tm1637_set_delay_us()` and `CONFIG_TM1637_BIT_DELAY_US`.
- `Kconfig` ("TM1637 display"); `ESP_TM1637_VERSION`.
- 3-digit temperature format: `24.2`, `-9.9`, `150`, `-12`.

### Changed
- Platform detection by `ESP_PLATFORM` (ESP-IDF on every ESP32 chip and ESP8266_RTOS_SDK);
  esp-open-rtos only when it is not defined. Before, only ESP32 and ESP8266 were recognised and
  every other chip (S2, S3, C3, C6, ...) fell into the esp-open-rtos branch and did not build.
- ESP-IDF 4.3+ / 5.x APIs: `esp_rom_delay_us()` from `esp_rom_sys.h`; the removed
  `GPIO_PIN_INTR_DISABLE` is no longer used. Older ESP-IDF keeps `ets_delay_us()`.
- CLK and DIO are open-drain on ESP-IDF (`GPIO_MODE_INPUT_OUTPUT_OD`): the MCU only pulls the
  lines low and never drives DIO high while the TM1637 acknowledges. ESP8266 and esp-open-rtos
  keep the previous direction switching.
- Each update is one write of all digits instead of one transaction per digit.
- `CMakeLists.txt` uses `idf_component_register()` (the legacy `register_component()` was removed
  in ESP-IDF 5).
- Leading zero of the temperature is blank (` 5.2` instead of `05.2`).
- Example: `portTICK_PERIOD_MS` instead of the removed `portTICK_RATE_MS`.

### Fixed
- `malloc()` result is checked; `tm1637_init()` returns NULL when out of memory.
- `tm1637_print_float()`: the digit buffer was one element too small.
- `tm1637_print_string()`: no longer reads past the end of a string shorter than the display.
- `tm1637_print_4_symbols()` on a 6-digit display no longer reads past its 4 arguments;
  `tm1637_set_segment_raw()` ignores indexes outside the display.
- Builds without warnings with ESP-IDF 5.5.1 for ESP32, ESP32-S3 and ESP32-C3.

## [1.0.0] — 2021

- Initial library: 4- and 6-digit modules, float, string, temperature, raw segments, brightness.
