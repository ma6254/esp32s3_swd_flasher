#include <string.h>
#include <esp_log.h>
#include "board.h"

static const char *TAG = "board";
global_board_t g_board;

/*******************************************************************************
 * @brief 板级初始化
 * @param None
 * @return None
 ******************************************************************************/
void board_init(void)
{
    esp_err_t err;

    memset(&g_board, 0, sizeof(g_board));

    // 1. LED灯带通用配置
    led_strip_config_t strip_config = {
        .strip_gpio_num = WS2812_LED_GPIO,                           // 数据引脚
        .max_leds = 1,                                               // 灯珠数量
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, // 颜色顺序，WS2812一般是GRB
        .flags.invert_out = false,                                   // 如果你的电路有反相器，设为true
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .mem_block_symbols = 0,            // 0=默认
        .flags.with_dma = false,
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &g_board.led_strip));

    // 清空所有灯珠（全部熄灭）
    set_led_color(0x00, 0x00, 0x00);
    // ESP_ERROR_CHECK(led_strip_clear(g_board.led_strip));
    ESP_LOGI(TAG, "LED初始化完成");
}

void set_led_color(uint8_t r, uint8_t g, uint8_t b)
{
    ESP_ERROR_CHECK(led_strip_set_pixel(g_board.led_strip, 0, r, g, b));
    ESP_ERROR_CHECK(led_strip_refresh(g_board.led_strip));
}
