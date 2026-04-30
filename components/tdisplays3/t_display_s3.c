// SPDX-FileCopyrightText: © 2025 Hiruna Wijesinghe <hiruna.kawinda@gmail.com>
// SPDX-License-Identifier: MIT

#include "t_display_s3.h"
#include <string.h>
#include "button_gpio.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_lcd_panel_st7789.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"

#define TAG "tdisplays3"

#define LCD_PIN_NUM_PWR      15
#define LCD_PIN_NUM_BK_LIGHT 38
#define LCD_PIN_NUM_DATA0    39
#define LCD_PIN_NUM_DATA1    40
#define LCD_PIN_NUM_DATA2    41
#define LCD_PIN_NUM_DATA3    42
#define LCD_PIN_NUM_DATA4    45
#define LCD_PIN_NUM_DATA5    46
#define LCD_PIN_NUM_DATA6    47
#define LCD_PIN_NUM_DATA7    48
#define LCD_PIN_NUM_PCLK     8
#define LCD_PIN_NUM_RD       9
#define LCD_PIN_NUM_DC       7
#define LCD_PIN_NUM_CS       6
#define LCD_PIN_NUM_RST      5

#define BTN_PIN_NUM_1 GPIO_NUM_0
#define BTN_PIN_NUM_2 GPIO_NUM_14

#define LCD_H_RES              320
#define LCD_V_RES              170
#define LCD_PIXEL_CLOCK_HZ     (17 * 1000 * 1000)
#define LCD_I80_TRANS_QUEUE_SIZE 20
#define LCD_PSRAM_TRANS_ALIGN  64
#define LCD_SRAM_TRANS_ALIGN   4
#define LVGL_BUFFER_SIZE       (((LCD_H_RES * LCD_V_RES) / 10) + LCD_H_RES)

static bool s_initialized = false;
static button_handle_t s_button_handles[2] = {0};
static tdisplays3_button_cb_t s_user_callbacks[2] = {0};

static void configure_gpio(void)
{
    const gpio_config_t pwr = {
        .pin_bit_mask = 1ULL << LCD_PIN_NUM_PWR,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&pwr));
    ESP_ERROR_CHECK(gpio_set_level(LCD_PIN_NUM_PWR, 1));

    const gpio_config_t backlight = {
        .pin_bit_mask = 1ULL << LCD_PIN_NUM_BK_LIGHT,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&backlight));
    ESP_ERROR_CHECK(gpio_set_level(LCD_PIN_NUM_BK_LIGHT, 1));

    const gpio_config_t rd = {
        .pin_bit_mask = 1ULL << LCD_PIN_NUM_RD,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&rd));

    const gpio_config_t buttons = {
        .pin_bit_mask = (1ULL << BTN_PIN_NUM_1) | (1ULL << BTN_PIN_NUM_2),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&buttons));
}

static int button_index_from_handle(button_handle_t button_handle)
{
    for (int i = 0; i < 2; i++) {
        if (s_button_handles[i] == button_handle) {
            return i;
        }
    }
    return -1;
}

static void button_dispatch_cb(void *arg, void *usr_data)
{
    (void)usr_data;
    const button_handle_t button_handle = (button_handle_t)arg;
    const int idx = button_index_from_handle(button_handle);
    if (idx < 0 || s_user_callbacks[idx] == NULL) {
        return;
    }

    const button_event_t event = iot_button_get_event(button_handle);
    s_user_callbacks[idx](event);
}

static esp_err_t init_buttons(void)
{
    const gpio_num_t pins[2] = {BTN_PIN_NUM_1, BTN_PIN_NUM_2};

    for (int i = 0; i < 2; i++) {
        const button_config_t btn_cfg = {0};
        const button_gpio_config_t btn_gpio_cfg = {
            .gpio_num = (int32_t)pins[i],
            .active_level = 0,
        };
        ESP_RETURN_ON_ERROR(
            iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &s_button_handles[i]),
            TAG,
            "button create failed");
    }
    return ESP_OK;
}

