// SPDX-FileCopyrightText: © 2025 Hiruna Wijesinghe <hiruna.kawinda@gmail.com>
// SPDX-License-Identifier: MIT

// High-level app flow:
// 1) Initialize board peripherals and UI labels.
// 2) Initialize IMU component (sensor + DMP).
// 3) Read roll/pitch/yaw from IMU component and render on display.

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "imu.h"
#include "t_display_s3.h"

#include <stdio.h>

static const char *TAG = "main";
static lv_obj_t *s_label_btn1;
static lv_obj_t *s_label_btn2;
static lv_obj_t *s_label_imu;
static lv_obj_t *s_label_rpy;

static void set_label_text(lv_obj_t *label, const char *text)
{
    if (label == NULL || text == NULL) {
        return;
    }
    if (!tdisplays3_display_lock(0)) {
        return;
    }
    lv_label_set_text(label, text);
    tdisplays3_display_unlock();
}

static void button1_event_cb(button_event_t event)
{
    set_label_text(s_label_btn1, (event == BUTTON_PRESS_DOWN) ? "Button 1: PRESSED" : "Button 1: RELEASED");
}

static void button2_event_cb(button_event_t event)
{
    set_label_text(s_label_btn2, (event == BUTTON_PRESS_DOWN) ? "Button 2: PRESSED" : "Button 2: RELEASED");
}

void app_main(void)
{
    tdisplays3_handle_t board;
    ESP_ERROR_CHECK(tdisplays3_init(&board));

    ESP_RETURN_VOID_ON_FALSE(tdisplays3_display_lock(0), TAG, "display lock failed");

    lv_obj_t *title = lv_label_create(lv_screen_active());
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 8, 8);
    lv_label_set_text(title, "ICM20948 DMP Demo");

    s_label_btn1 = lv_label_create(lv_screen_active());
    lv_obj_align(s_label_btn1, LV_ALIGN_TOP_LEFT, 8, 40);
    lv_label_set_text(s_label_btn1, "Button 1: RELEASED");

    s_label_btn2 = lv_label_create(lv_screen_active());
    lv_obj_align(s_label_btn2, LV_ALIGN_TOP_LEFT, 8, 72);
    lv_label_set_text(s_label_btn2, "Button 2: RELEASED");

    s_label_imu = lv_label_create(lv_screen_active());
    lv_obj_align(s_label_imu, LV_ALIGN_TOP_LEFT, 8, 104);
    lv_label_set_text(s_label_imu, "IMU: Initializing...");

    s_label_rpy = lv_label_create(lv_screen_active());
    lv_obj_align(s_label_rpy, LV_ALIGN_TOP_LEFT, 8, 136);
    lv_label_set_text(s_label_rpy, "R: --.- P: --.- Y: --.-");

    tdisplays3_display_unlock();

    ESP_ERROR_CHECK(tdisplays3_button_register_callback(TDISPLAYS3_BUTTON_1, button1_event_cb));
    ESP_ERROR_CHECK(tdisplays3_button_register_callback(TDISPLAYS3_BUTTON_2, button2_event_cb));

    esp_err_t err = imu_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "imu_init failed: %s", esp_err_to_name(err));
        set_label_text(s_label_imu, "IMU: INIT FAILED");
        return;
    }
    set_label_text(s_label_imu, "IMU: DMP RUNNING");

    while (true) {
        imu_rpy_t rpy;
        if (imu_read_rpy(&rpy) == ESP_OK) {
            char text[64];
            snprintf(text, sizeof(text), "R:%6.1f P:%6.1f Y:%6.1f", rpy.roll_deg, rpy.pitch_deg, rpy.yaw_deg);
            set_label_text(s_label_rpy, text);
            ESP_LOGI(TAG, "%s", text);
        }
    }
}
