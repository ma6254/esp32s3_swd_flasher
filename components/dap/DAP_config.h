#ifndef __DAP_CONFIG_H__
#define __DAP_CONFIG_H__

#define CPU_CLOCK 100000UL ///< Specifies the CPU Clock in Hz (ESP32-S3)

#define IO_PORT_WRITE_CYCLES 2 ///< I/O Cycles: 2=default, 1=Cortex-M0+ fast I/0

#define DAP_SWD 1 ///< SWD Mode:  1 = available, 0 = not available

#define DAP_JTAG 0 ///< JTAG Mode: 0 = not available

#define DAP_JTAG_DEV_CNT 8 ///< Maximum number of JTAG devices on scan chain

#define DAP_DEFAULT_PORT 1 ///< Default JTAG/SWJ Port Mode: 1 = SWD, 2 = JTAG.

#define DAP_DEFAULT_SWJ_CLOCK 400 ///< Default SWD/JTAG clock frequency in Hz.

/// Maximum Package Size for Command and Response data.
#define DAP_PACKET_SIZE 64 ///< USB: 64 = Full-Speed, 1024 = High-Speed.

/// Maximum Package Buffers for Command and Response data.
#define DAP_PACKET_COUNT 1 ///< Buffers: 64 = Full-Speed, 4 = High-Speed.

/// Indicate that UART Serial Wire Output (SWO) trace is available.
#define SWO_UART 0 ///< SWO UART:  1 = available, 0 = not available

#define SWO_UART_MAX_BAUDRATE 10000000U ///< SWO UART Maximum Baudrate in Hz

/// Indicate that Manchester Serial Wire Output (SWO) trace is available.
#define SWO_MANCHESTER 0 ///< SWO Manchester:  1 = available, 0 = not available

#define SWO_BUFFER_SIZE 4096U ///< SWO Trace Buffer Size in bytes (must be 2^n)

/// Debug Unit is connected to fixed Target Device.
#define TARGET_DEVICE_FIXED 0 ///< Target Device: 1 = known, 0 = unknown;

//**************************************************************************************************
/**
JTAG I/O Pin                 | SWD I/O Pin          | CMSIS-DAP Hardware pin mode
---------------------------- | -------------------- | ---------------------------------------------
TCK: Test Clock              | SWCLK: Clock         | Output Push/Pull
TMS: Test Mode Select        | SWDIO: Data I/O      | Output Push/Pull; Input (for receiving data)
TDI: Test Data Input         |                      | Output Push/Pull
TDO: Test Data Output        |                      | Input
nTRST: Test Reset (optional) |                      | Output Open Drain with pull-up resistor
nRESET: Device Reset         | nRESET: Device Reset  | Output Open Drain with pull-up resistor

DAP Hardware I/O Pin Access Functions
*/
#include <driver/gpio.h>
#include <soc/gpio_struct.h>
#include "board.h"

// Configure DAP I/O pins ------------------------------

#define PIN_SWCLK TARGET_SWCLK_GPIO_PIN
#define PIN_SWDIN TARGET_SWDIN_GPIO_PIN
#define PIN_SWDOUT TARGET_SWDOUT_GPIO_PIN
#define PIN_nRESET TARGET_NRESET_GPIO_PIN

// Whether SWDIN and SWDOUT share the same GPIO (half-duplex)
#define PIN_SWDIO_SHARED (PIN_SWDIN == PIN_SWDOUT)

// Bitmasks for direct register access (all pins < 32)
#define PIN_SWCLK_MASK (1UL << PIN_SWCLK)
#define PIN_SWDIN_MASK (1UL << PIN_SWDIN)
#define PIN_SWDOUT_MASK (1UL << PIN_SWDOUT)
#define PIN_nRESET_MASK (1UL << PIN_nRESET)

/** Setup JTAG I/O pins: TCK, TMS, TDI, TDO, nTRST, and nRESET.
 - TCK, TMS, TDI, nTRST, nRESET to output mode and set to high level.
 - TDO to input mode.
*/
static void PORT_JTAG_SETUP(void) {
#if (DAP_JTAG != 0)
#endif
}

/** Setup SWD I/O pins: SWCLK, SWDIO, and nRESET.
 - SWCLK, SWDIO, nRESET to output mode and set to default high level.
*/
static void PORT_SWD_SETUP(void) {
  gpio_set_level(PIN_SWCLK, 1);
  gpio_set_level(PIN_SWDOUT, 1);

  gpio_set_direction(PIN_SWCLK, GPIO_MODE_OUTPUT);
#if PIN_SWDIO_SHARED
  gpio_set_direction(PIN_SWDOUT, GPIO_MODE_INPUT_OUTPUT);
#else
  gpio_set_direction(PIN_SWDOUT, GPIO_MODE_INPUT_OUTPUT);
  gpio_set_direction(PIN_SWDIN, GPIO_MODE_INPUT);
#endif
}

