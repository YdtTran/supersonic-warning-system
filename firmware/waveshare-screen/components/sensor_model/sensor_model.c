/*
 * SPDX-FileCopyrightText: 2026 Vehicle Warning System
 * SPDX-License-Identifier: MIT
 */

#include "sensor_model.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define SENSOR_ZONE_SAFE_THRESHOLD_CM 100
#define SENSOR_ZONE_DANGER_THRESHOLD_CM 30

static SemaphoreHandle_t s_mutex = NULL;
static sensor_reading_t s_readings[SENSOR_MODEL_COUNT];

static const int16_t k_offsets_deg[SENSOR_MODEL_COUNT] = {
    [SENSOR_ID_FRONT] = 0,
    [SENSOR_ID_REAR] = 180,
    [SENSOR_ID_LEFT_FRONT] = -90,
    [SENSOR_ID_LEFT_REAR] = -90,
    [SENSOR_ID_RIGHT_FRONT] = 90,
    [SENSOR_ID_RIGHT_REAR] = 90,
};

void sensor_model_init(void)
{
    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutex();
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (int i = 0; i < SENSOR_MODEL_COUNT; i++) {
        s_readings[i].distance_cm = 0;
        s_readings[i].offset_deg = k_offsets_deg[i];
        s_readings[i].is_stale = true;
    }
    xSemaphoreGive(s_mutex);
}

void sensor_model_set_distance(sensor_id_t id, uint16_t distance_cm)
{
    if (id >= SENSOR_MODEL_COUNT || s_mutex == NULL) {
        return;
    }

    if (distance_cm > SENSOR_RANGE_MAX_CM) {
        distance_cm = SENSOR_RANGE_MAX_CM;
    } else if (distance_cm != 0 && distance_cm < SENSOR_RANGE_MIN_CM) {
        distance_cm = SENSOR_RANGE_MIN_CM;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_readings[id].distance_cm = distance_cm;
    s_readings[id].is_stale = false;
    xSemaphoreGive(s_mutex);
}

sensor_reading_t sensor_model_get(sensor_id_t id)
{
    sensor_reading_t reading = {0};
    if (id >= SENSOR_MODEL_COUNT || s_mutex == NULL) {
        return reading;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    reading = s_readings[id];
    xSemaphoreGive(s_mutex);
    return reading;
}

void sensor_model_get_all(sensor_reading_t out[SENSOR_MODEL_COUNT])
{
    if (out == NULL || s_mutex == NULL) {
        return;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (int i = 0; i < SENSOR_MODEL_COUNT; i++) {
        out[i] = s_readings[i];
    }
    xSemaphoreGive(s_mutex);
}

sensor_zone_t sensor_model_classify(uint16_t distance_cm)
{
    if (distance_cm < SENSOR_ZONE_DANGER_THRESHOLD_CM) {
        return SENSOR_ZONE_DANGER;
    }
    if (distance_cm <= SENSOR_ZONE_SAFE_THRESHOLD_CM) {
        return SENSOR_ZONE_CAUTION;
    }
    return SENSOR_ZONE_SAFE;
}
