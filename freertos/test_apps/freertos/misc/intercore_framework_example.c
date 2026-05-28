#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "shared_data.h"

#define CORE_SYSTEM_COMM 0
#define CORE_CONTROL_SENSOR 1

#define PRIO_SYSTEM_COMM  (configMAX_PRIORITIES - 4)
#define PRIO_CONTROL_LOOP (configMAX_PRIORITIES - 2)

#define SYSTEM_FLAG_NEW_SETPOINT (1U << 0)

static const char *TAG = "intercore_fw";

static intercore_shared_data_t s_shared_data;

static void control_sensor_task(void *arg)
{
    intercore_shared_data_t *shared = (intercore_shared_data_t *)arg;
    system_data_sample_t sensor_sample = {0};

    for (;;) {
        sensor_sample.value += 10;
        sensor_sample.sequence = system_counter_increment();

        (void)system_data_push(shared, SYSTEM_DATA_SENSOR_SNAPSHOT, &sensor_sample);

        if (system_flag_is_set(SYSTEM_FLAG_NEW_SETPOINT)) {
            system_data_sample_t setpoint = {0};
            (void)system_data_pull(shared, SYSTEM_DATA_SETPOINT, &setpoint, 0);
            system_flag_clear(SYSTEM_FLAG_NEW_SETPOINT);
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static void system_comm_task(void *arg)
{
    intercore_shared_data_t *shared = (intercore_shared_data_t *)arg;
    system_data_sample_t setpoint = {
        .value = 1000,
        .sequence = 1,
    };

    for (;;) {
        setpoint.value += 1;
        setpoint.sequence = system_counter_increment();

        (void)system_data_push(shared, SYSTEM_DATA_SETPOINT, &setpoint);
        (void)system_flag_set(SYSTEM_FLAG_NEW_SETPOINT);

        system_data_sample_t latest_sensor = {0};
        if (system_data_pull(shared, SYSTEM_DATA_SENSOR_SNAPSHOT, &latest_sensor, pdMS_TO_TICKS(2))) {
            ESP_LOGD(TAG, "sensor=%" PRId32 " seq=%" PRIu32, latest_sensor.value, latest_sensor.sequence);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void intercore_framework_start(void)
{
    if (!intercore_shared_data_init(&s_shared_data)) {
        ESP_LOGE(TAG, "intercore_shared_data_init failed");
        return;
    }

    (void)xTaskCreatePinnedToCore(control_sensor_task,
                                  "control_sensor",
                                  4096,
                                  &s_shared_data,
                                  PRIO_CONTROL_LOOP,
                                  NULL,
                                  CORE_CONTROL_SENSOR);

    (void)xTaskCreatePinnedToCore(system_comm_task,
                                  "system_comm",
                                  4096,
                                  &s_shared_data,
                                  PRIO_SYSTEM_COMM,
                                  NULL,
                                  CORE_SYSTEM_COMM);
}
