/*
 * SPDX-FileCopyrightText: 2026 Vehicle Warning System
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SENSOR_MODEL_COUNT 6

// JSN-SR04T datasheet: 75 deg beam aperture, 20cm - 600cm effective range.
#define SENSOR_RANGE_MIN_CM 20
#define SENSOR_RANGE_MAX_CM 600
#define SENSOR_BEAM_FOV_DEG 75

// Layout mirrors the standard truck "No-Zone" blind-spot diagram:
// one sensor centered on the front, one centered on the rear, and two
// sensors per side (covering the front-half and rear-half of that side).
typedef enum {
    SENSOR_ID_FRONT = 0,      // Zone 1 (front), offset 0 deg
    SENSOR_ID_REAR = 1,       // Zone 2 (rear), offset 180 deg
    SENSOR_ID_LEFT_FRONT = 2, // Zone 3 (left side, front half), offset -90 deg
    SENSOR_ID_LEFT_REAR = 3,  // Zone 3 (left side, rear half), offset -90 deg
    SENSOR_ID_RIGHT_FRONT = 4, // Zone 4 (right side, front half), offset +90 deg
    SENSOR_ID_RIGHT_REAR = 5,  // Zone 4 (right side, rear half), offset +90 deg
} sensor_id_t;

typedef enum {
    SENSOR_ZONE_SAFE = 0,   // > 100cm
    SENSOR_ZONE_CAUTION = 1, // 30-100cm
    SENSOR_ZONE_DANGER = 2,  // < 30cm
} sensor_zone_t;

typedef struct {
    uint16_t distance_cm;
    int16_t offset_deg;
    bool is_stale;
} sensor_reading_t;

/**
 * @brief Initialize the thread-safe sensor model (mutex + default values).
 */
void sensor_model_init(void);

/**
 * @brief Update the distance reading for a single sensor. Thread-safe.
 */
void sensor_model_set_distance(sensor_id_t id, uint16_t distance_cm);

/**
 * @brief Read a single sensor's current reading. Thread-safe.
 */
sensor_reading_t sensor_model_get(sensor_id_t id);

/**
 * @brief Snapshot all 6 sensor readings into the provided array. Thread-safe.
 */
void sensor_model_get_all(sensor_reading_t out[SENSOR_MODEL_COUNT]);

/**
 * @brief Classify a distance reading into a hazard zone using the standard thresholds.
 */
sensor_zone_t sensor_model_classify(uint16_t distance_cm);

#ifdef __cplusplus
}
#endif
