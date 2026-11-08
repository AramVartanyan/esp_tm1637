/*
 *  esp_tm1637.h
 *
 *
 *  Created by Aram Vartanyan on 6.05.21.
 *  based on:
 *  https://www.mcielectronics.cl/website_MCI/static/documents/Datasheet_TM1637.pdf
 *  https://github.com/petrows/esp-32-tm1637
 *  https://github.com/jasonacox/TM1637TinyDisplay
 *  https://github.com/avishorp/TM1637
 *
 *7 segment HEX map
 *
 *    AAA
 *  F     B
 *  F     B
 *    GGG
 *  E     C
 *  E     C
 *    DDD
 *
 * XGFEDCBA, DATA
 *
 * Wiring: SEG1..SEG8 = A..G, DP; GRID1 = leftmost digit (address 0).
 * CLK and DIO need pull-up resistors to the MCU logic level (e.g. 10 kOhm to 3.3 V);
 * the TM1637 itself may run from 5 V. On ESP-IDF the lines are driven open-drain.
 *
 * Thread safety: a display object must be used from one task only.
 */


#ifndef esp_tm1637_h
#define esp_tm1637_h

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>

#define ESP_TM1637_VERSION "1.1.0"

#define TM1637_MAX_DIGITS 6

typedef struct {
    uint8_t tm_clk;
    uint8_t tm_dio;
    uint8_t tm_bright;
    uint8_t tm_digits;      /* 1..6 */
    uint8_t tm_delay_us;    /* half bit time */
    bool    tm_online;      /* false after an ACK timeout */
    uint32_t tm_retry_tick; /* while offline: next attempt (FreeRTOS tick) */
} tm1637_led_t;

/*
 * @brief Constructs new LED TM1637 object
 * @param pin_clk - GPIO pin for CLK input of LED module
 * @param pin_data - GPIO pin for DIO input of LED module
 * @param n_digits - Number of digit positions, 1..6 (6-digit modules use the
 *                   usual 2-1-0-5-4-3 wiring)
 * @return the object, or NULL when out of memory
 */
tm1637_led_t * tm1637_init(uint8_t pin_clk, uint8_t pin_dio, uint8_t n_digits);

/*
 * @brief Releases the object created by tm1637_init() (the GPIOs are left as they are)
 */
void tm1637_delete(tm1637_led_t * led);

/*
 * @brief false while the display does not answer (no ACK). Writes to an offline display
 * return at once; a new attempt is made every TM1637_RETRY_MS (default 5 s), so a missing
 * or broken display never blocks the caller.
 */
bool tm1637_is_online(const tm1637_led_t * led);

/*
 * @brief Half bit time in microseconds (default CONFIG_TM1637_BIT_DELAY_US, 3 us).
 * Increase it (e.g. 10 - 20 us) for weak pull-ups or long wires.
 */
void tm1637_set_delay_us(tm1637_led_t * led, uint8_t us);

/*
 * @brief Set brightness level. Note - will be set after next display render
 * @param led LED object
 * @param level Brightness level 0..7 value
 */
void tm1637_set_brightness(tm1637_led_t * led, uint8_t level);

/*
 * @brief Set one-segment raw segment data
 * @param led LED object
 * @param segment_idx Digit index (0 .. n_digits-1, 0 = leftmost)
 * @param data Raw data, bitmask is XGFEDCBA
 */
void tm1637_set_segment_raw(tm1637_led_t * led, const uint8_t segment_idx, const uint8_t data);

/*
 * @brief Print temperature given in tenths of a degree (e.g. 242 = 24.2)
 * 6 digits: -23.2ºC; 4 digits: 23.2º / -9.9º; 3 digits: 23.2 / -9.9 / 150 / -12;
 * 1-2 digits: whole degrees.
 * @param led LED object
 * @param t temperature in integer format multiplied by 10.
 * The possible range is between -999 and 999 (±99.9ºC), with 3 digits -99..999 (-99..999ºC).
 */
void tm1637_print_temp(tm1637_led_t * led, int t);

/*
 * @brief Print float number with decimal point and automatically adjusted precision
 * @param led LED object
 * @param float value
 * For 4 digits display, the possible range is between -999 and 9999.
 * The precision varies dependig by the number size up to 0.000
 */
void tm1637_print_float(tm1637_led_t * led, float f_value);

/*
 * @brief Print string of characters based on ASCII code
 * @param led LED object
 * @param string_data - up to n_digits characters; a shorter string is padded with blanks
 */
void tm1637_print_string(tm1637_led_t * led, const char string_data[]);

/*
 * @brief Print raw symbols (XGFEDCBA) to the first `count` digits; missing digits are blank
 * @param led LED object
 * @param seg array of segment data, left to right
 * @param count number of elements in `seg`
 */
void tm1637_print_symbols(tm1637_led_t * led, const uint8_t *seg, uint8_t count);

/*
 * @brief Print symbols based on its byte hexadecimal code
 * @param led LED object
 * @param d1, d2 ... - HEX code of the 7-segment symbols (size up to 4 elements)
 */
void tm1637_print_4_symbols(tm1637_led_t * led, uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4);

/*
 * @brief Print symbols based on its byte hexadecimal code
 * @param led LED object
 * @param d1, d2 ... - HEX code of the 7-segment symbols (size up to 6 elements)
 */
void tm1637_print_6_symbols(tm1637_led_t * led, uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4, uint8_t d5, uint8_t d6);

/*
 * @brief Lights up all segments for test purpose or initialisation
 * @param led LED object
 */
void tm1637_test_segments(tm1637_led_t * led);

/*
 * @brief Clear display
 * @param led LED object
 */
void tm1637_clear(tm1637_led_t * led);


#endif /* esp_tm1637_h */
