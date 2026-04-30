// SPDX-FileCopyrightText: © 2025 Hiruna Wijesinghe <hiruna.kawinda@gmail.com>
// SPDX-License-Identifier: MIT

#include <stdio.h>
#include <stdatomic.h>
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "t_display_s3.h"

static const char *TAG = "main";
static atomic_bool s_btn1_pressed = false;
static atomic_bool s_btn2_pressed = false;

static void on_button_event(tdisplays3_button_t button, button_event_t event, void *user_data)
{
    (void)user_data;
    const bool pressed = (event == BUTTON_PRESS_DOWN || event == BUTTON_LONG_PRESS_START ||
                          event == BUTTON_LONG_PRESS_HOLD);
    const bool released = (event == BUTTON_PRESS_UP || event == BUTTON_PRESS_END);

    if (!pressed && !released) {
        return;
    }

    const bool state = pressed;
    if (button == TDISPLAYS3_BUTTON_1) {
        atomic_store(&s_btn1_pressed, state);
    } else if (button == TDISPLAYS3_BUTTON_2) {
        atomic_store(&s_btn2_pressed, state);
    }
}

void app_main(void)
{
    tdisplays3_handle_t board;
    ESP_ERROR_CHECK(tdisplays3_init(&board));

    ESP_ERROR_CHECK(
        tdisplays3_button_register_callback(TDISPLAYS3_BUTTON_1, BUTTON_PRESS_DOWN, NULL, on_button_event, NULL));
    ESP_ERROR_CHECK(
        tdisplays3_button_register_callback(TDISPLAYS3_BUTTON_1, BUTTON_PRESS_UP, NULL, on_button_event, NULL));
    ESP_ERROR_CHECK(
        tdisplays3_button_register_callback(TDISPLAYS3_BUTTON_2, BUTTON_PRESS_DOWN, NULL, on_button_event, NULL));
    ESP_ERROR_CHECK(
        tdisplays3_button_register_callback(TDISPLAYS3_BUTTON_2, BUTTON_PRESS_UP, NULL, on_button_event, NULL));

    lv_obj_t *label = NULL;
    if (tdisplays3_display_lock(0)) {
        label = lv_label_create(lv_screen_active());
        lv_obj_set_width(label, 320);
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_align(label, LV_ALIGN_TOP_LEFT, 8, 8);
        tdisplays3_display_unlock();
    } else {
        ESP_LOGE(TAG, "failed to acquire display lock for UI setup");
        return;
    }

    while (true) {
        const bool btn1_pressed = atomic_load(&s_btn1_pressed);
        const bool btn2_pressed = atomic_load(&s_btn2_pressed);

        char text[128];
        snprintf(text, sizeof(text),
                 "Hello world\n\nButton 1: %s\nButton 2: %s",
                 btn1_pressed ? "PRESSED" : "RELEASED",
                 btn2_pressed ? "PRESSED" : "RELEASED");

        if (tdisplays3_display_lock(0)) {
            lv_label_set_text(label, text);
            tdisplays3_display_unlock();
        }
        ESP_LOGI(TAG, "btn1=%d btn2=%d", btn1_pressed, btn2_pressed);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}