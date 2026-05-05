#pragma once

#include "esp_err.h"

typedef struct {
    float roll_deg;
    float pitch_deg;
    float yaw_deg;
} imu_rpy_t;

esp_err_t imu_init(void);
esp_err_t imu_read_rpy(imu_rpy_t *out_rpy);
