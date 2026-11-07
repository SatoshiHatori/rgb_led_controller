/*
 * デバッグ出力用の送信バッファと数値の整形。ハードウェアに依存しない。
 * 書き込みは待たずにバッファへ積み、メインループで 1 文字ずつ UART に出す。
 * バッファが一杯のときは書き込みを捨て、捨てた文字数を数える。
 */
#ifndef DBGLOG_H
#define DBGLOG_H

#include <stdint.h>

#define DBGLOG_BUF_SIZE 256u /* 2 のべき乗 */

void dbglog_init(void);
void dbglog_puts(const char *s);
void dbglog_hex(uint32_t value, uint8_t digits); /* 大文字 16 進、先頭 0 埋め */
void dbglog_dec(uint32_t value);

/* 送る文字があれば *c に入れて 1 を返す */
int dbglog_pop(uint8_t *c);

/* 一杯で捨てた文字数 (前回呼んでからの数)。呼ぶと 0 に戻る */
uint32_t dbglog_take_dropped(void);

#endif
