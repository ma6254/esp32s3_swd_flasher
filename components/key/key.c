#include <string.h>
#include <driver/gpio.h>
#include "board.h"
#include "key.h"

static const char *TAG = "key_user";

global_key_t g_key;

/*******************************************************************************
 * @brief 【内部回调函数】读取按键状态
 * @param None
 * @return None
 ******************************************************************************/
static bool call_key_read_state_cb(void *user_data)
{
    int level = gpio_get_level(BURN_KEY_GPIO_PIN);

    return (level == 0); // 按下时返回true
}

/*******************************************************************************
 * @brief 按键初始化
 * @param None
 * @return None
 ******************************************************************************/
esp_err_t key_init(void)
{
    esp_err_t err;

    memset(&g_key, 0, sizeof(g_key));

    g_key.event_queue = xQueueCreate(KEY_EVENT_QUEUE_SIZE, sizeof(key_handle_event_t));
    if (g_key.event_queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create key event queue");
        return ESP_FAIL;
    }

    // 引脚配置为输入，内部上拉
    gpio_config_t burn_key_gpio_conf = {
        .pin_bit_mask = 1ULL << BURN_KEY_GPIO_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    err = gpio_config(&burn_key_gpio_conf);
    ESP_ERROR_CHECK(err);

    key_handle_cfg_t burn_key_cfg = {
        .name = "burn_key",
        .read_state_cb = call_key_read_state_cb,
        .read_state_cb_user_data = NULL,
        .event_queue = g_key.event_queue,
    };

    err = key_handle_init(&g_key.burn_key, &burn_key_cfg);
    ESP_ERROR_CHECK(err);

    return ESP_OK;
}

/*******************************************************************************
 * @brief 按键任务处理，在主循环中调用
 * @param None
 * @return None
 ******************************************************************************/
esp_err_t key_task_handler(void)
{
    esp_err_t err;

    err = key_handle_process(&g_key.burn_key);
    ESP_ERROR_CHECK(err);

    return ESP_OK;
}
