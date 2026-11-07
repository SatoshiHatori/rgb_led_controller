/* PC 上で実行する color.c のテスト (make test) */
#include <assert.h>
#include <stdio.h>
#include "../src/color.h"

static void expect_rgb(const color_state_t *s, uint8_t r, uint8_t g, uint8_t b)
{
    assert(s->r == r && s->g == g && s->b == b);
}

int main(void)
{
    color_state_t s;
    const uint8_t rgb[8] = {10, 20, 30, 0, 99, 99, 99, 99};
    const uint8_t rgb_hold[4] = {1, 2, 3, 2};

    color_init(&s);
    expect_rgb(&s, 0, 0, 0);

    /* DLC 不足のフレームは状態を変えない */
    assert(color_on_frame(&s, 2, rgb) == 0);
    expect_rgb(&s, 0, 0, 0);

    /* DLC 3: 既定時間だけ点灯し、CAN が途絶えると消灯する */
    assert(color_on_frame(&s, 3, rgb) == 1);
    expect_rgb(&s, 10, 20, 30);
    for (unsigned i = 0; i + 1 < COLOR_TIMEOUT_MS; i++)
        color_tick_1ms(&s);
    expect_rgb(&s, 10, 20, 30);
    color_tick_1ms(&s);
    expect_rgb(&s, 0, 0, 0);

    /* 消灯後は次のフレームまで消灯のまま */
    for (int i = 0; i < 100000; i++)
        color_tick_1ms(&s);
    expect_rgb(&s, 0, 0, 0);

    /* byte3 = 0 も既定時間で消灯する。byte4 以降は無視 */
    assert(color_on_frame(&s, 8, rgb) == 1);
    assert(s.hold_ms == COLOR_TIMEOUT_MS);

    /* 既定時間より短い周期で受信し続ければ点灯を維持する */
    for (int n = 0; n < 10; n++) {
        color_on_frame(&s, 3, rgb);
        for (unsigned i = 0; i + 1 < COLOR_TIMEOUT_MS; i++)
            color_tick_1ms(&s);
        expect_rgb(&s, 10, 20, 30);
    }

    /* byte3 = 2 → 200ms で消灯。199ms では消えない */
    assert(color_on_frame(&s, 4, rgb_hold) == 1);
    for (int i = 0; i < 199; i++)
        color_tick_1ms(&s);
    expect_rgb(&s, 1, 2, 3);
    color_tick_1ms(&s);
    expect_rgb(&s, 0, 0, 0);

    /* 消灯前の再受信でタイマーを延長する */
    color_on_frame(&s, 4, rgb_hold);
    for (int i = 0; i < 150; i++)
        color_tick_1ms(&s);
    color_on_frame(&s, 4, rgb_hold);
    for (int i = 0; i < 150; i++)
        color_tick_1ms(&s);
    expect_rgb(&s, 1, 2, 3);

    /* DLC 3 の再受信で既定時間に戻る */
    color_on_frame(&s, 3, rgb);
    assert(s.hold_ms == COLOR_TIMEOUT_MS);

    /* 初期状態は消灯で、タイマーは動かない */
    color_init(&s);
    color_tick_1ms(&s);
    expect_rgb(&s, 0, 0, 0);

    /* デューティ: 端点、単調増加、非ゼロ入力は必ず点灯 */
    assert(color_duty(0) == 0);
    assert(color_duty(255) == COLOR_PWM_PERIOD);
    assert(color_duty(1) >= 1);
    for (int v = 1; v < 256; v++)
        assert(color_duty((uint8_t)v) >= color_duty((uint8_t)(v - 1)));

    puts("ok");
    return 0;
}
