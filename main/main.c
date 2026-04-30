// SPDX-FileCopyrightText: © 2025 Hiruna Wijesinghe <hiruna.kawinda@gmail.com>
// SPDX-License-Identifier: MIT

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "t_display_s3.h"

static const char *TAG = "main";
static lv_obj_t *s_label_hello;
static lv_obj_t *s_label_btn1;
static lv_obj_t *s_label_btn2;

static esp_err_t set_label_text(lv_obj_t *label, const char *text)
{
    ESP_RETURN_ON_FALSE(label != NULL, ESP_ERR_INVALID_ARG, TAG, "label is null");
    ESP_RETURN_ON_FALSE(text != NULL, ESP_ERR_INVALID_ARG, TAG, "text is null");
    ESP_RETURN_ON_FALSE(tdisplays3_display_lock(0), ESP_FAIL, TAG, "display lock failed");
    lv_label_set_text(label, text);
    tdisplays3_display_unlock();
    return ESP_OK;
}

static void button1_event_cb(button_event_t event)
{
    const char *text = (event == BUTTON_PRESS_DOWN) ? "Button 1: PRESSED" : "Button 1: RELEASED";
    ESP_ERROR_CHECK_WITHOUT_ABORT(set_label_text(s_label_btn1, text));
    ESP_LOGI(TAG, "button1 event=%d", event);
}

static void button2_event_cb(button_event_t event)
{
    const char *text = (event == BUTTON_PRESS_DOWN) ? "Button 2: PRESSED" : "Button 2: RELEASED";
    ESP_ERROR_CHECK_WITHOUT_ABORT(set_label_text(s_label_btn2, text));
    ESP_LOGI(TAG, "button2 event=%d", event);
}

void app_main(void)
{
    tdisplays3_handle_t board;
    ESP_ERROR_CHECK(tdisplays3_init(&board));

    ESP_RETURN_VOID_ON_FALSE(tdisplays3_display_lock(0), TAG, "display lock failed");
    s_label_hello = lv_label_create(lv_screen_active());
    lv_obj_align(s_label_hello, LV_ALIGN_TOP_LEFT, 8, 8);
    lv_label_set_text(s_label_hello, "Hello world");

    s_label_btn1 = lv_label_create(lv_screen_active());
    lv_obj_align(s_label_btn1, LV_ALIGN_TOP_LEFT, 8, 40);
    lv_label_set_text(s_label_btn1, "Button 1: RELEASED");

    s_label_btn2 = lv_label_create(lv_screen_active());
    lv_obj_align(s_label_btn2, LV_ALIGN_TOP_LEFT, 8, 72);
    lv_label_set_text(s_label_btn2, "Button 2: RELEASED");
    tdisplays3_display_unlock();

    ESP_ERROR_CHECK(tdisplays3_button_register_callback(TDISPLAYS3_BUTTON_1, button1_event_cb));
    ESP_ERROR_CHECK(tdisplays3_button_register_callback(TDISPLAYS3_BUTTON_2, button2_event_cb));
}
