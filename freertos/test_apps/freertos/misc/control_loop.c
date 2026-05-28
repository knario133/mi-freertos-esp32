/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "shared_data.h"

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_attr.h"

#define CONTROL_TASK_STACK_WORDS      4096
#define CONTROL_TASK_PRIORITY          (configMAX_PRIORITIES - 2)
#define CONTROL_TASK_CORE              1
#define CONTROL_LOOP_PERIOD_US         1000

static TaskHandle_t s_control_task;

static void IRAM_ATTR control_loop_task(void *arg);
static inline int32_t IRAM_ATTR simulate_control_step(int32_t setpoint, int32_t feedback);

BaseType_t control_loop_start(intercore_shared_data_t *shared)
{
    if ((shared == NULL) || (s_control_task != NULL)) {
        return pdFAIL;
    }

    return xTaskCreatePinnedToCore(control_loop_task,
                                   "control_loop",
                                   CONTROL_TASK_STACK_WORDS,
                                   shared,
                                   CONTROL_TASK_PRIORITY,
                                   &s_control_task,
                                   CONTROL_TASK_CORE);
}

static inline int32_t IRAM_ATTR simulate_control_step(int32_t setpoint, int32_t feedback)
{
    int32_t error = setpoint - feedback;
    return feedback + (error / 2);
}

static void IRAM_ATTR control_loop_task(void *arg)
{
    intercore_shared_data_t *shared = (intercore_shared_data_t *)arg;
    system_data_sample_t setpoint = { 0 };
    system_data_sample_t state = { 0 };
    int32_t feedback = 0;
    TickType_t last_wake = xTaskGetTickCount();

    while (true) {
        (void)system_data_pull(shared, SYSTEM_DATA_SETPOINT, &setpoint, 0);
        feedback = simulate_control_step(setpoint.value, feedback);

        state.value = feedback;
        state.sequence = setpoint.sequence;
        (void)system_data_push(shared, SYSTEM_DATA_SENSOR_SNAPSHOT, &state);

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(CONTROL_LOOP_PERIOD_US / 1000));
    }
}
