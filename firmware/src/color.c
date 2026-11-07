#include "color.h"

void color_init(color_state_t *s)
{
    s->r = s->g = s->b = 0;
    s->hold_ms = 0;
}

int color_on_frame(color_state_t *s, uint8_t dlc, const uint8_t *data)
{
    if (dlc < 3)
        return 0;
    s->r = data[0];
    s->g = data[1];
    s->b = data[2];
    s->hold_ms = (dlc >= 4 && data[3] != 0) ? (uint32_t)data[3] * COLOR_HOLD_UNIT_MS : COLOR_TIMEOUT_MS;
    return 1;
}

void color_tick_1ms(color_state_t *s)
{
    if (s->hold_ms == 0)
        return;
    if (--s->hold_ms == 0)
        s->r = s->g = s->b = 0;
}

uint16_t color_duty(uint8_t value)
{
    uint32_t d;

    if (value == 0)
        return 0;
    d = ((uint32_t)value * value * COLOR_PWM_PERIOD + (255u * 255u) / 2u) / (255u * 255u);
    return (uint16_t)(d == 0 ? 1 : d); /* 0 以外の入力が消灯にならないようにする */
}
