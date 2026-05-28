/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "shared_data.h"

#include <inttypes.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "unity.h"

#include "control_tasks.h"

static const char *TAG = "stress_test";

TEST_CASE("stress inter-core data path without mutex", "[freertos][stress]")
{
    intercore_shared_data_t shared;
    TEST_ASSERT_TRUE(intercore_shared_data_init(&shared));
    TEST_ASSERT_EQUAL(pdPASS, system_manager_start(&shared));
    TEST_ASSERT_EQUAL(pdPASS, control_loop_start(&shared));

    uint32_t total_samples = 0;
    uint32_t missed_seq = 0;
    uint32_t expected_seq = 0;
    int64_t last_ts = esp_timer_get_time();
    int64_t min_jitter_us = INT64_MAX;
    int64_t max_jitter_us = 0;

    while (total_samples < 10000) {
        system_data_sample_t state = { 0 };
        if (system_data_pull(&shared, SYSTEM_DATA_SENSOR_SNAPSHOT, &state, pdMS_TO_TICKS(2))) {
            int64_t now = esp_timer_get_time();
            int64_t elapsed = now - last_ts;
            int64_t jitter = elapsed - 1000;
            if (jitter < min_jitter_us) {
                min_jitter_us = jitter;
            }
            if (jitter > max_jitter_us) {
                max_jitter_us = jitter;
            }
            last_ts = now;

            if ((expected_seq != 0U) && (state.sequence > (expected_seq + 1U))) {
                missed_seq += (state.sequence - expected_seq - 1U);
            }
            expected_seq = state.sequence;
            total_samples++;

            if ((total_samples % 1000U) == 0U) {
                ESP_LOGI(TAG, "samples=%" PRIu32 " seq=%" PRIu32 " jitter[min,max]=[%" PRId64 ",%" PRId64 "] us",
                         total_samples, state.sequence, min_jitter_us, max_jitter_us);
            }
        } else {
            taskYIELD();
        }
    }

    ESP_LOGI(TAG, "done samples=%" PRIu32 " missed=%" PRIu32, total_samples, missed_seq);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(50, missed_seq);
}
