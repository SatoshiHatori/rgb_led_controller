#include "dbglog.h"

static uint8_t buf[DBGLOG_BUF_SIZE];
static uint32_t head, tail, dropped; /* head: 書き込み位置、tail: 読み出し位置 (どちらも単調増加) */

void dbglog_init(void)
{
    head = tail = dropped = 0;
}

static void put(char c)
{
    if (head - tail >= DBGLOG_BUF_SIZE) {
        dropped++;
        return;
    }
    buf[head++ & (DBGLOG_BUF_SIZE - 1u)] = (uint8_t)c;
}

void dbglog_puts(const char *s)
{
    while (*s)
        put(*s++);
}

void dbglog_hex(uint32_t value, uint8_t digits)
{
    while (digits-- > 0)
        put("0123456789ABCDEF"[(value >> (4u * digits)) & 0xFu]);
}

void dbglog_dec(uint32_t value)
{
    char tmp[10];
    uint8_t n = 0;

    do {
        tmp[n++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0);
    while (n > 0)
        put(tmp[--n]);
}

int dbglog_pop(uint8_t *c)
{
    if (head == tail)
        return 0;
    *c = buf[tail++ & (DBGLOG_BUF_SIZE - 1u)];
    return 1;
}

uint32_t dbglog_take_dropped(void)
{
    uint32_t n = dropped;

    dropped = 0;
    return n;
}
