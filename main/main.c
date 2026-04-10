/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include <esp_task_wdt.h>
#include <esp_chip_info.h>
#include <esp_flash.h>
#include <esp_system.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "board.h"
#include "DAP.h"
#include "DAP_config.h"
#include "SWD_flash.h"
#include "SWD_host.h"

static const char *TAG = "main";

/*******************************************************************************
 * @brief 示例任务：定期发送BLE通知
 * @param pvParameters 任务参数（未使用）
 * @return None
 ******************************************************************************/
void test_task(void *pvParameters)
{
    uint32_t idcode;

    ESP_LOGI(TAG, "test task started");

    // 关闭 gpio 组件的 INFO 级日志，避免每次 gpio_config 都打印
    esp_log_level_set("gpio", ESP_LOG_WARN);

    // SWD bit-banging 会长时间占用 CPU，取消当前核心 IDLE 任务的看门狗监控
    // esp_task_wdt_delete(xTaskGetIdleTaskHandleForCore(xPortGetCoreID()));
    // esp_task_wdt_add(xTaskGetIdleTaskHandleForCore(xPortGetCoreID()));

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));

        uint8_t is_detected = swd_detect();
        
        if(is_detected) {
            set_led_color(0xFF, 0x00, 0x00); // 设置为红色

            swd_read_idcode(&idcode);

            ESP_LOGI(TAG, "SWD device detected, IDCODE: 0x%08" PRIX32, idcode);
        }
        else
        {
            set_led_color(0x00, 0x00, 0xFF); // 设置为蓝色
        }
    }
}

void app_main(void)
{
    printf("Hello world!\n");

    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), %s%s%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    printf("silicon revision v%d.%d, ", major_rev, minor_rev);
    if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        printf("Get flash size failed");
        return;
    }

    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

    board_init();

    xTaskCreate(test_task, "test_task", 4096, NULL, 5, NULL);

    // while (1)
    // {
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    // }
}
