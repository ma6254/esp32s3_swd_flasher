#ifndef __SWD_FLASH_H__
#define __SWD_FLASH_H__

#include <stdint.h>
#include <dap_err.h>

dap_err_t target_flash_init(uint32_t flash_start);
dap_err_t target_flash_uninit(void);
dap_err_t target_flash_program_page(uint32_t addr, const uint8_t *buf, uint32_t size);
dap_err_t target_flash_erase_sector(uint32_t addr);
dap_err_t target_flash_erase_chip(void);


#endif // __SWD_FLASH_H__
