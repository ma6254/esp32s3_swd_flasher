#include <stdio.h>
#include "utils.h"

/*******************************************************************************
 * 以十六进制格式打印缓冲区内容
 * @param buf 要打印的缓冲区
 * @param len 缓冲区长度
 * @return None
 ******************************************************************************/
void hexdump(const uint8_t *buf, uint32_t len)
{
    uint32_t i = 0;
    uint32_t line_i = 0;

    printf("[hexdump] ======================================================================\r\n");
    printf("[hexdump]       0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F  0123456789ABCDEF\r\n");

    for (i = 0; i < len; i += 0x10)
    {
        printf("[hexdump] %04lX", i / 16 * 16);

        for (line_i = 0; line_i < 0x10; line_i++)
        {
            if ((i + line_i) >= len)
            {
                printf("   ");
            }
            else
            {
                printf(" %02X", *(buf + i + line_i));
            }

        }

        for (line_i = 0; line_i < 0x10; line_i++)
        {
            if ((i + line_i) >= len)
            {
                printf("   ");
            }
            else
            {
                printf(" %c", (*(buf + i + line_i) >= 32 && *(buf + i + line_i) <= 126) ? *(buf + i + line_i) : '.');
            }

        }

        printf("\r\n");
    }

    printf("[hexdump] ======================================================================\r\n");
}
