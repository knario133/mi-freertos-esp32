/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "shared_data.h"

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SYSTEM_MANAGER_STACK_WORDS    4096
#define SYSTEM_MANAGER_PRIORITY        (configMAX_PRIORITIES - 3)
#define SYSTEM_MANAGER_CORE            0
#define SYSTEM_MANAGER_RATE_HZ         1000

static TaskHandle_t s_system_task;

static void system_manager_task(void *arg);

BaseType_t system_manager_start(intercore_shared_data_t *shared)
{
    if ((shared == NULL) || (s_system_task != NULL)) {
        return pdFAIL;
    }

    return xTaskCreatePinnedToCore(system_manager_task,
                                   "system_manager",
                                   SYSTEM_MANAGER_STACK_WORDS,
                                   shared,
                                   SYSTEM_MANAGER_PRIORITY,
                                   &s_system_task,
                                   SYSTEM_MANAGER_CORE);
}

static void system_manager_task(void *arg)
{
    intercore_shared_data_t *shared = (intercore_shared_data_t *)arg;
    system_data_sample_t cmd = { .value = 0, .sequence = 0 };
    TickType_t last_wake = xTaskGetTickCount();

    while (true) {
        cmd.sequence++;
        cmd.value = (int32_t)(cmd.sequence & 0x3FFU);
        (void)system_data_push(shared, SYSTEM_DATA_SETPOINT, &cmd);

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1000 / SYSTEM_MANAGER_RATE_HZ));
    }
}
