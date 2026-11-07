/* CH32V203 用の最小スタートアップ。割り込みは使わない。例外時は停止し IWDG でリセットする。 */
    .section .init, "ax"
    .globl _start
_start:
    j reset

    .text
reset:
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop
    la sp, _estack

    la a0, _sidata
    la a1, _sdata
    la a2, _edata
1:  bgeu a1, a2, 2f
    lw t0, 0(a0)
    sw t0, 0(a1)
    addi a0, a0, 4
    addi a1, a1, 4
    j 1b
2:
    la a0, _sbss
    la a1, _ebss
3:  bgeu a0, a1, 4f
    sw zero, 0(a0)
    addi a0, a0, 4
    j 3b
4:
    li t0, 0x1f
    csrw 0xbc0, t0     /* パイプラインと命令予測 (WCH EVT と同じ値) */
    la t0, trap
    csrw mtvec, t0
    jal main
trap:
    j trap
