/* PC 上で実行する dbglog.c のテスト (make test) */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/dbglog.h"

/* バッファの中身を取り出して文字列にする */
static const char *drain(void)
{
    static char out[DBGLOG_BUF_SIZE + 1];
    size_t n = 0;
    uint8_t c;

    while (dbglog_pop(&c))
        out[n++] = (char)c;
    out[n] = '\0';
    return out;
}

int main(void)
{
    uint8_t c;

    dbglog_init();
    assert(dbglog_pop(&c) == 0);

    dbglog_puts("id=");
    dbglog_hex(0x7D0, 3);
    dbglog_puts(" r=");
    dbglog_hex(0x0A, 2);
    assert(strcmp(drain(), "id=7D0 r=0A") == 0);

    dbglog_hex(0xDEADBEEF, 8);
    dbglog_hex(0x12345, 2); /* 桁数を超える上位は出さない */
    assert(strcmp(drain(), "DEADBEEF45") == 0);

    dbglog_dec(0);
    dbglog_puts(",");
    dbglog_dec(500);
    dbglog_puts(",");
    dbglog_dec(4294967295u);
    assert(strcmp(drain(), "0,500,4294967295") == 0);

    /* 一杯になったら捨てて数える。先に積んだ分は壊れない */
    for (unsigned i = 0; i < DBGLOG_BUF_SIZE; i++)
        dbglog_puts("x");
    dbglog_puts("abc");
    assert(dbglog_take_dropped() == 3);
    assert(dbglog_take_dropped() == 0);
    assert(strlen(drain()) == DBGLOG_BUF_SIZE);

    /* 読み出し位置が一周しても正しく出る */
    for (unsigned i = 0; i < 3 * DBGLOG_BUF_SIZE; i++) {
        dbglog_hex(i & 0xFu, 1);
        assert(dbglog_pop(&c) == 1 && c == "0123456789ABCDEF"[i & 0xFu]);
    }
    assert(dbglog_pop(&c) == 0);

    puts("test_dbglog: ok");
    return 0;
}
