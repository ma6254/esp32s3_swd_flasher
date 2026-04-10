#include <string.h>
#include "key.h"

static const char *TAG = "key_core"; 

/*******************************************************************************
 * @brief 按键实例初始化
 * @param None
 * @return None
 ******************************************************************************/
esp_err_t key_handle_init(key_handle_t *handle, const key_handle_cfg_t *cfg)
{
    if (handle == NULL)
        return ESP_ERR_INVALID_ARG;

    memset(handle, 0, sizeof(key_handle_t));

    if(cfg)
    {
        handle->cfg = *cfg;
    }

    if(cfg->event_queue == NULL)
    {
        handle->event_queue = xQueueCreate(KEY_EVENT_QUEUE_SIZE, sizeof(key_handle_event_t));
        if (handle->event_queue == NULL)
        {
            ESP_LOGE(TAG, "Failed to create key event queue");
            return ESP_FAIL;
        }
    }
    else
    {
        handle->event_queue = cfg->event_queue;
    }

    return ESP_OK;
}

/*******************************************************************************
 * @brief 按键实例处理函数，在主循环中调用
 * @param None
 * @return None
 ******************************************************************************/
esp_err_t key_handle_process(key_handle_t *handle)
{
    if (handle == NULL)
        return ESP_ERR_INVALID_ARG;

    if (handle->cfg.read_state_cb == NULL)
        return ESP_ERR_INVALID_ARG;

    bool read_state = handle->cfg.read_state_cb(handle->cfg.read_state_cb_user_data);

    handle->filt <<= 1;
    if(read_state)
        handle->filt |= 1;

    bool curr_state;

    if(handle->filt == 0x00)
        curr_state = false;
    else if (handle->filt == 0xFF)
        curr_state = true;
    else
        // 状态不稳定，继续等待
        return ESP_OK;

    // 如果状态发生变化，或者是第一次读取状态
    if ((handle->not_first == false) || (handle->last_state != curr_state))
    {
        if((handle->last_state==false) && (curr_state==true))
        {
            // 按键按下事件
            key_handle_event_t event = {
                .code = KEY_EVENT_PRESSED,
                .user_data = handle->cfg.read_state_cb_user_data,
            };

            if (xQueueSend(handle->event_queue, &event, 0) != pdPASS)
            {
                ESP_LOGW(TAG, "Failed to send key event to queue");
            }

            ESP_LOGI(TAG, "pressed");
        }
        else if((handle->last_state==true) && (curr_state==false))
        {
            // 按键释放事件
            key_handle_event_t event = {
                .code = KEY_EVENT_RELEASED,
                .user_data = handle->cfg.read_state_cb_user_data,
            };

            if (xQueueSend(handle->event_queue, &event, 0) != pdPASS)
            {
                ESP_LOGW(TAG, "Failed to send key event to queue");
            }

            ESP_LOGI(TAG, "released");
        }
    }
    
    handle->not_first = true;
    handle->last_state = curr_state;

    return ESP_OK;
}
