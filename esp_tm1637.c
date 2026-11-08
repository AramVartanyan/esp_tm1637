/*
 *  esp_tm1637.c
 *
 *
 *  Created by Aram Vartanyan on 6.05.21.
 *  based on:
 *  https://www.mcielectronics.cl/website_MCI/static/documents/Datasheet_TM1637.pdf
 *  https://github.com/petrows/esp-32-tm1637
 *  https://github.com/jasonacox/TM1637TinyDisplay
 *  https://github.com/avishorp/TM1637
 *
 */

#include "esp_tm1637.h"

#include "stdio.h"
#include "stdlib.h"
#include "stdbool.h"
#include "string.h"
#include "math.h"

/* ------------------------------------------------------------- platform */

#ifdef ESP_PLATFORM
/* ESP-IDF (all ESP32 chips) and ESP8266_RTOS_SDK: both define ESP_PLATFORM */
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#if CONFIG_IDF_TARGET_ESP8266
#include "rom/ets_sys.h"
#define TM_DELAY_US(us) ets_delay_us(us)
#else
#include "esp_idf_version.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0)
#include "esp_rom_sys.h"
#define TM_DELAY_US(us) esp_rom_delay_us(us)
#else
#include "esp32/rom/ets_sys.h"
#define TM_DELAY_US(us) ets_delay_us(us)
#endif
#endif

#else
/* esp-open-rtos */
#include "FreeRTOS.h"
#include "task.h"
#include "esp/gpio.h"
#include "espressif/esp_misc.h"
#define TM_DELAY_US(us) sdk_os_delay_us(us)
#endif

#ifndef CONFIG_TM1637_BIT_DELAY_US
#define CONFIG_TM1637_BIT_DELAY_US  3
#endif
#ifndef CONFIG_TM1637_ACK_TIMEOUT_US
#define CONFIG_TM1637_ACK_TIMEOUT_US 100
#endif
#ifndef CONFIG_TM1637_RETRY_MS
#define CONFIG_TM1637_RETRY_MS      5000
#endif

#define TM1637_COMM1 0x40  //CmdSetData       0b01000000
#define TM1637_COMM2 0xC0  //CmdSetAddress    0b11000000
#define TM1637_COMM3 0x44  //CmdFixedAddress  0b01000100
#define TM1637_COMM4 0x80  //CmdDisplay       0b10000000
#define TM1637_LBRTN 0x88  //lower brightness 0b10001000

//Two possible operative mode (command 0x40 or 0x44):
//0x40 - with increasing car destination address: each data will be written progressively in successive addresses starting from the selected one
//0x44 - normal operation mode. Fixed destination address: data are sent to the selected address only (also write command setting)
//0xC0 - address command setting
//0x80 - display and control command setting

#define TM_MINUS 0x40
#define TM_DP    0x80
#define TM_DEG   0x63
#define TM_C     0x39

static const uint8_t tm_num[] = {
    // XGFEDCBA
    0x3f, // 0b00111111,    // 0
    0x06, // 0b00000110,    // 1
    0x5b, // 0b01011011,    // 2
    0x4f, // 0b01001111,    // 3
    0x66, // 0b01100110,    // 4
    0x6d, // 0b01101101,    // 5
    0x7d, // 0b01111101,    // 6
    0x07, // 0b00000111,    // 7
    0x7f, // 0b01111111,    // 8
    0x6f, // 0b01101111,    // 9
    0x77, // 0b01110111,    // A
    0x7c, // 0b01111100,    // b
    0x39, // 0b00111001,    // C
    0x5e, // 0b01011110,    // d
    0x79, // 0b01111001,    // E
    0x71, // 0b01110001     // F
};

