// SPDX-FileCopyrightText: © 2025 Hiruna Wijesinghe <hiruna.kawinda@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <stdbool.h>
#include "esp_err.h"
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
    lv_obj_t *label;
} tdisplays3_handle_t;

esp_err_t tdisplays3_init(tdisplays3_handle_t *handle);
esp_err_t tdisplays3_display_text(tdisplays3_handle_t *handle, const char *text);
bool tdisplays3_button_pressed(tdisplays3_button_t button);

#ifdef __cplusplus
} /* extern "C" */
#endif