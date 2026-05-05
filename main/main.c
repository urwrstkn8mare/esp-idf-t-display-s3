// SPDX-FileCopyrightText: © 2025 Hiruna Wijesinghe <hiruna.kawinda@gmail.com>
// SPDX-License-Identifier: MIT

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/twai.h"
#include "t_display_s3.h"

#include <stdio.h>
#include <string.h>

/**
 * LilyGO T-Display-S3: parallel LCD uses GPIO 5–9, 14–15, 38–42, 45–48; BOOT0/GPIO0 is Btn1.
 * Omit USB_JTAG pads (typically GPIO19/20) unless you remap USB away.
 * TWAI uses GPIO12/GPIO13 (not LCD, not Btn, not GPIO43/GPIO44).
 * MCU TWAI connects to CAN transceiver (SN65HVD230-class): TX/RX → DI/RO respectively.
 *
 * Override if your wiring differs: edit the two defines below.
 */
#define CAN_TWAI_TX_GPIO GPIO_NUM_12
#define CAN_TWAI_RX_GPIO GPIO_NUM_13

#define CAN_ARB_HEARTBEAT_BASE  0x200u
#define CAN_ARB_BUTTON_BASE     0x300u

static const char *TAG = "main";
static lv_obj_t *s_label_title;
static lv_obj_t *s_label_btn1;
static lv_obj_t *s_label_btn2;
static lv_obj_t *s_label_can_peer;
static lv_obj_t *s_label_can_counters;

static uint8_t s_node_id;
static uint32_t s_tx_hb_ok;
static uint32_t s_tx_btn_ok;
static uint32_t s_btn_local_seq;
static uint32_t s_rx_peer;
static uint32_t s_rx_last_seq;

static esp_err_t set_label_text(lv_obj_t *label, const char *text)
{
    ESP_RETURN_ON_FALSE(label != NULL, ESP_ERR_INVALID_ARG, TAG, "label is null");
    ESP_RETURN_ON_FALSE(text != NULL, ESP_ERR_INVALID_ARG, TAG, "text is null");
    ESP_RETURN_ON_FALSE(tdisplays3_display_lock(0), ESP_FAIL, TAG, "display lock failed");
    lv_label_set_text(label, text);
    tdisplays3_display_unlock();
    return ESP_OK;
}

static uint32_t read_le_u32(const uint8_t *b)
{
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8u) | ((uint32_t)b[2] << 16u) | ((uint32_t)b[3] << 24u);
}

static void transmit_heartbeat(void)
{
    uint32_t seq = s_tx_hb_ok + 1;
    const uint32_t arb = CAN_ARB_HEARTBEAT_BASE | s_node_id;
    uint8_t payload[8] = {
        (uint8_t)(seq & 0xffu),
        (uint8_t)((seq >> 8) & 0xffu),
        (uint8_t)((seq >> 16) & 0xffu),
        (uint8_t)((seq >> 24) & 0xffu),
        0,
        0,
        'H',
        'B',
    };
    twai_message_t msg = {.identifier = arb};
    memcpy(msg.data, payload, sizeof(payload));
    msg.data_length_code = 8;

    esp_err_t err = twai_transmit(&msg, pdMS_TO_TICKS(200));
    if (err == ESP_OK) {
        s_tx_hb_ok++;
    } else {
        ESP_LOGW(TAG, "heartbeat tx err=%s", esp_err_to_name(err));
    }
}

static void transmit_button_press(uint8_t button_index)
{
    s_btn_local_seq++;
    uint32_t seq = s_btn_local_seq;
    const uint32_t arb = CAN_ARB_BUTTON_BASE | s_node_id;
    uint8_t payload[8] = {
        button_index,
        s_node_id,
        (uint8_t)(seq & 0xffu),
        (uint8_t)((seq >> 8) & 0xffu),
        (uint8_t)((seq >> 16) & 0xffu),
        (uint8_t)((seq >> 24) & 0xffu),
        'B',
        'T',
    };
    twai_message_t msg = {.identifier = arb};
    memcpy(msg.data, payload, sizeof(payload));
    msg.data_length_code = 8;

    esp_err_t err = twai_transmit(&msg, pdMS_TO_TICKS(200));
    if (err == ESP_OK) {
        s_tx_btn_ok++;
    } else {
        ESP_LOGW(TAG, "button tx err=%s", esp_err_to_name(err));
    }
}

