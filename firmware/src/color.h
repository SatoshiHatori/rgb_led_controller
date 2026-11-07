/* CAN 色指定フレームの解釈と PWM デューティ計算。ハードウェアに依存しない。 */
#ifndef COLOR_H
#define COLOR_H

#include <stdint.h>

#define COLOR_PWM_PERIOD 8000u /* 8MHz / 8000 = 1kHz (ADR-0002 の R10 等で 2kHz 以下の前提) */
#define COLOR_HOLD_UNIT_MS 100u
#ifndef COLOR_TIMEOUT_MS
#define COLOR_TIMEOUT_MS 500u /* data[3] が 0 か省略のとき、最後の受信からこの時間で消灯する */
#endif

typedef struct {
    uint8_t r, g, b;
    uint32_t hold_ms; /* 残り点灯時間。尽きたら消灯する。0 = 消灯中 */
} color_state_t;

void color_init(color_state_t *s);

/* DLC と先頭データを渡す。DLC < 3 は無視し 0 を返す。適用したら 1 を返す。 */
int color_on_frame(color_state_t *s, uint8_t dlc, const uint8_t *data);

/* 1ms ごとに呼ぶ */
void color_tick_1ms(color_state_t *s);

/* 0-255 の入力を 2 乗カーブで 0..COLOR_PWM_PERIOD に変換する。0 は完全消灯、255 は完全点灯。 */
uint16_t color_duty(uint8_t value);

#endif
