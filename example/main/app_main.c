/**
 * @file app_main.c
 * @brief Example application for the TM1637 LED segment display
 */

#include <stdio.h>
#include <stdbool.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <driver/gpio.h>
#include <esp_log.h>

#include "esp_tm1637.h"

#define TAG "TM1637 example"

#define LED_CLK CONFIG_TM1637_CLK_PIN
#define LED_DTA CONFIG_TM1637_DIO_PIN

void tm1637_task(void * arg)
{
    uint8_t n_digits = 6;
	tm1637_led_t * display = tm1637_init(LED_CLK, LED_DTA, n_digits);

	while (true)
	{
        //Initial test
        tm1637_test_segments(display);
        vTaskDelay(2000 / portTICK_RATE_MS);
        tm1637_clear(display);
        vTaskDelay(1000 / portTICK_RATE_MS);
        
        //Test stmbols
        tm1637_print_string(display, "Lets");
        vTaskDelay(1000 / portTICK_RATE_MS);
        tm1637_print_string(display, " go ");
        vTaskDelay(1000 / portTICK_RATE_MS);
        tm1637_print_string(display, "Auto");
        vTaskDelay(1000 / portTICK_RATE_MS);
        
        //Test stmbols
        uint8_t display_text[6] = {0x38, 0x79, 0x78, 0x6D};
        tm1637_print_symbols(display, display_text);
        vTaskDelay(1000 / portTICK_RATE_MS);
        display_text [0] = 0x3D;
        display_text [1] = 0x79;
        display_text [2] = 0x78;
        display_text [3] = 0x00;
        tm1637_print_symbols(display, display_text);
        vTaskDelay(1000 / portTICK_RATE_MS);
        if (n_digits == 6) {
            display_text [0] = 0x08;
            display_text [1] = 0x58;
            display_text [2] = 0x5C;
            display_text [3] = 0x5C;
            display_text [4] = 0x18;
            display_text [5] = 0x08;
            tm1637_print_symbols(display, display_text);
            vTaskDelay(2000 / portTICK_RATE_MS);
        }

        // Test print float
        float test_number = 99.99;
        while (test_number > - 99.99) {
            if (test_number > 5) {
                test_number = test_number - 1.234;
            } else if (test_number < 5 && test_number > - 5) {
                test_number = test_number - 0.123;
            } else {
                test_number = test_number - 5.678;
            }
            tm1637_print_float(display, test_number);
            vTaskDelay(100 / portTICK_RATE_MS);
        }

         // Test print temperature
         int temp = -125;
         while (temp < 200) {
             temp = temp + 1;
             tm1637_print_temp(display, temp);
             vTaskDelay(50 / portTICK_RATE_MS);
         }
        
        if (n_digits == 6) {
            // Test segment control
            uint8_t seg_data[] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20};
            for (uint8_t x=0; x<32; ++x)
            {
                uint8_t v_seg_data = seg_data[x%6];
                tm1637_set_segment_raw(display, 0, v_seg_data);
                tm1637_set_segment_raw(display, 1, v_seg_data);
                tm1637_set_segment_raw(display, 2, v_seg_data);
                tm1637_set_segment_raw(display, 3, v_seg_data);
                tm1637_set_segment_raw(display, 4, v_seg_data);
                tm1637_set_segment_raw(display, 5, v_seg_data);
                vTaskDelay(100 / portTICK_PERIOD_MS);
            }

            // Test brightness
            for (int x=0; x<7; x++) {
                tm1637_set_brightness(display, x);
                tm1637_print_float(display, 888888);
                vTaskDelay(300 / portTICK_RATE_MS);
            }
        } else {
            // Test segment control
            uint8_t seg_data[] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20};
            for (uint8_t x=0; x<32; ++x)
            {
                uint8_t v_seg_data = seg_data[x%6];
                tm1637_set_segment_raw(display, 0, v_seg_data);
                tm1637_set_segment_raw(display, 1, v_seg_data);
                tm1637_set_segment_raw(display, 2, v_seg_data);
                tm1637_set_segment_raw(display, 3, v_seg_data);
                vTaskDelay(100 / portTICK_PERIOD_MS);
            }

            // Test brightness
            for (int x=0; x<7; x++) {
                tm1637_set_brightness(display, x);
                tm1637_print_float(display, 8888);
                vTaskDelay(300 / portTICK_RATE_MS);
            }
        }
             
        tm1637_clear(display);
        vTaskDelay(1000 / portTICK_RATE_MS);
	}
}

void app_main()
{
	xTaskCreate(&tm1637_task, "tm1637_task", 4096, NULL, 5, NULL);
}

