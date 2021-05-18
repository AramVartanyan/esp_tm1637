/*
 *  esp_tm1637.c
 *
 *
 *  Created by Aram Vartanyan on 6.05.21.
 *  based on https://github.com/petrows/esp-32-tm1637
 *  and on https://github.com/jasonacox
 */

#include "esp_tm1637.h"

#include "stdio.h"
#include "stdarg.h"
#include "stdlib.h"
#include "stdbool.h"
#include "string.h"
#include "math.h"
#include "driver/gpio.h"
#include "esp32/rom/ets_sys.h"
//#include "esp_log.h"
//static const char *TAG = "TM1637 library";

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

#define DELAY_US 3 // safety is 5 us

#define TM_DEG 0x63

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
    0x20, // 105 i
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

static void tm_start(tm1637_led_t * led);
static void tm_stop(tm1637_led_t * led);
static void tm_send_byte(tm1637_led_t * led, uint8_t byte);
static void tm_delay();
static void output_level(uint8_t gpio, uint8_t lvl);
static uint8_t tm1637_digitmap(uint8_t tm_digits, uint8_t digit);

static void tm_delay() {

#if defined CONFIG_IDF_TARGET_ESP8266 || defined CONFIG_IDF_TARGET_ESP32
    ets_delay_us(DELAY_US);
#else
    //esp-open-rtos
    sdk_os_delay_us(DELAY_US);
#endif
}

static void output_level(uint8_t gpio, uint8_t lvl) {

#if defined CONFIG_IDF_TARGET_ESP8266 || defined CONFIG_IDF_TARGET_ESP32
    gpio_set_level(gpio, lvl);
#else
    //esp-open-rtos
    gpio_write(gpio, lvl);
#endif
}

static void tm_start(tm1637_led_t * led) {
    // Send start signal
    // Both outputs are expected to be HIGH beforehand
    output_level(led->tm_dio, 0);
    tm_delay();
}

static void tm_stop(tm1637_led_t * led) {
    // Send stop signal
    // CLK is expected to be LOW beforehand
    output_level(led->tm_dio, 0);
    tm_delay();
    output_level(led->tm_clk, 1);
    tm_delay();
    output_level(led->tm_dio, 1);
    tm_delay();
}

static void tm_send_byte(tm1637_led_t * led, uint8_t byte) {
    for (uint8_t i=0; i<8; ++i) {
        output_level(led->tm_clk, 0);
        tm_delay();
        output_level(led->tm_dio, byte & 0x01); // Send current bit
        byte >>= 1;
        tm_delay();
        output_level(led->tm_clk, 1);
        tm_delay();
    }
    
    /*
     The TM1637 signals an ACK by pulling DIO low from the falling edge of CLK after sending the 8th bit, to the next falling edge of CLK. DIO needs to be set as input during this time to avoid having both chips trying to drive DIO at the same time.
     */
    
    output_level(led->tm_clk, 0); // TM1637 starts ACK (pulls DIO low)
    tm_delay();
   
#if defined CONFIG_IDF_TARGET_ESP8266 || defined CONFIG_IDF_TARGET_ESP32

    gpio_set_direction(led->tm_dio, GPIO_MODE_INPUT);
    uint8_t ack = gpio_get_level(led->tm_dio);
    while (ack);
    gpio_set_level(led->tm_clk, 1);
    tm_delay();
    gpio_set_level(led->tm_clk, 0); // TM1637 ends ACK (releasing DIO)
    tm_delay();
    gpio_set_direction(led->tm_dio, GPIO_MODE_OUTPUT);

#else
//esp-open-rtos
    gpio_enable(led->tm_dio, GPIO_INPUT);
    uint8_t ack = gpio_read(led->tm_dio);
    while (ack);
    gpio_write(led->tm_clk, 1);
    tm_delay();
    gpio_write(led->tm_clk, 0);
    tm_delay();
    gpio_enable(led->tm_dio, GPIO_OUTPUT);

#endif
 
}

