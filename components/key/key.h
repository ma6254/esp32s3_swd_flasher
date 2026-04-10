#ifndef USER_KEY_H
#define USER_KEY_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "key_core.h"

typedef struct GLOBAL_KEY_T
{
    key_handle_t burn_key;
    QueueHandle_t event_queue;
} global_key_t;

extern global_key_t g_key;

esp_err_t key_init(void);
esp_err_t key_task_handler(void);

#endif // USER_KEY_H
