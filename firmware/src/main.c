/*
 * RGB LED Controller Rev B ファームウェア (CH32V203K8T6)
 *
 * CAN で受けた色を TIM2 の PWM (PA0=R, PA1=G, PA2=B) に出す。
 * 割り込みは使わず、メインループでポーリングする。クロックは 8MHz 水晶を PLL なしで使う。
 * フレーム形式は docs/adr/0005-can-color-frame.md を参照。
 *
 * レジスタのオフセットとビットは WCH EVT (ch32v20x.h, ch32v20x_can.c) と照合した。
 *
 * デバッグ出力: USART1 (PA9=TX、J5) に 115200bps 8N1 で状態を出す。-DDEBUG_UART=0 で無効。
 * 出力は 1 行 1 イベントで、起動、色の変化、途絶による消灯、CAN エラー状態の変化、取りこぼし。
 */
#include <stdint.h>
#include "color.h"
#include "dbglog.h"

#ifndef DEBUG_UART
#define DEBUG_UART 1
#endif

#ifndef CAN_COLOR_ID
#define CAN_COLOR_ID 0x7D0u /* 11bit 標準 ID。番号が大きいほど低優先 (ADR-0005) */
#endif

/* 8MHz / (BRP * (1 + TS1 + TS2)) がビットレート。サンプル点 87.5% (TS1=13, TS2=2)。 */
#ifndef CAN_BRP
#define CAN_BRP 1u /* 1: 500kbit/s, 2: 250kbit/s, 4: 125kbit/s */
#endif

#define REG32(addr) (*(volatile uint32_t *)(addr))
#define REG16(addr) (*(volatile uint16_t *)(addr))

#define RCC_BASE 0x40021000u
#define RCC_CTLR REG32(RCC_BASE + 0x00)
#define RCC_CFGR0 REG32(RCC_BASE + 0x04)
#define RCC_APB2PCENR REG32(RCC_BASE + 0x18)
#define RCC_APB1PCENR REG32(RCC_BASE + 0x1C)
#define RCC_RSTSCKR REG32(RCC_BASE + 0x24)

#define USART1_BASE 0x40013800u
#define USART1_STATR REG32(USART1_BASE + 0x00)
#define USART1_DATAR REG32(USART1_BASE + 0x04)
#define USART1_BRR REG32(USART1_BASE + 0x08)
#define USART1_CTLR1 REG32(USART1_BASE + 0x0C)
#define USART_BAUD 115200u
#define SYSCLK_HZ 8000000u

#define GPIOA_BASE 0x40010800u
#define GPIOB_BASE 0x40010C00u
#define GPIO_CFGLR(b) REG32((b) + 0x00)
#define GPIO_CFGHR(b) REG32((b) + 0x04)
#define GPIO_OUTDR(b) REG32((b) + 0x0C)

#define TIM2_BASE 0x40000000u
#define TIM2_CTLR1 REG32(TIM2_BASE + 0x00)
#define TIM2_INTFR REG32(TIM2_BASE + 0x10)
#define TIM2_SWEVGR REG32(TIM2_BASE + 0x14)
#define TIM2_CHCTLR1 REG32(TIM2_BASE + 0x18)
#define TIM2_CHCTLR2 REG32(TIM2_BASE + 0x1C)
#define TIM2_CCER REG32(TIM2_BASE + 0x20)
#define TIM2_PSC REG32(TIM2_BASE + 0x28)
#define TIM2_ATRLR REG32(TIM2_BASE + 0x2C)
#define TIM2_CH1CVR REG32(TIM2_BASE + 0x34)
#define TIM2_CH2CVR REG32(TIM2_BASE + 0x38)
#define TIM2_CH3CVR REG32(TIM2_BASE + 0x3C)

#define IWDG_BASE 0x40003000u
#define IWDG_CTLR REG32(IWDG_BASE + 0x00)
#define IWDG_PSCR REG32(IWDG_BASE + 0x04)
#define IWDG_RLDR REG32(IWDG_BASE + 0x08)
#define IWDG_STATR REG32(IWDG_BASE + 0x0C)