static uint8_t tm1637_digitmap(uint8_t tm_digits, uint8_t digit) {
    if (tm_digits == 6) {
        switch (digit) {
            case 0:
                digit = 2;
                break;
            case 1:
                digit = 1;
                break;
            case 2:
                digit = 0;
                break;
            case 3:
                digit = 5;
                break;
            case 4:
                digit = 4;
                break;
            case 5:
                digit = 3;
                break;
                
            default:
                break;
        }
    }
    return digit;
}

tm1637_led_t * tm1637_init(uint8_t pin_clk, uint8_t pin_dio, uint8_t n_digits) {
    tm1637_led_t * led = (tm1637_led_t *) malloc(sizeof(tm1637_led_t));
    led->tm_clk = pin_clk;
    led->tm_dio = pin_dio;
    led->tm_bright = 0x07;
    
    //old statement n_digits > 0 || n_digits < 7
    if (n_digits > 4) {
      led->tm_digits = 6;
    } else {
      led->tm_digits = 4;
    }
    
#if defined CONFIG_IDF_TARGET_ESP8266 || defined CONFIG_IDF_TARGET_ESP32
    gpio_config_t io_conf = {0};
    
    io_conf.pin_bit_mask = (1ULL<<pin_clk);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    gpio_config(&io_conf);
    
    io_conf.pin_bit_mask = (1ULL<<pin_dio);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    gpio_config(&io_conf);

#else
//esp-open-rtos
    gpio_enable(pin_clk, GPIO_OUTPUT);
    gpio_enable(pin_dio, GPIO_OUTPUT);

#endif
    
    // Set CLK to low during DIO initialization to avoid sending a start signal by mistake
    output_level(pin_clk, 0);
    tm_delay();
    output_level(pin_dio, 1);
    tm_delay();
    output_level(pin_clk, 1);
    tm_delay();
    return led;
}

void tm1637_set_brightness(tm1637_led_t * led, uint8_t level) {
    if (level > 0x07) {
        level = 0x07;
    } // Check max level
    led->tm_bright = level;
}

void tm1637_set_segment_raw(tm1637_led_t * led, const uint8_t segment_idx, const uint8_t data) {
    tm_start(led);
    tm_send_byte(led, TM1637_COMM3);
    tm_stop(led);
    tm_start(led);
    tm_send_byte(led, tm1637_digitmap(led->tm_digits, segment_idx) | TM1637_COMM2);
    tm_send_byte(led, data);
    tm_stop(led);
    tm_start(led);
    tm_send_byte(led, led->tm_bright | TM1637_LBRTN);
    tm_stop(led);
}

static uint8_t tm1637_encode(uint8_t chr)
{
  if(chr > 127 || chr < 32) return 0;     // Blank
  return tm_ascii[chr - 32];
}

void tm1637_print_string(tm1637_led_t * led, const char string_data[]) {
    
    tm_start(led);
    tm_send_byte(led, TM1637_COMM1);
    tm_stop(led);
    
    for (uint8_t i = 0; i < led->tm_digits; ++i) {
        tm_start(led);
        tm_send_byte(led, tm1637_digitmap(led->tm_digits, i) | TM1637_COMM2);
        tm_send_byte(led, tm1637_encode(string_data[i]));
        tm_stop(led);
    }
    
    tm_start(led);
    tm_send_byte(led, led->tm_bright | TM1637_LBRTN);
    tm_stop(led);
}

/*
 void tm1637_print_symbols(tm1637_led_t * led, uint8_t *seg_data) {
 
 tm_start(led);
 tm_send_byte(led, TM1637_COMM1);
 tm_stop(led);
 
 for (uint8_t i = 0; i < led->tm_digits; ++i) {
     tm_start(led);
     tm_send_byte(led, tm1637_digitmap(led->tm_digits, i) | TM1637_COMM2);
     tm_send_byte(led, seg_data[i]);
     tm_stop(led);
 }
 
 tm_start(led);
 tm_send_byte(led, led->tm_bright | TM1637_LBRTN);
 tm_stop(led);
}
 */

