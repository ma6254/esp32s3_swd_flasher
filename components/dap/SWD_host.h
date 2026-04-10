#ifndef SWDHOST_CM_H
#define SWDHOST_CM_H

#include <stdint.h>

typedef enum {
    RESET_HOLD,              // Hold target in reset
    RESET_PROGRAM,           // Reset target and setup for flash programming.
    RESET_RUN,               // Reset target and run normally
    NO_DEBUG,                // Disable debug on running target
    DEBUG,                   // Enable debug on running target
    HALT,                    // Halt the target without resetting it
    RUN                      // Resume the target without resetting it
} TARGET_RESET_STATE;

typedef struct
{
    uint32_t breakpoint;
    uint32_t static_base;
    uint32_t stack_pointer;
} program_syscall_t;

typedef struct
{
    const uint32_t init;
    const uint32_t uninit;
    const uint32_t erase_chip;
    const uint32_t erase_sector;
    const uint32_t program_page;
    const program_syscall_t sys_call_s;
    const uint32_t program_buffer;
    const uint32_t algo_start;
    const uint32_t algo_size;
    const uint32_t *algo_blob;
    const uint32_t program_buffer_size;
} program_target_t;


uint8_t swd_init(void);
uint8_t swd_off(void);
uint8_t swd_init_debug(void);
uint8_t swd_read_idcode(uint32_t *id);

uint8_t swd_read_dp(uint8_t adr, uint32_t *val);
uint8_t swd_write_dp(uint8_t adr, uint32_t val);
uint8_t swd_read_ap(uint32_t adr, uint32_t *val);
uint8_t swd_write_ap(uint32_t adr, uint32_t val);
uint8_t swd_read_memory(uint32_t address, uint8_t *data, uint32_t size);
uint8_t swd_write_memory(uint32_t address, uint8_t *data, uint32_t size);
uint8_t swd_flash_syscall_exec(const program_syscall_t *sysCallParam, uint32_t entry, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4);
void swd_set_target_reset(uint8_t asserted);
uint8_t swd_set_target_state_hw(TARGET_RESET_STATE state);
uint8_t swd_set_target_state_sw(TARGET_RESET_STATE state);

uint8_t swd_read_word(uint32_t addr, uint32_t *val);
uint8_t swd_write_word(uint32_t addr, uint32_t val);

uint8_t swd_detect(void);

#endif