#define CAN_BASE 0x40006400u
#define CAN_CTLR REG32(CAN_BASE + 0x000)
#define CAN_STATR REG32(CAN_BASE + 0x004)
#define CAN_RFIFO0 REG32(CAN_BASE + 0x00C)
#define CAN_ERRSR REG32(CAN_BASE + 0x018)
#define CAN_BTIMR REG32(CAN_BASE + 0x01C)
#define CAN_RXMDTR0 REG32(CAN_BASE + 0x1B4)
#define CAN_RXMDLR0 REG32(CAN_BASE + 0x1B8)
#define CAN_FCTLR REG32(CAN_BASE + 0x200)
#define CAN_FMCFGR REG32(CAN_BASE + 0x204)
#define CAN_FSCFGR REG32(CAN_BASE + 0x20C)
#define CAN_FAFIFOR REG32(CAN_BASE + 0x214)
#define CAN_FWR REG32(CAN_BASE + 0x21C)
#define CAN_F0R1 REG32(CAN_BASE + 0x240)
#define CAN_F0R2 REG32(CAN_BASE + 0x244)

#define CAN_INAK_TIMEOUT 0xFFFFu
#define HSE_START_TIMEOUT 0x8000u

/* 水晶が起動しない場合は内蔵 8MHz のまま動く。周波数は同じだが CAN の精度は保証されない。
 * 水晶で動いていれば 1 を返す。 */
static int clock_init(void)
{
    uint32_t n = 0;

    RCC_CTLR |= 1u << 16; /* HSEON */
    while (!(RCC_CTLR & (1u << 17)) && n++ < HSE_START_TIMEOUT) /* HSERDY */
        ;
    if (!(RCC_CTLR & (1u << 17)))
        return 0;
    RCC_CFGR0 = (RCC_CFGR0 & ~3u) | 1u; /* SW = HSE */
    while ((RCC_CFGR0 & 0xCu) != 0x4u)
        ;
    return 1;
}

static void gpio_init(void)
{
    uint32_t cfgh;

    RCC_APB2PCENR |= (1u << 0) | (1u << 2) | (1u << 3); /* AFIO, GPIOA, GPIOB */

    /* 未使用ピンはプルアップ入力 (0x8)。PA0-2 は PWM 出力 (AF PP 10MHz = 0x9)。 */
    GPIO_OUTDR(GPIOA_BASE) = 0xFFF8u;
    GPIO_CFGLR(GPIOA_BASE) = 0x88888999u;

    /* PA11=CAN_RX (0x8)、PA12=CAN_TX (AF PP 50MHz = 0xB)。PA13/14 はデバッグ用に変更しない。
     * DEBUG_UART のとき PA9=USART1_TX (AF PP 50MHz = 0xB)。PA10 (RX) はプルアップ入力のまま。 */
    cfgh = GPIO_CFGHR(GPIOA_BASE);
    cfgh &= 0x0FF00000u;
    cfgh |= DEBUG_UART ? 0x800B88B8u : 0x800B8888u;
    GPIO_CFGHR(GPIOA_BASE) = cfgh;

    GPIO_OUTDR(GPIOB_BASE) = 0xFFFFu;
    GPIO_CFGLR(GPIOB_BASE) = 0x88888888u;
    GPIO_CFGHR(GPIOB_BASE) = 0x88888888u;
}

