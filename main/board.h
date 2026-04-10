#ifndef APP_BOARD_H
#define APP_BOARD_H

#include <led_strip.h>

// 目标芯片的 SWCLK 引脚，必须
#define TARGET_SWCLK_GPIO_PIN (GPIO_NUM_1) 

// 目标芯片的 SWDIO 引脚，必须
#define TARGET_SWDIN_GPIO_PIN (GPIO_NUM_2)
#define TARGET_SWDOUT_GPIO_PIN (GPIO_NUM_2)

// 目标芯片的 NRESET 引脚，非必须
#define TARGET_NRESET_GPIO_PIN (GPIO_NUM_3)

#define WS2812_LED_GPIO GPIO_NUM_48

typedef struct GLOBAL_BOARD_T
{
    led_strip_handle_t led_strip;

} global_board_t;

extern global_board_t g_board;

void board_init(void);

void set_led_color(uint8_t r, uint8_t g, uint8_t b);

#endif // APP_BOARD_H
