#pragma once

#include "freertos/FreeRTOS.h"

#include "shared_data.h"

#ifdef __cplusplus
extern "C" {
#endif

BaseType_t control_loop_start(intercore_shared_data_t *shared);
BaseType_t system_manager_start(intercore_shared_data_t *shared);

#ifdef __cplusplus
}
#endif