static void pwm_init(void)
{
    RCC_APB1PCENR |= 1u << 0; /* TIM2 */
    TIM2_PSC = 0;
    TIM2_ATRLR = COLOR_PWM_PERIOD - 1u;
    TIM2_CH1CVR = TIM2_CH2CVR = TIM2_CH3CVR = 0;
    TIM2_CHCTLR1 = (6u << 4) | (1u << 3) | (6u << 12) | (1u << 11); /* CH1/2: PWM1, preload */
    TIM2_CHCTLR2 = (6u << 4) | (1u << 3);                           /* CH3: PWM1, preload */
    TIM2_CCER = (1u << 0) | (1u << 4) | (1u << 8);
    TIM2_SWEVGR = 1u; /* UG */
    TIM2_CTLR1 = (1u << 7) | (1u << 0); /* ARPE, CEN */
}

static int can_wait_inak(uint32_t expected)
{
    uint32_t n = 0;

    while ((CAN_STATR & 1u) != expected) {
        if (n++ >= CAN_INAK_TIMEOUT)
            return 0;
    }
    return 1;
}

/* 送信のみ。待たずに済むよう、メインループの uart_poll で 1 文字ずつ出す。 */
static void uart_init(void)
{
    RCC_APB2PCENR |= 1u << 14; /* USART1 */
    USART1_BRR = (SYSCLK_HZ + USART_BAUD / 2u) / USART_BAUD; /* 69: 115942bps、誤差 +0.6% */
    USART1_CTLR1 = (1u << 13) | (1u << 3); /* UE, TE */
}

static void uart_poll(void)
{
    uint8_t c;

    if ((USART1_STATR & (1u << 7)) && dbglog_pop(&c)) /* TXE */
        USART1_DATAR = c;
}

/* 通常モードに入れたら 1 を返す */
static int can_init(void)
{
    RCC_APB1PCENR |= 1u << 25; /* CAN1 */

    CAN_CTLR = (CAN_CTLR & ~2u) | 1u; /* SLEEP 解除, INRQ */
    (void)can_wait_inak(1u);

    CAN_CTLR |= 1u << 6; /* ABOM: バスオフから自動復帰 */
    CAN_BTIMR = (0u << 24) | ((2u - 1u) << 20) | ((13u - 1u) << 16) | (CAN_BRP - 1u);

    /* フィルタ 0: 32bit マスク。標準 ID のデータフレームだけを FIFO0 に入れる */
    CAN_FCTLR |= 1u; /* FINIT */
    CAN_FWR &= ~1u;
    CAN_FSCFGR |= 1u;
    CAN_F0R1 = CAN_COLOR_ID << 21;
    CAN_F0R2 = (0x7FFu << 21) | (1u << 2) | (1u << 1); /* ID + IDE + RTR を比較 */

    /* WCH EVT の CAN_FilterInit が CH32V20x_D6 (K8 を含む) で行う処理をそのまま再現する */
    if ((REG32(0x40022030u) & 0x0F000000u) == 0) {
        for (uint32_t i = 0; i < 64u; i++)
            REG16(0x40006000u + 512u + 4u * i) = REG16(0x40006000u + 768u + 4u * i);
    }

    CAN_FMCFGR &= ~1u;
    CAN_FAFIFOR &= ~1u;
    CAN_FWR |= 1u;
    CAN_FCTLR &= ~1u;

    CAN_CTLR &= ~1u; /* 通常モードへ */
    return can_wait_inak(0u);
}

static void iwdg_init(void)
{
    IWDG_CTLR = 0x5555u;
    IWDG_PSCR = 3u;    /* LSI 40kHz / 32 */
    IWDG_RLDR = 625u;  /* 約 0.5 秒 */
    IWDG_CTLR = 0xAAAAu;
    IWDG_CTLR = 0xCCCCu;
}

static void pwm_apply(const color_state_t *s)
{
    TIM2_CH1CVR = color_duty(s->r);
    TIM2_CH2CVR = color_duty(s->g);
    TIM2_CH3CVR = color_duty(s->b);
}

/* 直前のリセット要因を返してフラグを消す */
static const char *reset_cause(void)
{
    uint32_t f = RCC_RSTSCKR;
    const char *s = "?";

    if (f & (1u << 29))
        s = "iwdg";
    else if (f & (1u << 28))
        s = "soft";
    else if (f & (1u << 27))
        s = "por";
    else if (f & (1u << 26))
        s = "pin";
    RCC_RSTSCKR |= 1u << 24; /* RMVF */
    return s;
}