void tm1637_print_4_symbols(tm1637_led_t * led, uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4) {

    uint8_t seg_data[] = {d1, d2, d3, d4};
    tm_start(led);
    tm_send_byte(led, TM1637_COMM1);
    tm_stop(led);

    for (uint8_t i = 0; i < led->tm_digits; ++i) {
        tm_start(led);
        tm_send_byte(led, tm1637_digitmap(led->tm_digits, i) | TM1637_COMM2);
        tm_send_byte(led, seg_data[i]);
        tm_stop(led);
    }

    tm_start(led);
    tm_send_byte(led, led->tm_bright | TM1637_LBRTN);
    tm_stop(led);
}

void tm1637_print_6_symbols(tm1637_led_t * led, uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4, uint8_t d5, uint8_t d6) {

    uint8_t seg_data[] = {d1, d2, d3, d4, d5, d6};
    tm_start(led);
    tm_send_byte(led, TM1637_COMM1);
    tm_stop(led);

    for (uint8_t i = 0; i < led->tm_digits; ++i) {
        tm_start(led);
        tm_send_byte(led, tm1637_digitmap(led->tm_digits, i) | TM1637_COMM2);
        tm_send_byte(led, seg_data[i]);
        tm_stop(led);
    }

    tm_start(led);
    tm_send_byte(led, led->tm_bright | TM1637_LBRTN);
    tm_stop(led);
}

void tm1637_print_temp(tm1637_led_t * led, int t) {
    //t = t*10;
    bool msign = false;
    uint8_t digit, s, d, e;
    uint8_t seg_data = 0x00;
    
    if (t > 999) {
        t = 999;
    } else if (t < -999) {
        t = -999;
    }
    
    if (t < 0) {
        t = t * (-1);
        msign = true;
        seg_data = TM1637_COMM1; //minus sign
        tm1637_set_segment_raw(led, 0, seg_data);
    }
    
    if (led->tm_digits == 6) {
        //-XX.XºC
        digit = 1;
        tm1637_set_segment_raw(led, 5, 0x39);
        if (msign == false) {
            tm1637_set_segment_raw(led, 0, 0x00);
        }
    } else {
        digit = 0;
    }
    
    s = t / 100;
    d = (t - s*100) / 10;
    e = t - s*100 - d*10;
    
    if (msign && led->tm_digits == 4) {
        if (s == 0) {
            seg_data = tm_num[d];
            seg_data |= TM1637_COMM4; //decimal point
            tm1637_set_segment_raw(led, 1, seg_data);
            seg_data = tm_num[e];
            tm1637_set_segment_raw(led, 2, seg_data);
        } else {
            if (e > 4) {
                d = d + 1;
            }
            seg_data = tm_num[s];
            tm1637_set_segment_raw(led, 1, seg_data);
            seg_data = tm_num[d];
            tm1637_set_segment_raw(led, 2, seg_data);
        }
    } else {
        seg_data = tm_num[s];
        tm1637_set_segment_raw(led, digit, seg_data);
        seg_data = tm_num[d];
        seg_data |= TM1637_COMM4; //decimal point
        tm1637_set_segment_raw(led, digit+1, seg_data);
        seg_data = tm_num[e];
        tm1637_set_segment_raw(led, digit+2, seg_data);
    }
    seg_data = TM_DEG;
    tm1637_set_segment_raw(led, digit+3, seg_data);
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

    uint8_t f_num[int_part - 1];
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
    if (led->tm_digits == 6) {
        tm1637_print_6_symbols(led, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
    } else {
        tm1637_print_4_symbols(led, 0xFF, 0xFF, 0xFF, 0xFF);
    }
}

void tm1637_clear(tm1637_led_t * led) {
    if (led->tm_digits == 6) {
        tm1637_print_6_symbols(led, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    } else {
        tm1637_print_4_symbols(led, 0x00, 0x00, 0x00, 0x00);
    }
}
