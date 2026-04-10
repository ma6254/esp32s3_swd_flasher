/**
 * @file    SWDd_host.c
 * @brief   Host driver for accessing the DAP
 */
#include "swd_host.h"

#include "DAP_config.h"
#include "DAP.h"
#include "debug_cm.h"

#include <esp_log.h>
#include <inttypes.h>
static const char *SWD_TAG = "swd_host";

extern uint32_t Flash_Page_Size;

#define NVIC_Addr (0xe000e000)
#define DBG_Addr (0xe000edf0)

// AP CSW register, base value
#define CSW_VALUE (CSW_RESERVED | CSW_MSTRDBG | CSW_HPROT | CSW_DBGSTAT | CSW_SADDRINC)

// SWD register access
#define SWD_REG_AP (1)
#define SWD_REG_DP (0)
#define SWD_REG_R (1 << 1)
#define SWD_REG_W (0 << 1)
#define SWD_REG_ADR(a) (a & 0x0c)

#define DCRDR 0xE000EDF8
#define DCRSR 0xE000EDF4
#define DHCSR 0xE000EDF0
#define REGWnR (1 << 16)

#define MAX_SWD_RETRY 10
#define MAX_TIMEOUT 1000000 // Timeout for syscalls on target

typedef struct
{
    uint32_t select;
    uint32_t csw;
} DAP_STATE;

typedef struct
{
    uint32_t r[16];
    uint32_t xpsr;
} DEBUG_STATE;

static DAP_STATE dap_state;