/** Disable JTAG/SWD I/O Pins.
 - TCK/SWCLK, TMS/SWDIO, TDI, TDO, nTRST, nRESET to High-Z mode.
*/
static void PORT_OFF(void) {
  gpio_set_direction(PIN_SWCLK, GPIO_MODE_DISABLE);
  gpio_set_direction(PIN_SWDOUT, GPIO_MODE_DISABLE);
#if !PIN_SWDIO_SHARED
  gpio_set_direction(PIN_SWDIN, GPIO_MODE_DISABLE);
#endif
}

// SWCLK/TCK I/O pin -------------------------------------

static __inline uint32_t PIN_SWCLK_TCK_IN(void) {
  return (GPIO.in >> PIN_SWCLK) & 1U;
}

static __inline void PIN_SWCLK_TCK_SET(void) {
  GPIO.out_w1ts = PIN_SWCLK_MASK;
}

static __inline void PIN_SWCLK_TCK_CLR(void) {
  GPIO.out_w1tc = PIN_SWCLK_MASK;
}

// SWDIO/TMS Pin I/O --------------------------------------

static __inline uint32_t PIN_SWDIO_TMS_IN(void) {
  return (GPIO.in >> PIN_SWDIN) & 1U;
}

static __inline void PIN_SWDIO_TMS_SET(void) {
  GPIO.out_w1ts = PIN_SWDOUT_MASK;
}

static __inline void PIN_SWDIO_TMS_CLR(void) {
  GPIO.out_w1tc = PIN_SWDOUT_MASK;
}

static __inline uint32_t PIN_SWDIO_IN(void) {
  return (GPIO.in >> PIN_SWDIN) & 1U;
}

static __inline void PIN_SWDIO_OUT(uint32_t bit) {
  if (bit & 1)
    GPIO.out_w1ts = PIN_SWDOUT_MASK;
  else
    GPIO.out_w1tc = PIN_SWDOUT_MASK;
}

static __inline void PIN_SWDIO_OUT_ENABLE(void) {
#if PIN_SWDIO_SHARED
  gpio_set_direction(PIN_SWDOUT, GPIO_MODE_INPUT_OUTPUT);
#endif
  GPIO.enable_w1ts = PIN_SWDOUT_MASK;
}

static __inline void PIN_SWDIO_OUT_DISABLE(void) {
  GPIO.enable_w1tc = PIN_SWDOUT_MASK;
#if PIN_SWDIO_SHARED
  gpio_set_direction(PIN_SWDIN, GPIO_MODE_INPUT);
#endif
}

// TDI Pin I/O ---------------------------------------------

static __inline uint32_t PIN_TDI_IN(void) {
#if (DAP_JTAG != 0)
#endif
  return 0;
}

static __inline void PIN_TDI_OUT(uint32_t bit) {
#if (DAP_JTAG != 0)
#endif
}

// TDO Pin I/O ---------------------------------------------

static __inline uint32_t PIN_TDO_IN(void) {
#if (DAP_JTAG != 0)
#endif
  return 0;
}

// nTRST Pin I/O -------------------------------------------

static __inline uint32_t PIN_nTRST_IN(void) { return 0; }

static __inline void PIN_nTRST_OUT(uint32_t bit) {}

// nRESET Pin I/O------------------------------------------
static __inline uint32_t PIN_nRESET_IN(void) {
  return gpio_get_level(PIN_nRESET);
}

extern uint8_t swd_write_word(uint32_t addr, uint32_t val);
static __inline void PIN_nRESET_OUT(uint32_t bit) {
  gpio_set_level(PIN_nRESET, (bit & 1));
}

//**************************************************************************************************
/** Connect LED: is active when the DAP hardware is connected to a debugger
    Running LED: is active when program execution in target started
*/

static __inline void LED_CONNECTED_OUT(uint32_t bit) {
  (void)bit;
}

static __inline void LED_RUNNING_OUT(uint32_t bit) {
  (void)bit;
}

static void DAP_SETUP(void) {
  gpio_config_t io_conf;

  // SWCLK - output push/pull
  io_conf.pin_bit_mask = PIN_SWCLK_MASK;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&io_conf);

#if PIN_SWDIO_SHARED
  // SWDIO - bidirectional (shared pin) with pull-up
  io_conf.pin_bit_mask = PIN_SWDOUT_MASK;
  io_conf.mode = GPIO_MODE_INPUT_OUTPUT;
  io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
  gpio_config(&io_conf);
#else
  // SWDIN - input with pull-up
  io_conf.pin_bit_mask = PIN_SWDIN_MASK;
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
  gpio_config(&io_conf);

  // SWDOUT - output with pull-up
  io_conf.pin_bit_mask = PIN_SWDOUT_MASK;
  io_conf.mode = GPIO_MODE_INPUT_OUTPUT;
  io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
  gpio_config(&io_conf);
#endif

  // nRESET - open drain with pull-up
  io_conf.pin_bit_mask = PIN_nRESET_MASK;
  io_conf.mode = GPIO_MODE_INPUT_OUTPUT_OD;
  io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  gpio_config(&io_conf);
  gpio_set_level(PIN_nRESET, 1);

  PORT_OFF();
}

static uint32_t RESET_TARGET(void) {
  return (0); // change to '1' when a device reset sequence is implemented
}

#endif // __DAP_CONFIG_H__
