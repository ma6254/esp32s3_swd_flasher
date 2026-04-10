#ifndef KEY_CORE_H
#define KEY_CORE_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_log.h>
#include <esp_err.h>

#define KEY_EVENT_QUEUE_SIZE (10) // 按键事件队列大小

typedef struct KEY_HANDLE_T key_handle_t;

typedef bool (*key_read_state_cb_t)(void *user_data);


typedef enum KEY_EVENT_CODE_T
{
    KEY_EVENT_NONE = 0,
    KEY_EVENT_PRESSED, // 按键按下事件
    KEY_EVENT_RELEASED, // 按键松开事件
    KEY_EVENT_LONG,     // 按键长按事件
} key_event_code_t;

typedef struct KEY_HANDLE_EVENT_T
{
    key_event_code_t code; // 事件类型
    void *user_data;             // 事件数据指针
} key_handle_event_t;

typedef void (*key_event_cb_t)(key_handle_event_t *event);

typedef struct KEY_HANDLE_CFG_T
{
    const char *name;                  // 按键名称
    key_read_state_cb_t read_state_cb; // 回调函数：读取按键
    void *read_state_cb_user_data;     // 回调函数用户参数
    QueueHandle_t event_queue;         // 按键事件队列
} key_handle_cfg_t;

typedef struct KEY_HANDLE_T
{
    struct
    {
        uint32_t not_first : 1;  // 是否是第一次读取按键状态
        uint32_t last_state : 1; // 上一次的按键状态
        uint32_t reserved : 30;  // 保留位
    };

    key_handle_cfg_t cfg;

    uint8_t filt;
    QueueHandle_t event_queue;

} key_handle_t;

esp_err_t key_handle_init(key_handle_t *handle, const key_handle_cfg_t *cfg);
esp_err_t key_handle_process(key_handle_t *handle);

#endif // KEY_CORE_H