static esp_err_t init_display(tdisplays3_handle_t *handle)
{
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 2,
        .task_stack = 4096,
        .task_affinity = 1,
        .task_max_sleep_ms = 10,
        .timer_period_ms = 5,
    };
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "lvgl init failed");

    esp_lcd_i80_bus_handle_t i80_bus = NULL;
    const esp_lcd_i80_bus_config_t bus_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .dc_gpio_num = LCD_PIN_NUM_DC,
        .wr_gpio_num = LCD_PIN_NUM_PCLK,
        .data_gpio_nums = {
            LCD_PIN_NUM_DATA0,
            LCD_PIN_NUM_DATA1,
            LCD_PIN_NUM_DATA2,
            LCD_PIN_NUM_DATA3,
            LCD_PIN_NUM_DATA4,
            LCD_PIN_NUM_DATA5,
            LCD_PIN_NUM_DATA6,
            LCD_PIN_NUM_DATA7,
        },
        .bus_width = 8,
        .max_transfer_bytes = LCD_H_RES * 100 * sizeof(uint16_t),
        .psram_trans_align = LCD_PSRAM_TRANS_ALIGN,
        .sram_trans_align = LCD_SRAM_TRANS_ALIGN,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_i80_bus(&bus_config, &i80_bus), TAG, "i80 bus init failed");

    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_i80_config_t io_config = {
        .cs_gpio_num = LCD_PIN_NUM_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .trans_queue_depth = LCD_I80_TRANS_QUEUE_SIZE,
        .dc_levels = {
            .dc_idle_level = 0,
            .dc_cmd_level = 0,
            .dc_dummy_level = 0,
            .dc_data_level = 1,
        },
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &io_handle), TAG, "i80 io init failed");

    esp_lcd_panel_handle_t panel_handle = NULL;
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_NUM_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle), TAG, "panel init failed");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel_handle), TAG, "panel reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel_handle), TAG, "panel hw init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(panel_handle, true), TAG, "panel invert failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(panel_handle, true), TAG, "panel swap_xy failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(panel_handle, false, true), TAG, "panel mirror failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_set_gap(panel_handle, 0, 35), TAG, "panel gap failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(panel_handle, true), TAG, "panel on failed");

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = LVGL_BUFFER_SIZE,
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .swap_xy = true,
            .mirror_x = false,
            .mirror_y = true,
        },
        .flags = {
            .buff_spiram = true,
            .swap_bytes = true,
        },
    };
    handle->display = lvgl_port_add_disp(&disp_cfg);
    if (handle->display == NULL) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t tdisplays3_init(tdisplays3_handle_t *handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(handle, 0, sizeof(*handle));

    if (!s_initialized) {
        configure_gpio();
        ESP_RETURN_ON_ERROR(init_buttons(), TAG, "button init failed");
        ESP_RETURN_ON_ERROR(init_display(handle), TAG, "display init failed");
        s_initialized = true;
        return ESP_OK;
    }

    ESP_LOGW(TAG, "already initialized");
    return ESP_ERR_INVALID_STATE;
}

bool tdisplays3_display_lock(uint32_t timeout_ms)
{
    return lvgl_port_lock(timeout_ms);
}

void tdisplays3_display_unlock(void)
{
    lvgl_port_unlock();
}

esp_err_t tdisplays3_button_register_callback(tdisplays3_button_t button, tdisplays3_button_cb_t callback)
{
    if (!s_initialized || button > TDISPLAYS3_BUTTON_2 || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_user_callbacks[button] != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    s_user_callbacks[button] = callback;
    ESP_RETURN_ON_ERROR(
        iot_button_register_cb(s_button_handles[button], BUTTON_PRESS_DOWN, NULL, button_dispatch_cb, NULL),
        TAG,
        "button down callback register failed");
    ESP_RETURN_ON_ERROR(
        iot_button_register_cb(s_button_handles[button], BUTTON_PRESS_UP, NULL, button_dispatch_cb, NULL),
        TAG,
        "button up callback register failed");
    return ESP_OK;
}