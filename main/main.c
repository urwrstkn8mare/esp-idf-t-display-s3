// SPDX-FileCopyrightText: © 2025 Hiruna Wijesinghe <hiruna.kawinda@gmail.com>
// SPDX-License-Identifier: MIT

#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "t_display_s3.h"

static const char *TAG = "main";

void app_main(void)
{
    tdisplays3_handle_t board;
    ESP_ERROR_CHECK(tdisplays3_init(&board));

    while (true) {
        const bool btn1_pressed = tdisplays3_button_pressed(TDISPLAYS3_BUTTON_1);
        const bool btn2_pressed = tdisplays3_button_pressed(TDISPLAYS3_BUTTON_2);

        char text[128];
        snprintf(text, sizeof(text),
                 "Hello world\n\nButton 1: %s\nButton 2: %s",
                 btn1_pressed ? "PRESSED" : "RELEASED",
                 btn2_pressed ? "PRESSED" : "RELEASED");

        ESP_ERROR_CHECK(tdisplays3_display_text(&board, text));
        ESP_LOGI(TAG, "btn1=%d btn2=%d", btn1_pressed, btn2_pressed);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}