static void log_boot(int hse, int can_ok)
{
    dbglog_puts("\r\nboot rst=");
    dbglog_puts(reset_cause());
    dbglog_puts(hse ? " clk=hse" : " clk=hsi");
    dbglog_puts(can_ok ? " can=ok" : " can=ng");
    dbglog_puts(" id=");
    dbglog_hex(CAN_COLOR_ID, 3);
    dbglog_puts(" kbps=");
    dbglog_dec(500u / CAN_BRP);
    dbglog_puts(" timeout_ms=");
    dbglog_dec(COLOR_TIMEOUT_MS);
    dbglog_puts("\r\n");
}

static void log_color(const color_state_t *s)
{
    dbglog_puts("rgb ");
    dbglog_hex(s->r, 2);
    dbglog_hex(s->g, 2);
    dbglog_hex(s->b, 2);
    dbglog_puts(" hold_ms=");
    dbglog_dec(s->hold_ms);
    dbglog_puts("\r\n");
}

/* ERRSR の BOFF/EPVF/EWGF と LEC (直近のエラー種別) が変わったら出す */
static void log_can_error(uint32_t *last)
{
    uint32_t e = CAN_ERRSR;
    uint32_t key = e & 0x77u;

    if (key == *last)
        return;
    *last = key;
    dbglog_puts("can errsr=");
    dbglog_hex(e, 8);
    dbglog_puts((e & 4u) ? " busoff" : (e & 2u) ? " passive" : (e & 1u) ? " warn" : " ok");
    dbglog_puts("\r\n");
}

int main(void)
{
    color_state_t state;
    uint32_t last_err = 0;
    int hse, can_ok;

    hse = clock_init();
    gpio_init();
    pwm_init();
    can_ok = can_init();
    iwdg_init();
    color_init(&state);
    dbglog_init();
    if (DEBUG_UART) {
        uart_init();
        log_boot(hse, can_ok);
    }

    for (;;) {
        if (CAN_RFIFO0 & 3u) { /* FMP0 */
            uint32_t low = CAN_RXMDLR0;
            uint8_t data[4] = {(uint8_t)low, (uint8_t)(low >> 8), (uint8_t)(low >> 16), (uint8_t)(low >> 24)};
            uint8_t dlc = (uint8_t)(CAN_RXMDTR0 & 0xFu);
            color_state_t before = state;

            if (color_on_frame(&state, dlc, data)) {
                /* 毎フレーム出すと送信が追いつかないので、色が変わったときと消灯から点いたときだけ出す */
                if (DEBUG_UART && (before.hold_ms == 0 || before.r != state.r || before.g != state.g ||
                                   before.b != state.b))
                    log_color(&state);
            } else if (DEBUG_UART) {
                dbglog_puts("rx short dlc=");
                dbglog_dec(dlc);
                dbglog_puts("\r\n");
            }
            CAN_RFIFO0 |= 1u << 5; /* RFOM0 */
        }
        if (TIM2_INTFR & 1u) { /* UIF: 1ms 周期 */
            uint32_t was_on = state.hold_ms;

            TIM2_INTFR = 0xFFFFFFFEu;
            color_tick_1ms(&state);
            if (DEBUG_UART && was_on && state.hold_ms == 0)
                dbglog_puts("off timeout\r\n");
        }
        pwm_apply(&state);
        if (DEBUG_UART) {
            uint32_t dropped;

            log_can_error(&last_err);
            dropped = dbglog_take_dropped();
            if (dropped) {
                dbglog_puts("log dropped=");
                dbglog_dec(dropped);
                dbglog_puts("\r\n");
            }
            uart_poll();
        }
        IWDG_CTLR = 0xAAAAu;
    }
}