static const uint8_t tm_ascii[] = {
    0x00, // 032 (Space)
    0x30, // 033 !
    0x22, // 034 "
    0x41, // 035 #
    0x6D, // 036 $
    0x52, // 037 %
    0x7C, // 038 &
    0x20, // 039 '
    0x39, // 040 (
    0x0F, // 041 )
    0x21, // 042 *
    0x70, // 043 +
    0x08, // 044 ,
    0x40, // 045 -
    0x80, // 046 .
    0x52, // 047 /
    0x3F, // 048 0
    0x06, // 049 1
    0x5B, // 050 2
    0x4F, // 051 3
    0x66, // 052 4
    0x6D, // 053 5
    0x7D, // 054 6
    0x07, // 055 7
    0x7F, // 056 8
    0x6F, // 057 9
    0x48, // 058 :
    0x48, // 059 ;
    0x39, // 060 <
    0x48, // 061 =
    0x0F, // 062 >
    0x53, // 063 ?
    0x5F, // 064 @
    0x77, // 065 A
    0x7C, // 066 B
    0x39, // 067 C
    0x5E, // 068 D
    0x79, // 069 E
    0x71, // 070 F
    0x3D, // 071 G
    0x76, // 072 H
    0x30, // 073 I
    0x1E, // 074 J
    0x7A, // 075 K
    0x38, // 076 L
    0x55, // 077 M
    0x54, // 078 N
    0x5C, // 079 O
    0x73, // 080 P
    0x67, // 081 Q
    0x50, // 082 R
    0x6D, // 083 S
    0x78, // 084 T
    0x3E, // 085 U
    0x7E, // 086 V
    0x6A, // 087 W
    0x36, // 088 X
    0x6E, // 089 Y
    0x48, // 090 Z
    0x39, // 091 [
    0x64, // 092 (backslash)
    0x0F, // 093 ]
    0x23, // 094 ^
    0x08, // 095 _
    0x20, // 096 `
    0x77, // 097 a
    0x7C, // 098 b
    0x58, // 099 c
    0x5E, // 100 d
    0x79, // 101 e
    0x71, // 102 f
    0x3D, // 103 g
    0x74, // 104 h
    0x10, // 105 i
    0x1E, // 106 j
    0x7A, // 107 k
    0x18, // 108 l
    0x55, // 109 m
    0x54, // 110 n
    0x5C, // 111 o
    0x73, // 112 p
    0x67, // 113 q
    0x50, // 114 r
    0x6D, // 115 s
    0x78, // 116 t
    0x1C, // 117 u
    0x1C, // 118 v
    0x6A, // 119 w
    0x36, // 120 x
    0x6E, // 121 y
    0x48, // 122 z
    0x39, // 123 {
    0x30, // 124 |
    0x0F, // 125 }
    0x40, // 126 ~
    0x00  // 127 
};


/* ------------------------------------------------------------- line level */

static void tm_delay(const tm1637_led_t * led) {
    TM_DELAY_US(led->tm_delay_us);
}

static void pin_level(uint8_t gpio, uint8_t lvl) {
#ifdef ESP_PLATFORM
    gpio_set_level((gpio_num_t)gpio, lvl);
#else
    gpio_write(gpio, lvl);
#endif
}

static uint8_t pin_read(uint8_t gpio) {
#ifdef ESP_PLATFORM
    return gpio_get_level((gpio_num_t)gpio);
#else
    return gpio_read(gpio);
#endif
}

static void pin_dir(uint8_t gpio, bool output) {
#ifdef ESP_PLATFORM
    gpio_set_direction((gpio_num_t)gpio, output ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT);
#else
    gpio_enable(gpio, output ? GPIO_OUTPUT : GPIO_INPUT);
#endif
}

/* CLK and DIO are driven push-pull (fast edges, whatever the capacitors on the module).
 * Only the MCU drives CLK. DIO is switched to input for the ACK *before* the falling edge
 * of the 8th clock, when the TM1637 starts pulling it low, and driven again after the 9th
 * clock, so the two chips never drive DIO against each other. */
static void tm_clk(const tm1637_led_t * led, uint8_t lvl) { pin_level(led->tm_clk, lvl); }
static void tm_dio(const tm1637_led_t * led, uint8_t lvl) { pin_level(led->tm_dio, lvl); }

static uint32_t tm_now_tick(void) {
    return (uint32_t)xTaskGetTickCount();
}

/* ------------------------------------------------------------- protocol */

static void tm_start(const tm1637_led_t * led) {
    // Send start signal
    // Both outputs are expected to be HIGH beforehand
    tm_dio(led, 0);
    tm_delay(led);
}

static void tm_stop(const tm1637_led_t * led) {
    // Send stop signal
    // CLK is expected to be LOW beforehand
    tm_dio(led, 0);
    tm_delay(led);
    tm_clk(led, 1);
    tm_delay(led);
    tm_dio(led, 1);
    tm_delay(led);
}

