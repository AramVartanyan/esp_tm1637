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
 */


#ifndef esp_tm1637_h
#define esp_tm1637_h

#include <stdio.h>
#include <stdarg.h>

typedef struct {
    uint8_t tm_clk;
    uint8_t tm_dio;
    uint8_t tm_bright;
    uint8_t tm_digits;
} tm1637_led_t;

/*
 * @brief Constructs new LED TM1637 object
 * @param pin_clk - GPIO pin for CLK input of LED module
 * @param pin_data - GPIO pin for DIO input of LED module
 * @param n_digits - Number of digit positions 4 or 6
 * @return
 */
tm1637_led_t * tm1637_init(uint8_t pin_clk, uint8_t pin_dio, uint8_t n_digits);

/*
 * @brief Set brightness level. Note - will be set after next display render
 * @param led LED object
 * @param level Brightness level 0..7 value
 */
void tm1637_set_brightness(tm1637_led_t * led, uint8_t level);

/*
 * @brief Set one-segment raw segment data
 * @param led LED object
 * @param segment_idx Segment index (0..3)
 * @param data Raw data, bitmask is XGFEDCBA
 */
void tm1637_set_segment_raw(tm1637_led_t * led, const uint8_t segment_idx, const uint8_t data);

/*
 * @brief Print temperature in format 23.2º or -9.9º (23.2ºC or -13.8ºC for 6 digits displays)
 * @param led LED object
 * @param t temperature in integer format multiplied by 10
 * in order to easily do calculation without loosing the decimals.
 * The possible range is between -999 and 999 (±99ºC)
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
 * @param string_data array - up to 4 or 6 word input
 */
void tm1637_print_string(tm1637_led_t * led, const char string_data[]);

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
