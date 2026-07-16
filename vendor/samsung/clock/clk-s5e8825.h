/*
 * Exynos 1280 (s5e8825) Clock Controller — Register definitions
 * ExynosNext Kernel — Linux 6.18 LTS
 *
 * TODO: Port register definitions from Samsung's kernel source.
 * These are placeholders showing the register layout.
 *
 * The actual register offsets must be obtained from:
 *   - Samsung kernel source: drivers/clk/samsung/clk-s5e8825.c
 *   - Samsung TRM (Technical Reference Manual) for s5e8825
 */

#ifndef _CLK_S5E8825_H
#define _CLK_S5E8825_H

/* Clock controller base addresses */
#define S5E8825_CMU_BASE		0x12000000

/* PLL registers (placeholder offsets) */
#define S5E8825_PLL_CON0			0x0000
#define S5E8825_PLL_CON1			0x0004
#define S5E8825_PLL_CON2			0x0008
#define S5E8825_PLL_LOCK			0x0010

/* MUX registers */
#define S5E8825_MUX_SEL0			0x0100
#define S5E8825_MUX_SEL1			0x0104
#define S5E8825_MUX_SEL2			0x0108
#define S5E8825_MUX_EN0				0x0200

/* DIV registers */
#define S5E8825_DIV_SEL0			0x0300
#define S5E8825_DIV_SEL1			0x0304
#define S5E8825_DIV_SEL2			0x0308

/* Gate registers */
#define S5E8825_GATE_ENABLE0			0x0400
#define S5E8825_GATE_ENABLE1			0x0404
#define S5E8825_GATE_ENABLE2			0x0408
#define S5E8825_GATE_STATUS0			0x0500

/* Clock names */
#define S5E8825_PLL_AOUT			"pll_aout"
#define S5E8825_PLL_DOUT			"pll_dout"
#define S5E8825_PLL_EOUT			"pll_eout"
#define S5E8825_PLL_S2OUT			"pll_s2out"
#define S5E8825_PLL_VOUT			"pll_vout"

#define S5E8825_CLK_CPU				"clk_cpu"
#define S5E8825_CLK_BUS				"clk_bus"
#define S5E8825_CLK_DISPLAY			"clk_display"
#define S5E8825_CLK_MEDIA			"clk_media"

#endif /* _CLK_S5E8825_H */