/* Returns false when the TM1637 does not acknowledge the byte (missing/broken display). */
static bool tm_send_byte(const tm1637_led_t * led, uint8_t byte) {
    const bool last_bit = byte & 0x80;      /* the 8th (last) bit, LSB first */
    for (uint8_t i = 0; i < 8; ++i) {
        tm_clk(led, 0);
        tm_delay(led);
        tm_dio(led, byte & 0x01); // Send current bit, LSB first
        byte >>= 1;
        tm_delay(led);
        tm_clk(led, 1);
        tm_delay(led);
    }

    /*
     The TM1637 signals an ACK by pulling DIO low from the falling edge of CLK after sending
     the 8th bit, to the next falling edge of CLK. The MCU must stop driving DIO by then, but
     DIO must not rise while CLK is high (that would be a STOP condition):
     - last bit 1: DIO is high; it is released while CLK is still high (the pull-up keeps it
       high), so the MCU never drives high against the TM1637 pulling low;
     - last bit 0: DIO is low and the TM1637 also pulls low, so it is released only after CLK
       falls. Either way both chips never drive DIO to different levels.
     */
    if (last_bit) {
        pin_dir(led->tm_dio, false);
        tm_clk(led, 0); // TM1637 starts ACK (pulls DIO low)
    } else {
        tm_clk(led, 0); // TM1637 starts ACK (pulls DIO low)
        pin_dir(led->tm_dio, false);
    }
    tm_delay(led);

    bool ack = false;
    for (uint32_t t = 0; t <= CONFIG_TM1637_ACK_TIMEOUT_US; ++t) {
        if (pin_read(led->tm_dio) == 0) {
            ack = true;
            break;
        }
        TM_DELAY_US(1);
    }

    tm_clk(led, 1);
    tm_delay(led);
    tm_clk(led, 0); // TM1637 ends ACK (releasing DIO)
    tm_delay(led);
    tm_dio(led, 0);  // level first, then drive again (next: data bit or stop, both start low)
    pin_dir(led->tm_dio, true);
    return ack;
}

static uint8_t tm1637_digitmap(uint8_t tm_digits, uint8_t digit) {
    if (tm_digits == 6) {
        /* usual wiring of the 6-digit modules */
        static const uint8_t map6[6] = { 2, 1, 0, 5, 4, 3 };
        return map6[digit];
    }
    return digit;
}

/* Writes `count` digits starting at `first` (fixed address mode), then the display control.
 * An offline display is skipped until the retry time; a missing ACK marks it offline. */
static void tm_write(tm1637_led_t * led, uint8_t first, const uint8_t *data, uint8_t count) {
    if (!led) {
        return;
    }
    if (!led->tm_online && (int32_t)(tm_now_tick() - led->tm_retry_tick) < 0) {
        return;
    }
    bool ok;
    tm_start(led);
    ok = tm_send_byte(led, TM1637_COMM3);
    tm_stop(led);
    for (uint8_t i = 0; ok && i < count && first + i < led->tm_digits; ++i) {
        tm_start(led);
        ok = tm_send_byte(led, tm1637_digitmap(led->tm_digits, first + i) | TM1637_COMM2) &&
             tm_send_byte(led, data[i]);
        tm_stop(led);
    }
    if (ok) {
        tm_start(led);
        ok = tm_send_byte(led, led->tm_bright | TM1637_LBRTN);
        tm_stop(led);
    }
    led->tm_online = ok;
    if (!ok) {
        led->tm_retry_tick = tm_now_tick() + pdMS_TO_TICKS(CONFIG_TM1637_RETRY_MS);
    }
}

/* ------------------------------------------------------------- API */

