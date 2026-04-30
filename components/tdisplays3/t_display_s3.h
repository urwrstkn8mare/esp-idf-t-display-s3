// SPDX-FileCopyrightText: © 2025 Hiruna Wijesinghe <hiruna.kawinda@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "iot_button.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TDISPLAYS3_BUTTON_1 = 0,
    TDISPLAYS3_BUTTON_2 = 1,
} tdisplays3_button_t;

typedef struct {
    lv_display_t *display;
} tdisplays3_handle_t;

typedef void (*tdisplays3_button_cb_t)(tdisplays3_button_t button, button_event_t event, void *user_data);

esp_err_t tdisplays3_init(tdisplays3_handle_t *handle);
lv_display_t *tdisplays3_display_get(tdisplays3_handle_t *handle);
bool tdisplays3_display_lock(uint32_t timeout_ms);
void tdisplays3_display_unlock(void);
bool tdisplays3_button_pressed(tdisplays3_button_t button);
esp_err_t tdisplays3_button_register_callback(tdisplays3_button_t button,
                                              button_event_t event,
                                              button_event_args_t *event_args,
                                              tdisplays3_button_cb_t callback,
                                              void *user_data);

#ifdef __cplusplus
} /* extern "C" */
#endif