static uint8_t swd_read_core_register(uint32_t n, uint32_t *val);
static uint8_t swd_write_core_register(uint32_t n, uint32_t val);

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void delaymS(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

static void int2array(uint8_t *res, uint32_t data, uint8_t len)
{
    uint8_t i = 0;

    for (i = 0; i < len; i++)
    {
        res[i] = (data >> 8 * i) & 0xFF;
    }
}

static uint8_t swd_transfer_retry(uint32_t req, uint32_t *data)
{
    uint8_t i, ack;

    for (i = 0; i < MAX_SWD_RETRY; i++)
    {
        ack = SWD_Transfer(req, data);

        if (ack != DAP_TRANSFER_WAIT)
        {
            return ack;
        }
    }

    return ack;
}

uint8_t swd_init(void)
{
    DAP_Setup();
    PORT_SWD_SETUP();

    return 1;
}

uint8_t swd_off(void)
{
    PORT_OFF();

    return 1;
}

// Read debug port register.
uint8_t swd_read_dp(uint8_t adr, uint32_t *val)
{
    uint32_t tmp_in;
    uint8_t tmp_out[4];
    uint8_t ack;
    uint32_t tmp;

    tmp_in = SWD_REG_DP | SWD_REG_R | SWD_REG_ADR(adr);
    ack = swd_transfer_retry(tmp_in, (uint32_t *)tmp_out);

    *val = 0;
    tmp = tmp_out[3];
    *val |= (tmp << 24);
    tmp = tmp_out[2];
    *val |= (tmp << 16);
    tmp = tmp_out[1];
    *val |= (tmp << 8);
    tmp = tmp_out[0];
    *val |= (tmp << 0);

    return (ack == 0x01);
}

// Write debug port register
uint8_t swd_write_dp(uint8_t adr, uint32_t val)
{
    uint32_t req;
    uint8_t data[4];
    uint8_t ack;

    switch (adr)
    {
    case DP_SELECT:
        if (dap_state.select == val)
        {
            return 1;
        }

        dap_state.select = val;
        break;

    default:
        break;
    }

    req = SWD_REG_DP | SWD_REG_W | SWD_REG_ADR(adr);
    int2array(data, val, 4);
    ack = swd_transfer_retry(req, (uint32_t *)data);

    return (ack == 0x01);
}

// Read access port register.
uint8_t swd_read_ap(uint32_t adr, uint32_t *val)
{
    uint8_t tmp_in, ack;
    uint8_t tmp_out[4];
    uint32_t tmp;
    uint32_t apsel = adr & 0xff000000;
    uint32_t bank_sel = adr & APBANKSEL;

    if (!swd_write_dp(DP_SELECT, apsel | bank_sel))
    {
        return 0;
    }

    tmp_in = SWD_REG_AP | SWD_REG_R | SWD_REG_ADR(adr);
    // first dummy read
    swd_transfer_retry(tmp_in, (uint32_t *)tmp_out);
    ack = swd_transfer_retry(tmp_in, (uint32_t *)tmp_out);

    *val = 0;
    tmp = tmp_out[3];
    *val |= (tmp << 24);
    tmp = tmp_out[2];
    *val |= (tmp << 16);
    tmp = tmp_out[1];
    *val |= (tmp << 8);
    tmp = tmp_out[0];
    *val |= (tmp << 0);

    return (ack == 0x01);
}

// Write access port register
uint8_t swd_write_ap(uint32_t adr, uint32_t val)
{
    uint8_t data[4];
    uint8_t req, ack;
    uint32_t apsel = adr & 0xff000000;
    uint32_t bank_sel = adr & APBANKSEL;

    if (!swd_write_dp(DP_SELECT, apsel | bank_sel))
    {
        return 0;
    }

    switch (adr)
    {
    case AP_CSW:
        if (dap_state.csw == val)
        {
            return 1;
        }

        dap_state.csw = val;
        break;

    default:
        break;
    }

    req = SWD_REG_AP | SWD_REG_W | SWD_REG_ADR(adr);
    int2array(data, val, 4);

    if (swd_transfer_retry(req, (uint32_t *)data) != 0x01)
    {
        return 0;
    }

    req = SWD_REG_DP | SWD_REG_R | SWD_REG_ADR(DP_RDBUFF);
    ack = swd_transfer_retry(req, NULL);

    return (ack == 0x01);
}

// Write 32-bit word aligned values to target memory using address auto-increment.
// size is in bytes.
static uint8_t swd_write_block(uint32_t address, uint8_t *data, uint32_t size)
{
    uint8_t tmp_in[4], req;
    uint32_t size_in_words;
    uint32_t i, ack;

    if (size == 0)
    {
        return 0;
    }

    size_in_words = size / 4;

    // CSW register
    if (!swd_write_ap(AP_CSW, CSW_VALUE | CSW_SIZE32))
    {
        return 0;
    }

    // TAR write
    req = SWD_REG_AP | SWD_REG_W | (1 << 2);
    int2array(tmp_in, address, 4);

    if (swd_transfer_retry(req, (uint32_t *)tmp_in) != 0x01)
    {
        return 0;
    }

    // DRW write
    req = SWD_REG_AP | SWD_REG_W | (3 << 2);

    for (i = 0; i < size_in_words; i++)
    {
        if (swd_transfer_retry(req, (uint32_t *)data) != 0x01)
        {
            return 0;
        }

        data += 4;
    }

    // dummy read
    req = SWD_REG_DP | SWD_REG_R | SWD_REG_ADR(DP_RDBUFF);
    ack = swd_transfer_retry(req, NULL);
    return (ack == 0x01);
}

// Read 32-bit word aligned values from target memory using address auto-increment.
// size is in bytes.
static uint8_t swd_read_block(uint32_t address, uint8_t *data, uint32_t size)
{
    uint8_t tmp_in[4], req, ack;
    uint32_t size_in_words;
    uint32_t i;

    if (size == 0)
    {
        return 0;
    }

    size_in_words = size / 4;

    if (!swd_write_ap(AP_CSW, CSW_VALUE | CSW_SIZE32))
    {
        return 0;
    }

    // TAR write
    req = SWD_REG_AP | SWD_REG_W | AP_TAR;
    int2array(tmp_in, address, 4);

    if (swd_transfer_retry(req, (uint32_t *)tmp_in) != DAP_TRANSFER_OK)
    {
        return 0;
    }

    // read data
    req = SWD_REG_AP | SWD_REG_R | AP_DRW;

    // initiate first read, data comes back in next read
    if (swd_transfer_retry(req, NULL) != 0x01)
    {
        return 0;
    }

    for (i = 0; i < (size_in_words - 1); i++)
    {
        if (swd_transfer_retry(req, (uint32_t *)data) != DAP_TRANSFER_OK)
        {
            return 0;
        }

        data += 4;
    }

    // read last word
    req = SWD_REG_DP | SWD_REG_R | SWD_REG_ADR(DP_RDBUFF);
    ack = swd_transfer_retry(req, (uint32_t *)data);
    return (ack == 0x01);
}

// Read target memory.
static uint8_t swd_read_data(uint32_t addr, uint32_t *val)
{
    uint8_t tmp_in[4];
    uint8_t tmp_out[4];
    uint8_t req, ack;
    uint32_t tmp;
    // put addr in TAR register
    int2array(tmp_in, addr, 4);
    req = SWD_REG_AP | SWD_REG_W | (1 << 2);

    if (swd_transfer_retry(req, (uint32_t *)tmp_in) != 0x01)
    {
        return 0;
    }

    // read data
    req = SWD_REG_AP | SWD_REG_R | (3 << 2);

    if (swd_transfer_retry(req, (uint32_t *)tmp_out) != 0x01)
    {
        return 0;
    }

    // dummy read
    req = SWD_REG_DP | SWD_REG_R | SWD_REG_ADR(DP_RDBUFF);
    ack = swd_transfer_retry(req, (uint32_t *)tmp_out);
    *val = 0;
    tmp = tmp_out[3];
    *val |= (tmp << 24);
    tmp = tmp_out[2];
    *val |= (tmp << 16);
    tmp = tmp_out[1];
    *val |= (tmp << 8);
    tmp = tmp_out[0];
    *val |= (tmp << 0);
    return (ack == 0x01);
}

// Write target memory.
static uint8_t swd_write_data(uint32_t address, uint32_t data)
{
    uint8_t tmp_in[4];
    uint8_t req, ack;
    // put addr in TAR register
    int2array(tmp_in, address, 4);
    req = SWD_REG_AP | SWD_REG_W | (1 << 2);

    if (swd_transfer_retry(req, (uint32_t *)tmp_in) != 0x01)
    {
        return 0;
    }

    // write data
    int2array(tmp_in, data, 4);
    req = SWD_REG_AP | SWD_REG_W | (3 << 2);

    if (swd_transfer_retry(req, (uint32_t *)tmp_in) != 0x01)
    {
        return 0;
    }

    // dummy read
    req = SWD_REG_DP | SWD_REG_R | SWD_REG_ADR(DP_RDBUFF);
    ack = swd_transfer_retry(req, NULL);
    return (ack == 0x01) ? 1 : 0;
}

// Read 32-bit word from target memory.
uint8_t swd_read_word(uint32_t addr, uint32_t *val)
{
    if (!swd_write_ap(AP_CSW, CSW_VALUE | CSW_SIZE32))
    {
        return 0;
    }

    if (!swd_read_data(addr, val))
    {
        return 0;
    }

    return 1;
}

// Write 32-bit word to target memory.
uint8_t swd_write_word(uint32_t addr, uint32_t val)
{
    if (!swd_write_ap(AP_CSW, CSW_VALUE | CSW_SIZE32))
    {
        return 0;
    }

    if (!swd_write_data(addr, val))
    {
        return 0;
    }

    return 1;
}

// Read 8-bit byte from target memory.
static uint8_t swd_read_byte(uint32_t addr, uint8_t *val)
{
    uint32_t tmp;

    if (!swd_write_ap(AP_CSW, CSW_VALUE | CSW_SIZE8))
    {
        return 0;
    }

    if (!swd_read_data(addr, &tmp))
    {
        return 0;
    }

    *val = (uint8_t)(tmp >> ((addr & 0x03) << 3));
    return 1;
}

// Write 8-bit byte to target memory.
static uint8_t swd_write_byte(uint32_t addr, uint8_t val)
{
    uint32_t tmp;

    if (!swd_write_ap(AP_CSW, CSW_VALUE | CSW_SIZE8))
    {
        return 0;
    }

    tmp = val << ((addr & 0x03) << 3);

    if (!swd_write_data(addr, tmp))
    {
        return 0;
    }

    return 1;
}

// Read unaligned data from target memory.
// size is in bytes.
uint8_t swd_read_memory(uint32_t address, uint8_t *data, uint32_t size)
{
    uint32_t n;

    // Read bytes until word aligned
    while ((size > 0) && (address & 0x3))
    {
        if (!swd_read_byte(address, data))
        {
            return 0;
        }

        address++;
        data++;
        size--;
    }

    // Read word aligned blocks
    while (size > 3)
    {
        // Limit to auto increment page size
        n = Flash_Page_Size - (address & (Flash_Page_Size - 1));

        if (size < n)
        {
            n = size & 0xFFFFFFFC; // Only count complete words remaining
        }

        if (!swd_read_block(address, data, n))
        {
            return 0;
        }

        address += n;
        data += n;
        size -= n;
    }

    // Read remaining bytes
    while (size > 0)
    {
        if (!swd_read_byte(address, data))
        {
            return 0;
        }

        address++;
        data++;
        size--;
    }

    return 1;
}

// Write unaligned data to target memory.
// size is in bytes.
uint8_t swd_write_memory(uint32_t address, uint8_t *data, uint32_t size)
{
    uint32_t n = 0;

    // Write bytes until word aligned
    while ((size > 0) && (address & 0x3))
    {
        if (!swd_write_byte(address, *data))
        {
            return 0;
        }

        address++;
        data++;
        size--;
    }

    // Write word aligned blocks
    while (size > 3)
    {
        // Limit to auto increment page size
        n = Flash_Page_Size - (address & (Flash_Page_Size - 1));

        if (size < n)
        {
            n = size & 0xFFFFFFFC; // Only count complete words remaining
        }

        if (!swd_write_block(address, data, n))
        {
            return 0;
        }

        address += n;
        data += n;
        size -= n;
    }

    // Write remaining bytes
    while (size > 0)
    {
        if (!swd_write_byte(address, *data))
        {
            return 0;
        }

        address++;
        data++;
        size--;
    }

    return 1;
}

// Execute system call.
static uint8_t swd_write_debug_state(DEBUG_STATE *state)
{
    uint32_t i, status;

    if (!swd_write_dp(DP_SELECT, 0))
    {
        return 0;
    }

    // R0, R1, R2, R3
    for (i = 0; i < 4; i++)
    {
        if (!swd_write_core_register(i, state->r[i]))
        {
            return 0;
        }
    }

    // R9
    if (!swd_write_core_register(9, state->r[9]))
    {
        return 0;
    }

    // R13, R14, R15
    for (i = 13; i < 16; i++)
    {
        if (!swd_write_core_register(i, state->r[i]))
        {
            return 0;
        }
    }

    // xPSR
    if (!swd_write_core_register(16, state->xpsr))
    {
        return 0;
    }

    if (!swd_write_word(DBG_HCSR, DBGKEY | C_DEBUGEN))
    {
        return 0;
    }

    // check status
    if (!swd_read_dp(DP_CTRL_STAT, &status))
    {
        return 0;
    }

    if (status & (STICKYERR | WDATAERR))
    {
        return 0;
    }

    return 1;
}

static uint8_t swd_read_core_register(uint32_t n, uint32_t *val)
{
    int i = 0, timeout = 100;

    if (!swd_write_word(DCRSR, n))
    {
        return 0;
    }

    // wait for S_REGRDY
    for (i = 0; i < timeout; i++)
    {
        if (!swd_read_word(DHCSR, val))
        {
            return 0;
        }

        if (*val & S_REGRDY)
        {
            break;
        }
    }

    if (i == timeout)
    {
        return 0;
    }

    if (!swd_read_word(DCRDR, val))
    {
        return 0;
    }

    return 1;
}

static uint8_t swd_write_core_register(uint32_t n, uint32_t val)
{
    int i = 0, timeout = 100;

    if (!swd_write_word(DCRDR, val))
    {
        return 0;
    }

    if (!swd_write_word(DCRSR, n | REGWnR))
    {
        return 0;
    }

    // wait for S_REGRDY
    for (i = 0; i < timeout; i++)
    {
        if (!swd_read_word(DHCSR, &val))
        {
            return 0;
        }

        if (val & S_REGRDY)
        {
            return 1;
        }
    }

    return 0;
}

static uint8_t swd_wait_until_halted(void)
{
    // Wait for target to stop
    uint32_t val, i, timeout = MAX_TIMEOUT;

    for (i = 0; i < timeout; i++)
    {
        if (!swd_read_word(DBG_HCSR, &val))
        {
            return 0;
        }

        if (val & S_HALT)
        {
            return 1;
        }
    }

    return 0;
}

uint8_t swd_flash_syscall_exec(const program_syscall_t *sysCallParam, uint32_t entry, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4)
{
    DEBUG_STATE state = {{0}, 0};
    // Call flash algorithm function on target and wait for result.
    state.r[0] = arg1;                         // R0: Argument 1
    state.r[1] = arg2;                         // R1: Argument 2
    state.r[2] = arg3;                         // R2: Argument 3
    state.r[3] = arg4;                         // R3: Argument 4
    state.r[9] = sysCallParam->static_base;    // SB: Static Base
    state.r[13] = sysCallParam->stack_pointer; // SP: Stack Pointer
    state.r[14] = sysCallParam->breakpoint;    // LR: Exit Point
    state.r[15] = entry;                       // PC: Entry Point
    state.xpsr = 0x01000000;                   // xPSR: T = 1, ISR = 0

    if (!swd_write_debug_state(&state))
    {
        return 0;
    }

    if (!swd_wait_until_halted())
    {
        return 0;
    }

    if (!swd_read_core_register(0, &state.r[0]))
    {
        return 0;
    }

    // Flash functions return 0 if successful.
    if (state.r[0] != 0)
    {
        return 0;
    }

    return 1;
}

// SWD Reset - send at least 50 HIGH bits on SWDIO
static uint8_t swd_reset(void)
{
    uint8_t tmp_in[8];
    uint8_t i = 0;

    for (i = 0; i < 8; i++)
    {
        tmp_in[i] = 0xff;
    }

    SWJ_Sequence(56, tmp_in);
    return 1;
}

// SWD Switch
static uint8_t swd_switch(uint16_t val)
{
    uint8_t tmp_in[2];
    tmp_in[0] = val & 0xff;
    tmp_in[1] = (val >> 8) & 0xff;
    SWJ_Sequence(16, tmp_in);
    return 1;
}

// Helper: bit-bang one SWD clock cycle with given SWDIO value
static inline void swd_clock_bit(uint32_t bit)
{
    PIN_SWDIO_OUT(bit);
    PIN_SWCLK_TCK_CLR();
    PIN_DELAY_SLOW(DAP_Data.clock_delay);
    PIN_SWCLK_TCK_SET();
    PIN_DELAY_SLOW(DAP_Data.clock_delay);
}

// Helper: clock one cycle without driving SWDIO (for turnaround/ACK)
static inline void swd_clock_cycle(void)
{
    PIN_SWCLK_TCK_CLR();
    PIN_DELAY_SLOW(DAP_Data.clock_delay);
    PIN_SWCLK_TCK_SET();
    PIN_DELAY_SLOW(DAP_Data.clock_delay);
}

// SWD Write TARGETSEL register (multi-drop SWDv2, target does NOT ACK)
// This must be done entirely by bit-banging because SWD_Transfer() expects an ACK.
static void swd_write_targetsel(uint32_t val)
{
    uint32_t parity;
    uint32_t n;

    // 1. Idle cycles (SWDIO low, at least 2 cycles after line reset)
    for (n = 0; n < 8; n++)
    {
        swd_clock_bit(0U);
    }

    // 2. Send 8-bit request header for DP Write TARGETSEL (addr 0x0C)
    //    Start=1, APnDP=0, RnW=0, A[2]=1, A[3]=1, Parity=0, Stop=0, Park=1
    //    LSB first: 1,0,0,1,1,0,0,1 = 0x99
    swd_clock_bit(1U);  // Start
    swd_clock_bit(0U);  // APnDP = 0 (DP)
    swd_clock_bit(0U);  // RnW = 0 (Write)
    swd_clock_bit(1U);  // A2 = 1
    swd_clock_bit(1U);  // A3 = 1
    swd_clock_bit(0U);  // Parity (0+0+1+1 = even = 0)
    swd_clock_bit(0U);  // Stop
    swd_clock_bit(1U);  // Park

    // 3. Turnaround(1) + ACK(3) + Turnaround(1) = 5 cycles
    //    Nobody drives SWDIO during this phase for TARGETSEL
    PIN_SWDIO_OUT_DISABLE();
    for (n = 0; n < 5; n++)
    {
        swd_clock_cycle();
    }
    PIN_SWDIO_OUT_ENABLE();

    // 4. Write 32-bit TARGETSEL data (LSB first)
    parity = 0U;
    for (n = 0; n < 32; n++)
    {
        swd_clock_bit(val & 1U);
        parity += val & 1U;
        val >>= 1;
    }

    // 5. Write parity bit
    swd_clock_bit(parity & 1U);

    // 6. Trailing idle cycles
    for (n = 0; n < 8; n++)
    {
        swd_clock_bit(0U);
    }

    PIN_SWDIO_OUT(1U);
}

// SWD Read ID
uint8_t swd_read_idcode(uint32_t *id)
{
    uint8_t tmp_in[1];
    uint8_t tmp_out[4];
    tmp_in[0] = 0x00;
    SWJ_Sequence(8, tmp_in);

    if (swd_read_dp(0, (uint32_t *)tmp_out) != 0x01)
    {
        return 0;
    }

    *id = (tmp_out[3] << 24) | (tmp_out[2] << 16) | (tmp_out[1] << 8) | tmp_out[0];
    return 1;
}

// Dormant-to-SWD wake-up sequence (ADIv5.2)
// Required if the DP is in dormant state (e.g. RP2040 after certain conditions)
static void swd_dormant_wakeup(void)
{
    // 1. At least 8 HIGH clocks
    uint8_t ones[8];
    for (int i = 0; i < 8; i++) ones[i] = 0xFF;
    SWJ_Sequence(8, ones);

    // 2. 128-bit Selection Alert sequence (LSB first)
    //    Value: 0x19BC0EA2 E3DDAFE9 86852D95 6209F392
    static const uint8_t selection_alert[16] = {
        0x92, 0xF3, 0x09, 0x62,  // 6209F392 LSB first
        0x95, 0x2D, 0x85, 0x86,  // 86852D95 LSB first
        0xE9, 0xAF, 0xDD, 0xE3,  // E3DDAFE9 LSB first
        0xA2, 0x0E, 0xBC, 0x19   // 19BC0EA2 LSB first
    };
    SWJ_Sequence(128, selection_alert);

    // 3. 4 LOW clocks
    uint8_t zero[1] = {0x00};
    SWJ_Sequence(4, zero);

    // 4. SWD Activation Code: 0x1A (8 bits, LSB first)
    uint8_t activation = 0x1A;
    SWJ_Sequence(8, &activation);
}

// Line reset + JTAG-to-SWD switch + optional TARGETSEL + read IDCODE
static uint8_t JTAG2SWD(uint32_t targetsel)
{
    uint32_t tmp = 0;

    if (!swd_reset())
    {
        return 0;
    }

    if (!swd_switch(0xE79E))
    {
        return 0;
    }

    if (!swd_reset())
    {
        return 0;
    }

    // For SWD multi-drop targets (e.g. RP2040), send TARGETSEL before reading IDCODE
    if (targetsel != 0)
    {
        swd_write_targetsel(targetsel);
    }

    if (!swd_read_idcode(&tmp))
    {
        return 0;
    }

    ESP_LOGI(SWD_TAG, "JTAG2SWD OK, IDCODE=0x%08" PRIX32, tmp);
    return 1;
}

// Try dormant-to-SWD wake-up, then TARGETSEL + read IDCODE
static uint8_t swd_dormant_to_swd(uint32_t targetsel)
{
    uint32_t tmp = 0;

    swd_dormant_wakeup();

    if (!swd_reset())
    {
        return 0;
    }

    if (targetsel != 0)
    {
        swd_write_targetsel(targetsel);
    }

    if (!swd_read_idcode(&tmp))
    {
        return 0;
    }

    ESP_LOGI(SWD_TAG, "Dormant-to-SWD OK, IDCODE=0x%08" PRIX32, tmp);
    return 1;
}

uint8_t swd_detect(void)
{
    // init dap state with fake values
    dap_state.select = 0xffffffff;
    dap_state.csw = 0xffffffff;
    swd_init();

    // 1. Try standard JTAG-to-SWD with TARGETSEL (RP2040 Core 0)
    if (JTAG2SWD(RP2040_CORE0_TARGETSEL))
    {
        ESP_LOGI(SWD_TAG, "1. JTAG2SWD with Core0 TARGETSEL successful");
        return 1;
    }
    // ESP_LOGW(SWD_TAG, "JTAG2SWD with Core0 TARGETSEL failed");

    // 2. Try dormant-to-SWD wake-up (RP2040 might be in dormant state)
    swd_init();
    if (swd_dormant_to_swd(RP2040_CORE0_TARGETSEL))
    {
        ESP_LOGI(SWD_TAG, "2. Dormant-to-SWD with Core0 TARGETSEL successful");
        return 2;
    }
    // ESP_LOGW(SWD_TAG, "Dormant-to-SWD with Core0 TARGETSEL failed");

    // 3. Try Rescue DP (works even if user code disabled SWD on Core 0)
    swd_init();
    if (JTAG2SWD(RP2040_RESCUE_TARGETSEL))
    {
        ESP_LOGI(SWD_TAG, "3. JTAG2SWD with Rescue DP successful");
        return 3;
    }
    // ESP_LOGW(SWD_TAG, "JTAG2SWD with Rescue DP failed");

    // 4. Try without TARGETSEL (standard SWD v1 targets)
    swd_init();
    if (JTAG2SWD(0))
    {
        ESP_LOGI(SWD_TAG, "4. JTAG2SWD without TARGETSEL successful");
        return 4;
    }
    // ESP_LOGW(SWD_TAG, "JTAG2SWD without TARGETSEL failed");

    return 0;
}

uint8_t swd_init_debug(void)
{
    uint32_t tmp = 0;
    int i = 0;
    int timeout = 100;
    // init dap state with fake values
    dap_state.select = 0xffffffff;
    dap_state.csw = 0xffffffff;
    swd_init();

    // call a target dependant function
    // this function can do several stuff before really initing the debug
    // target_before_init_debug();

    if (!JTAG2SWD(RP2040_CORE0_TARGETSEL))
    {
        return 0;
    }

    if (!swd_write_dp(DP_ABORT, STKCMPCLR | STKERRCLR | WDERRCLR | ORUNERRCLR))
    {
        return 0;
    }

    // Ensure CTRL/STAT register selected in DPBANKSEL
    if (!swd_write_dp(DP_SELECT, 0))
    {
        return 0;
    }

    // Power up
    if (!swd_write_dp(DP_CTRL_STAT, CSYSPWRUPREQ | CDBGPWRUPREQ))
    {
        return 0;
    }

    for (i = 0; i < timeout; i++)
    {
        if (!swd_read_dp(DP_CTRL_STAT, &tmp))
        {
            return 0;
        }
        if ((tmp & (CDBGPWRUPACK | CSYSPWRUPACK)) == (CDBGPWRUPACK | CSYSPWRUPACK))
        {
            // Break from loop if powerup is complete
            break;
        }
    }
    if (i == timeout)
    {
        // Unable to powerup DP
        return 0;
    }

    if (!swd_write_dp(DP_CTRL_STAT, CSYSPWRUPREQ | CDBGPWRUPREQ | TRNNORMAL | MASKLANE))
    {
        return 0;
    }

    // call a target dependant function:
    // some target can enter in a lock state, this function can unlock these targets
    // target_unlock_sequence();

    if (!swd_write_dp(DP_SELECT, 0))
    {
        return 0;
    }

    return 1;
}
/*
__attribute__((weak)) void swd_set_target_reset(uint8_t asserted)
{
    (asserted) ? PIN_nRESET_OUT(0) : PIN_nRESET_OUT(1);
}
*/
void swd_set_target_reset(uint8_t asserted)
{
    (asserted) ? PIN_nRESET_OUT(0) : PIN_nRESET_OUT(1);
}

uint8_t swd_set_target_state_hw(TARGET_RESET_STATE state)
{
    uint32_t val;
    int8_t ap_retries = 2;
    /* Calling swd_init prior to entering RUN state causes operations to fail. */
    if (state != RUN)
    {
        swd_init();
    }

    switch (state)
    {
    case RESET_HOLD:
        swd_set_target_reset(1);
        break;

    case RESET_RUN:
        swd_set_target_reset(1);
        delaymS(20);
        swd_set_target_reset(0);
        delaymS(20);
        swd_off();
        break;

    case RESET_PROGRAM:
        if (!swd_init_debug())
        {
            return 0;
        }

        // Enable debug
        while (swd_write_word(DBG_HCSR, DBGKEY | C_DEBUGEN) == 0)
        {
            if (--ap_retries <= 0)
                return 0;
            // Target is in invalid state?
            swd_set_target_reset(1);
            delaymS(20);
            swd_set_target_reset(0);
            delaymS(20);
        }

        // Enable halt on reset
        if (!swd_write_word(DBG_EMCR, VC_CORERESET))
        {
            return 0;
        }

        // Reset again
        swd_set_target_reset(1);
        delaymS(20);
        swd_set_target_reset(0);
        delaymS(20);

        do
        {
            if (!swd_read_word(DBG_HCSR, &val))
            {
                return 0;
            }
        } while ((val & S_HALT) == 0);

        // Disable halt on reset
        if (!swd_write_word(DBG_EMCR, 0))
        {
            return 0;
        }

        break;

    case NO_DEBUG:
        if (!swd_write_word(DBG_HCSR, DBGKEY))
        {
            return 0;
        }

        break;

    case DEBUG:
        if (!JTAG2SWD(RP2040_CORE0_TARGETSEL))
        {
            return 0;
        }

        if (!swd_write_dp(DP_ABORT, STKCMPCLR | STKERRCLR | WDERRCLR | ORUNERRCLR))
        {
            return 0;
        }

        // Ensure CTRL/STAT register selected in DPBANKSEL
        if (!swd_write_dp(DP_SELECT, 0))
        {
            return 0;
        }

        // Power up
        if (!swd_write_dp(DP_CTRL_STAT, CSYSPWRUPREQ | CDBGPWRUPREQ))
        {
            return 0;
        }

        // Enable debug
        if (!swd_write_word(DBG_HCSR, DBGKEY | C_DEBUGEN))
        {
            return 0;
        }

        break;

    case HALT:
        if (!swd_init_debug())
        {
            return 0;
        }

        // Enable debug and halt the core (DHCSR <- 0xA05F0003)
        if (!swd_write_word(DBG_HCSR, DBGKEY | C_DEBUGEN | C_HALT))
        {
            return 0;
        }

        // Wait until core is halted
        do
        {
            if (!swd_read_word(DBG_HCSR, &val))
            {
                return 0;
            }
        } while ((val & S_HALT) == 0);
        break;

    case RUN:
        if (!swd_write_word(DBG_HCSR, DBGKEY))
        {
            return 0;
        }
        swd_off();

    default:
        return 0;
    }

    return 1;
}

uint8_t swd_set_target_state_sw(TARGET_RESET_STATE state)
{
    uint32_t val;

    /* Calling swd_init prior to enterring RUN state causes operations to fail. */
    if (state != RUN)
    {
        swd_init();
    }

    switch (state)
    {
    case RESET_HOLD:
        swd_set_target_reset(1);
        break;

    case RESET_RUN:
        swd_set_target_reset(1);
        delaymS(20);
        swd_set_target_reset(0);
        delaymS(20);
        swd_off();
        break;

    case RESET_PROGRAM:
        if (!swd_init_debug())
        {
            return 0;
        }

        // Enable debug and halt the core (DHCSR <- 0xA05F0003)
        if (!swd_write_word(DBG_HCSR, DBGKEY | C_DEBUGEN | C_HALT))
        {
            return 0;
        }

        // Wait until core is halted
        do
        {
            if (!swd_read_word(DBG_HCSR, &val))
            {
                return 0;
            }
        } while ((val & S_HALT) == 0);

        // Enable halt on reset
        if (!swd_write_word(DBG_EMCR, VC_CORERESET))
        {
            return 0;
        }

        // Perform a soft reset
        if (!swd_read_word(NVIC_AIRCR, &val))
        {
            return 0;
        }

        if (!swd_write_word(NVIC_AIRCR, VECTKEY | (val & AIRCR_PRIGROUP_Msk) | SYSRESETREQ))
        {
            return 0;
        }

        delaymS(20);

        do
        {
            if (!swd_read_word(DBG_HCSR, &val))
            {
                return 0;
            }
        } while ((val & S_HALT) == 0);

        // Disable halt on reset
        if (!swd_write_word(DBG_EMCR, 0))
        {
            return 0;
        }

        break;

    case NO_DEBUG:
        if (!swd_write_word(DBG_HCSR, DBGKEY))
        {
            return 0;
        }

        break;

    case DEBUG:
        if (!JTAG2SWD(RP2040_CORE0_TARGETSEL))
        {
            return 0;
        }

        if (!swd_write_dp(DP_ABORT, STKCMPCLR | STKERRCLR | WDERRCLR | ORUNERRCLR))
        {
            return 0;
        }

        // Ensure CTRL/STAT register selected in DPBANKSEL
        if (!swd_write_dp(DP_SELECT, 0))
        {
            return 0;
        }

        // Power up
        if (!swd_write_dp(DP_CTRL_STAT, CSYSPWRUPREQ | CDBGPWRUPREQ))
        {
            return 0;
        }

        // Enable debug
        if (!swd_write_word(DBG_HCSR, DBGKEY | C_DEBUGEN))
        {
            return 0;
        }

        break;

    case HALT:
        if (!swd_init_debug())
        {
            return 0;
        }

        // Enable debug and halt the core (DHCSR <- 0xA05F0003)
        if (!swd_write_word(DBG_HCSR, DBGKEY | C_DEBUGEN | C_HALT))
        {
            return 0;
        }

        // Wait until core is halted
        do
        {
            if (!swd_read_word(DBG_HCSR, &val))
            {
                return 0;
            }
        } while ((val & S_HALT) == 0);
        break;

    case RUN:
        if (!swd_write_word(DBG_HCSR, DBGKEY))
        {
            return 0;
        }
        swd_off();

    default:
        return 0;
    }

    return 1;
}
