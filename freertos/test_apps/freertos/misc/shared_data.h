#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_attr.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SYSTEM_DATA_SETPOINT = 0,
    SYSTEM_DATA_SENSOR_SNAPSHOT,
    SYSTEM_DATA_CHANNEL_COUNT,
} system_data_channel_t;

typedef struct {
    int32_t value;
    uint32_t sequence;
} system_data_sample_t;

typedef struct {
    QueueHandle_t mailbox[SYSTEM_DATA_CHANNEL_COUNT];
} intercore_shared_data_t;

bool intercore_shared_data_init(intercore_shared_data_t *shared);

bool IRAM_ATTR system_data_push(intercore_shared_data_t *shared,
                                system_data_channel_t channel,
                                const system_data_sample_t *sample);

bool IRAM_ATTR system_data_push_from_isr(intercore_shared_data_t *shared,
                                         system_data_channel_t channel,
                                         const system_data_sample_t *sample,
                                         BaseType_t *pxHigherPriorityTaskWoken);

bool system_data_pull(intercore_shared_data_t *shared,
                      system_data_channel_t channel,
                      system_data_sample_t *sample,
                      TickType_t timeout_ticks);

bool system_flag_set(uint32_t mask);
bool system_flag_clear(uint32_t mask);
bool system_flag_is_set(uint32_t mask);
uint32_t system_counter_increment(void);
uint32_t system_counter_get(void);

#ifdef __cplusplus
}
#endif