tm1637_led_t * tm1637_init(uint8_t pin_clk, uint8_t pin_dio, uint8_t n_digits) {
    tm1637_led_t * led = (tm1637_led_t *) calloc(1, sizeof(tm1637_led_t));
    if (!led) {
        return NULL;
    }
    led->tm_clk = pin_clk;
    led->tm_dio = pin_dio;
    led->tm_bright = 0x07;
    led->tm_delay_us = CONFIG_TM1637_BIT_DELAY_US;
    led->tm_online = true;

    if (n_digits < 1) {
        n_digits = 1;
    } else if (n_digits > TM1637_MAX_DIGITS) {
        n_digits = TM1637_MAX_DIGITS;
    }
    led->tm_digits = n_digits;

#ifdef ESP_PLATFORM
    gpio_config_t io_conf = {0};            /* interrupts disabled */
    io_conf.pin_bit_mask = (1ULL << pin_clk) | (1ULL << pin_dio);
    io_conf.mode = GPIO_MODE_OUTPUT;        /* push-pull; DIO turns to input for each ACK */
    /* keeps DIO defined while it is an input; helps the external resistors (weak, ~45 kOhm) */
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);
#else
    gpio_enable(pin_clk, GPIO_OUTPUT);
    gpio_enable(pin_dio, GPIO_OUTPUT);
#endif

    // Set CLK to low during DIO initialization to avoid sending a start signal by mistake
    tm_clk(led, 0);
    tm_delay(led);
    tm_dio(led, 1);
    tm_delay(led);
    tm_clk(led, 1);
    tm_delay(led);
    return led;
}

void tm1637_delete(tm1637_led_t * led) {
    free(led);
}

bool tm1637_is_online(const tm1637_led_t * led) {
    return led && led->tm_online;
}

void tm1637_set_delay_us(tm1637_led_t * led, uint8_t us) {
    if (led) {
        led->tm_delay_us = us ? us : 1;
    }
}

void tm1637_set_brightness(tm1637_led_t * led, uint8_t level) {
    if (level > 0x07) {
        level = 0x07;
    } // Check max level
    led->tm_bright = level;
}

void tm1637_set_segment_raw(tm1637_led_t * led, const uint8_t segment_idx, const uint8_t data) {
    if (segment_idx < led->tm_digits) {
        tm_write(led, segment_idx, &data, 1);
    }
}

static uint8_t tm1637_encode(uint8_t chr)
{
  if(chr > 127 || chr < 32) return 0;     // Blank
  return tm_ascii[chr - 32];
}

void tm1637_print_symbols(tm1637_led_t * led, const uint8_t *seg, uint8_t count) {
    uint8_t buf[TM1637_MAX_DIGITS] = {0};
    for (uint8_t i = 0; i < count && i < led->tm_digits; ++i) {
        buf[i] = seg[i];
    }
    tm_write(led, 0, buf, led->tm_digits);
}

void tm1637_print_string(tm1637_led_t * led, const char string_data[]) {
    uint8_t buf[TM1637_MAX_DIGITS] = {0};
    for (uint8_t i = 0; string_data && i < led->tm_digits && string_data[i]; ++i) {
        buf[i] = tm1637_encode((uint8_t)string_data[i]);
    }
    tm_write(led, 0, buf, led->tm_digits);
}

void tm1637_print_4_symbols(tm1637_led_t * led, uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4) {
    const uint8_t seg_data[] = {d1, d2, d3, d4};
    tm1637_print_symbols(led, seg_data, 4);
}

void tm1637_print_6_symbols(tm1637_led_t * led, uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4, uint8_t d5, uint8_t d6) {
    const uint8_t seg_data[] = {d1, d2, d3, d4, d5, d6};
    tm1637_print_symbols(led, seg_data, 6);
}

/* whole value right aligned in `n` digits starting at buf[0]; blank leading zeros */
static void put_int(uint8_t *buf, uint8_t n, int v) {
    for (int i = n - 1; i >= 0; --i) {
        buf[i] = (v || i == n - 1) ? tm_num[v % 10] : 0x00;
        v /= 10;
    }
}