static void format_can_line(char *dst, size_t dst_len)
{
    twai_status_info_t info = {0};
    if (twai_get_status_info(&info) != ESP_OK) {
        snprintf(dst, dst_len, "CAN TWAI?");
        return;
    }

    if (info.state == TWAI_STATE_BUS_OFF) {
        (void)twai_initiate_recovery();
    }

    const char *st_name = "?";
    switch (info.state) {
    case TWAI_STATE_STOPPED: st_name = "STOPPED"; break;
    case TWAI_STATE_RUNNING: st_name = "RUN"; break;
    case TWAI_STATE_BUS_OFF: st_name = "BUS_OFF"; break;
    case TWAI_STATE_RECOVERING: st_name = "RECVR"; break;
    default: break;
    }

    snprintf(dst,
             dst_len,
             "%s TEC:%u REC:%u",
             st_name,
             (unsigned)info.tx_error_counter,
             (unsigned)info.rx_error_counter);
}

static void flush_ui_summary(void)
{
    char peer[140];
    char cnt[176];
    char status[112];

    snprintf(peer,
             sizeof(peer),
             "Peer frames:%u last seq:%u",
             (unsigned)s_rx_peer,
             (unsigned)s_rx_last_seq);

    snprintf(cnt,
             sizeof(cnt),
             "TX hb:%u btn:%u TWAI Tx%d Rx%d",
             (unsigned)s_tx_hb_ok,
             (unsigned)s_tx_btn_ok,
             (int)CAN_TWAI_TX_GPIO,
             (int)CAN_TWAI_RX_GPIO);

    format_can_line(status, sizeof(status));

    char combo[320];
    snprintf(combo, sizeof(combo), "%s\n%s", status, cnt);

    ESP_ERROR_CHECK_WITHOUT_ABORT(set_label_text(s_label_can_peer, peer));
    ESP_ERROR_CHECK_WITHOUT_ABORT(set_label_text(s_label_can_counters, combo));
}

static void handle_received(const twai_message_t *msg)
{
    const uint32_t id = msg->identifier;
    const uint8_t from = id & 0xffu;
    if (from == s_node_id) {
        return;
    }
    if (msg->data_length_code < 8) {
        return;
    }

    if (id >= CAN_ARB_BUTTON_BASE && id < (CAN_ARB_BUTTON_BASE + 0x100u)) {
        const uint32_t seq = read_le_u32(msg->data + 2);
        s_rx_peer++;
        s_rx_last_seq = seq;
        ESP_LOGI(TAG, "CAN BTN peer %02x Btn%u seq %u", from, (unsigned)msg->data[0], (unsigned)seq);
    } else if (id >= CAN_ARB_HEARTBEAT_BASE && id < (CAN_ARB_HEARTBEAT_BASE + 0x100u)) {
        const uint32_t seq = read_le_u32(msg->data + 0);
        s_rx_peer++;
        s_rx_last_seq = seq;
        ESP_LOGI(TAG, "CAN HB peer %02x seq %u", from, (unsigned)seq);
    } else {
        return;
    }

    flush_ui_summary();
}

static void can_rx_task(void *arg)
{
    (void)arg;
    twai_message_t msg;
    for (;;) {
        if (twai_receive(&msg, pdMS_TO_TICKS(500)) != ESP_OK) {
            flush_ui_summary();
            continue;
        }
        handle_received(&msg);
    }
}

static void can_tx_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(120 + (unsigned)(s_node_id & 127u)));
    const TickType_t period = pdMS_TO_TICKS(750);
    for (;;) {
        vTaskDelay(period);
        transmit_heartbeat();
        flush_ui_summary();
    }
}

static esp_err_t can_twai_init(void)
{
    twai_general_config_t g_cfg = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TWAI_TX_GPIO, CAN_TWAI_RX_GPIO, TWAI_MODE_NORMAL);
    g_cfg.tx_queue_len = 16;
    g_cfg.rx_queue_len = 16;
    const twai_timing_config_t t_cfg = TWAI_TIMING_CONFIG_500KBITS();
    const twai_filter_config_t f_cfg = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    ESP_RETURN_ON_ERROR(twai_driver_install(&g_cfg, &t_cfg, &f_cfg), TAG, "TWAI install");
    ESP_RETURN_ON_ERROR(twai_start(), TAG, "TWAI start");
    return ESP_OK;
}

