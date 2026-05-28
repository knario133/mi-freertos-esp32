#include "shared_data.h"

#include <stdatomic.h>

static _Atomic uint32_t s_system_flags;
static _Atomic uint32_t s_fast_counter;

static inline bool channel_is_valid(system_data_channel_t channel)
{
    return (channel >= SYSTEM_DATA_SETPOINT) && (channel < SYSTEM_DATA_CHANNEL_COUNT);
}

bool intercore_shared_data_init(intercore_shared_data_t *shared)
{
    if (shared == NULL) {
        return false;
    }

    for (int i = 0; i < SYSTEM_DATA_CHANNEL_COUNT; ++i) {
        shared->mailbox[i] = xQueueCreate(1, sizeof(system_data_sample_t));
        if (shared->mailbox[i] == NULL) {
            return false;
        }
    }

    atomic_store_explicit(&s_system_flags, 0, memory_order_relaxed);
    atomic_store_explicit(&s_fast_counter, 0, memory_order_relaxed);
    return true;
}

bool IRAM_ATTR system_data_push(intercore_shared_data_t *shared,
                                system_data_channel_t channel,
                                const system_data_sample_t *sample)
{
    if ((shared == NULL) || (sample == NULL) || !channel_is_valid(channel)) {
        return false;
    }

    return (xQueueOverwrite(shared->mailbox[channel], sample) == pdPASS);
}

bool IRAM_ATTR system_data_push_from_isr(intercore_shared_data_t *shared,
                                         system_data_channel_t channel,
                                         const system_data_sample_t *sample,
                                         BaseType_t *pxHigherPriorityTaskWoken)
{
    if ((shared == NULL) || (sample == NULL) || !channel_is_valid(channel)) {
        return false;
    }

    return (xQueueOverwriteFromISR(shared->mailbox[channel], sample, pxHigherPriorityTaskWoken) == pdPASS);
}

bool system_data_pull(intercore_shared_data_t *shared,
                      system_data_channel_t channel,
                      system_data_sample_t *sample,
                      TickType_t timeout_ticks)
{
    if ((shared == NULL) || (sample == NULL) || !channel_is_valid(channel)) {
        return false;
    }

    return (xQueueReceive(shared->mailbox[channel], sample, timeout_ticks) == pdPASS);
}

bool system_flag_set(uint32_t mask)
{
    (void)atomic_fetch_or_explicit(&s_system_flags, mask, memory_order_release);
    return true;
}

bool system_flag_clear(uint32_t mask)
{
    (void)atomic_fetch_and_explicit(&s_system_flags, ~mask, memory_order_release);
    return true;
}

bool system_flag_is_set(uint32_t mask)
{
    uint32_t flags = atomic_load_explicit(&s_system_flags, memory_order_acquire);
    return (flags & mask) != 0;
}

uint32_t system_counter_increment(void)
{
    return atomic_fetch_add_explicit(&s_fast_counter, 1, memory_order_acq_rel) + 1;
}

uint32_t system_counter_get(void)
{
    return atomic_load_explicit(&s_fast_counter, memory_order_acquire);
}