void tm1637_print_temp(tm1637_led_t * led, int t) {
    uint8_t buf[TM1637_MAX_DIGITS] = {0};
    uint8_t n = led->tm_digits;
    bool msign = t < 0;
    int a = msign ? -t : t;                 /* tenths */

    if (n >= 4) {
        /* 6 digits: -XX.XºC; 4 digits: XX.Xº, -X.Xº, -XXº */
        uint8_t digit = (n == 6) ? 1 : 0;
        if (a > 999) {
            a = 999;
        }
        uint8_t s = a / 100, d = (a / 10) % 10, e = a % 10;
        if (n == 6) {
            buf[0] = msign ? TM_MINUS : 0x00;
            buf[5] = TM_C;
        } else if (msign) {
            buf[0] = TM_MINUS;
            digit = 1;
        }
        if (msign && n == 4) {
            if (s == 0) {                   /* -9.9º */
                buf[1] = tm_num[d] | TM_DP;
                buf[2] = tm_num[e];
            } else {                        /* -12º */
                int w = (a + 5) / 10;
                if (w > 99) {
                    w = 99;
                }
                buf[1] = tm_num[w / 10];
                buf[2] = tm_num[w % 10];
            }
            buf[3] = TM_DEG;
        } else {
            buf[digit] = s ? tm_num[s] : 0x00;
            buf[digit + 1] = tm_num[d] | TM_DP;
            buf[digit + 2] = tm_num[e];
            buf[digit + 3] = TM_DEG;
        }
    } else if (n == 3) {
        /* 23.2, " 5.3", 150, -9.9, -12 */
        if (!msign && a < 1000) {
            buf[0] = (a / 100) ? tm_num[a / 100] : 0x00;
            buf[1] = tm_num[(a / 10) % 10] | TM_DP;
            buf[2] = tm_num[a % 10];
        } else if (!msign) {
            int w = (a + 5) / 10;
            put_int(buf, 3, w > 999 ? 999 : w);
        } else if (a < 100) {
            buf[0] = TM_MINUS;
            buf[1] = tm_num[a / 10] | TM_DP;
            buf[2] = tm_num[a % 10];
        } else {
            int w = (a + 5) / 10;
            buf[0] = TM_MINUS;
            put_int(&buf[1], 2, w > 99 ? 99 : w);
        }
    } else {
        /* 1 - 2 digits: whole degrees */
        int w = (a + 5) / 10;
        int max = (n == 2) ? (msign ? 9 : 99) : (msign ? 0 : 9);
        if (w > max) {
            w = max;
        }
        if (msign && n == 2) {
            buf[0] = TM_MINUS;
            buf[1] = tm_num[w];
        } else {
            put_int(buf, n, w);
        }
    }
    tm_write(led, 0, buf, n);
}

void tm1637_print_float(tm1637_led_t * led, float f_value) {

    bool msign = false;
    uint8_t k;
    uint8_t seg_data = 0x00;
    uint8_t int_part = 0x00; //digits count before decimal point
    int precision = 0x00; //digits after decimal point
    int i;
    
    if (f_value < 0) {
        f_value = f_value * (-1);
        msign = true;
    }
    
    if (f_value < 10) {
        int_part = 1;
    } else if (f_value < 100) {
        int_part = 2;
    } else if (f_value < 1000) {
        int_part = 3;
    } else if (f_value < 10000) {
        int_part = 4;
    } else if (f_value < 100000) {
        int_part = 5;
    } else {
        int_part = led->tm_digits;
    }
    
    precision = led->tm_digits - int_part;
    
    if (msign) {
        precision = precision - 1;
    }

    if (precision < 0) {
        precision = precision * (-1);
        f_value = f_value / pow(10,precision);
    } else {
        f_value = f_value * pow(10,precision);
    }
    
    uint8_t dp = led->tm_digits - precision - 1; //no dp in case precision is negative
    uint32_t num = f_value;
    uint32_t sum = 0;

    //int_part => how many digits in total
    if (msign) {
        int_part = int_part + precision + 1;
    } else {
        int_part = int_part + precision;
    }

    uint8_t f_num[int_part];               /* one element per printed digit */
    k = int_part - 1;
    
    for (i = 0; i < int_part; ++i) {
        if (i == 0 && msign == true) {
            seg_data = TM1637_COMM1; //minus sign
            tm1637_set_segment_raw(led, i, seg_data);
        } else {
            f_num[i] = (uint8_t)((num - sum) / pow(10,(k - i)));
            sum = sum + f_num[i] * pow(10,(k - i));
            seg_data = tm_num[f_num[i]];
            if (i == dp && precision > 0) {
                seg_data |= TM1637_COMM4; //decimal point
            }
            tm1637_set_segment_raw(led, i, seg_data);
        }
    }
}

void tm1637_test_segments(tm1637_led_t * led) {
    const uint8_t all[TM1637_MAX_DIGITS] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    tm1637_print_symbols(led, all, TM1637_MAX_DIGITS);
}

void tm1637_clear(tm1637_led_t * led) {
    tm1637_print_symbols(led, NULL, 0);
}