static void button1_event_cb(button_event_t event)
{
    const char *text = (event == BUTTON_PRESS_DOWN) ? "B1 DN" : "B1 UP";
    ESP_ERROR_CHECK_WITHOUT_ABORT(set_label_text(s_label_btn1, text));
    ESP_LOGI(TAG, "button1 event=%d", event);
    if (event == BUTTON_PRESS_DOWN) {
        transmit_button_press(1);
        flush_ui_summary();
    }
}

static void button2_event_cb(button_event_t event)
{
    const char *text = (event == BUTTON_PRESS_DOWN) ? "B2 DN" : "B2 UP";
    ESP_ERROR_CHECK_WITHOUT_ABORT(set_label_text(s_label_btn2, text));
    ESP_LOGI(TAG, "button2 event=%d", event);
    if (event == BUTTON_PRESS_DOWN) {
        transmit_button_press(2);
        flush_ui_summary();
    }
}

void app_main(void)
{
    uint8_t mac[8] = {0};
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_EFUSE_FACTORY));
    s_node_id = mac[5];

    ESP_ERROR_CHECK(can_twai_init());

    tdisplays3_handle_t board;
    ESP_ERROR_CHECK(tdisplays3_init(&board));

    ESP_RETURN_VOID_ON_FALSE(tdisplays3_display_lock(0), TAG, "display lock failed");
    s_label_title = lv_label_create(lv_screen_active());
    lv_obj_set_style_text_font(s_label_title, &lv_font_montserrat_12, 0);
    lv_label_set_long_mode(s_label_title, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_label_title, 304);

    lv_obj_align(s_label_title, LV_ALIGN_TOP_LEFT, 4, 2);
    {
        char t[128];
        snprintf(t,
                 sizeof(t),
                 "CAN NODE:%02x GPIO TX:%u RX:%u 500k",
                 (unsigned)s_node_id,
                 (unsigned)CAN_TWAI_TX_GPIO,
                 (unsigned)CAN_TWAI_RX_GPIO);
        lv_label_set_text(s_label_title, t);
    }

    s_label_btn1 = lv_label_create(lv_screen_active());
    lv_obj_set_style_text_font(s_label_btn1, &lv_font_montserrat_12, 0);
    lv_obj_align(s_label_btn1, LV_ALIGN_TOP_LEFT, 4, 34);
    lv_label_set_text(s_label_btn1, "B1: --");

    s_label_btn2 = lv_label_create(lv_screen_active());
    lv_obj_set_style_text_font(s_label_btn2, &lv_font_montserrat_12, 0);
    lv_obj_align(s_label_btn2, LV_ALIGN_TOP_LEFT, 4, 52);
    lv_label_set_text(s_label_btn2, "B2: --");

    s_label_can_peer = lv_label_create(lv_screen_active());
    lv_obj_set_style_text_font(s_label_can_peer, &lv_font_montserrat_12, 0);
    lv_label_set_long_mode(s_label_can_peer, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_label_can_peer, 304);
    lv_obj_align(s_label_can_peer, LV_ALIGN_TOP_LEFT, 4, 70);
    lv_label_set_text(s_label_can_peer, "Peer: waiting...");

    s_label_can_counters = lv_label_create(lv_screen_active());
    lv_obj_set_style_text_font(s_label_can_counters, &lv_font_montserrat_12, 0);
    lv_label_set_long_mode(s_label_can_counters, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_label_can_counters, 304);
    lv_obj_align(s_label_can_counters, LV_ALIGN_TOP_LEFT, 4, 102);
    lv_label_set_text(s_label_can_counters, "TWAI ..\nCounters ..");
    tdisplays3_display_unlock();

    ESP_ERROR_CHECK(tdisplays3_button_register_callback(TDISPLAYS3_BUTTON_1, button1_event_cb));
    ESP_ERROR_CHECK(tdisplays3_button_register_callback(TDISPLAYS3_BUTTON_2, button2_event_cb));

    xTaskCreate(can_rx_task, "can_rx", 4096, NULL, 5, NULL);
    xTaskCreate(can_tx_task, "can_tx", 4096, NULL, 4, NULL);

    flush_ui_summary();
}
