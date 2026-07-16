/*
 * Samsung Exynos 1280 (s5e8825) Clock Controller Driver
 * ExynosNext Kernel — Linux 6.18 LTS
 *
 * Ported from Samsung's Android 16 kernel source.
 * Uses standard samsung_clk API instead of proprietary CAL-IF.
 *
 * Reference upstream drivers:
 *   - drivers/clk/samsung/clk-exynos850.c
 *   - drivers/clk/samsung/clk-exynos2100.c
 *   - drivers/clk/samsung/clk-gs101.c
 *
 * Copyright (C) 2026 ExynosNext Project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/slab.h>

#include <dt-bindings/clock/samsung,exynos1280-clk.h>

#include "clk-s5e8825.h"

/*
 * Exynos 1280 Clock Controller
 *
 * The CMU (Clock Management Unit) controls clocks for all IP blocks.
 * Base address: 0x12000000
 *
 * Clock hierarchy:
 *   xin26m (26MHz) ──┬── APLL (CPU)
 *                     ├── DPLL (Display)
 *                     ├── EPLL (Audio)
 *                     ├── S2PLL (Modem)
 *                     └── VPLL (Video)
 */

/* PLL configurations */
#define APLL_LOCKED_VAL		0x00002020
#define PLL_CON0_ENABLE		(1 << 31)
#define PLL_CON0_DIV_MASK	0x3F
#define PLL_CON1_DIV_MASK	0x3F

/* PLL CON0 register fields */
#define PLL_P_SHIFT		0
#define PLL_M_SHIFT		16
#define PLL_S_SHIFT		24
#define PLL_K_SHIFT		0

/* PLL CON1 register fields */
#define PLL_MFRAC_SHIFT		0
#define PLL_KFRAC_SHIFT		16

/* PLL lock status */
#define PLL_LOCKED		(1 << 29)

/* Clock gate registers */
#define CLK_ENABLE_MASK		(1 << 0)

/* MUX select registers */
#define MUX_SEL_MASK		0x7
#define MUX_SEL_SHIFT(n)	((n) * 4)

/* ── Fixed-rate clocks ────────────────────────────────────────────────── */

SAMSUNG_FIXED_CLK(xin26m_clk, NULL, CLK_IGNORE_UNUSED, 26000000);

static struct samsung_fixed_rate_clock exynos1280_fr_clks[] __initdata = {
	FIXED_CLK(xin26m_clk),
};

/* ── PLL clocks ───────────────────────────────────────────────────────── */

static const struct samsung_pll_rate_table apll_rates[] = {
	/* APLL: CPU clock */
	{ .rate = 1980000000, .m = 330, .p = 2, .s = 0, .k = 0 },
	{ .rate = 1800000000, .m = 300, .p = 2, .s = 0, .k = 0 },
	{ .rate = 1600000000, .m = 267, .p = 2, .s = 0, .k = 0 },
	{ .rate = 1400000000, .m = 233, .p = 2, .s = 0, .k = 0 },
	{ .rate = 1200000000, .m = 200, .p = 2, .s = 0, .k = 0 },
	{ .rate = 1000000000, .m = 167, .p = 2, .s = 0, .k = 0 },
	{ .rate =  800000000, .m = 133, .p = 2, .s = 0, .k = 0 },
	{ .rate =  600000000, .m = 200, .p = 4, .s = 0, .k = 0 },
	{ .rate =  500000000, .m = 250, .p = 6, .s = 0, .k = 0 },
	{ .rate =  300000000, .m = 200, .p = 8, .s = 0, .k = 0 },
	{ /* sentinel */ }
};

static const struct samsung_pll_rate_table dpll_rates[] = {
	/* DPLL: Display clock (max 1.4GHz) */
	{ .rate = 1400000000, .m = 233, .p = 2, .s = 0, .k = 0 },
	{ .rate = 1200000000, .m = 200, .p = 2, .s = 0, .k = 0 },
	{ .rate =  800000000, .m = 133, .p = 2, .s = 0, .k = 0 },
	{ .rate =  600000000, .m = 200, .p = 4, .s = 0, .k = 0 },
	{ /* sentinel */ }
};

static const struct samsung_pll_rate_table epll_rates[] = {
	/* EPLL: Audio clock */
	{ .rate = 491520000, .m = 19, .p = 3, .s = 1, .k = 0 },
	{ .rate = 245760000, .m = 19, .p = 3, .s = 2, .k = 0 },
	{ .rate = 122880000, .m = 19, .p = 3, .s = 3, .k = 0 },
	{ .rate =  61440000, .m = 19, .p = 3, .s = 4, .k = 0 },
	{ /* sentinel */ }
};

static const struct samsung_pll_rate_table vpll_rates[] = {
	/* VPLL: Video/MIPI clock */
	{ .rate = 800000000, .m = 133, .p = 2, .s = 0, .k = 0 },
	{ .rate = 600000000, .m = 200, .p = 4, .s = 0, .k = 0 },
	{ .rate = 540000000, .m = 90,  .p = 2, .s = 0, .k = 0 },
	{ /* sentinel */ }
};

static struct samsung_pll_clock exynos1280_pll_clks[] __initdata = {
	PLL(pll, CLK_IGNORE_UNUSED, "fin_pll", "xin26m", 0x0000, apll_rates),
	PLL(pll, CLK_IGNORE_UNUSED, "fin_pll", "xin26m", 0x10000, dpll_rates),
	PLL(pll, 0, "fin_pll", "xin26m", 0x14000, epll_rates),
	PLL(pll, 0, "fin_pll", "xin26m", 0x18000, vpll_rates),
};

/* ── MUX clocks ───────────────────────────────────────────────────────── */

#define MUX(_id, _name, _parent_names, _flags, _reg, _shift, _width) \
	SAMSUNG_MUX(_id, _name, _parent_names, _flags, _reg, _shift, _width)

#define MUX_F(_id, _name, _parent_names, _flags, _reg, _shift, _width, _mux_flags) \
	SAMSUNG_MUX_F(_id, _name, _parent_names, _flags, _reg, _shift, _width, _mux_flags)

static const char * const mout_apll_p[] = { "fin_pll", "fout_apll", };
static const char * const mout_dpll_p[] = { "fin_pll", "fout_dpll", };
static const char * const mout_epll_p[] = { "fin_pll", "fout_epll", };
static const char * const mout_vpll_p[] = { "fin_pll", "fout_vpll", };
static const char * const mout_cpu_p[] = { "fin_pll", "fout_apll", };

static struct samsung_mux_clock exynos1280_mux_clks[] __initdata = {
	/* CPU MUX */
	MUX_F(CLK_MOUT_APLL, "mout_apll", mout_apll_p,
	      CLK_SET_RATE_PARENT, 0x0100, 0, 1, CLK_MUX_READ_ONLY),
	MUX(CLK_MOUT_CPU, "mout_cpu", mout_cpu_p,
	    CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_RECALC, 0x0104, 0, 1),

	/* Display MUX */
	MUX(CLK_MOUT_DPLL, "mout_dpll", mout_dpll_p,
	    CLK_SET_RATE_PARENT, 0x10000, 0, 1),

	/* Audio MUX */
	MUX(CLK_MOUT_EPLL, "mout_epll", mout_epll_p,
	    CLK_SET_RATE_PARENT, 0x14000, 0, 1),

	/* Video MUX */
	MUX(CLK_MOUT_VPLL, "mout_vpll", mout_vpll_p,
	    CLK_SET_RATE_PARENT, 0x18000, 0, 1),
};

/* ── Divider clocks ───────────────────────────────────────────────────── */

#define DIV(_id, _name, _parent_name, _flags, _reg, _shift, _width, _div_flags) \
	SAMSUNG_DIV(_id, _name, _parent_name, _flags, _reg, _shift, _width, _div_flags)

static struct samsung_div_clock exynos1280_div_clks[] __initdata = {
	/* CPU dividers */
	DIV(CLK_DIV_CPU, "div_cpu", "mout_cpu",
	    CLK_SET_RATE_PARENT, 0x0200, 0, 3, 0),

	/* Bus dividers */
	DIV(CLK_DIV_BUS, "div_bus", "mout_dpll",
	    0, 0x0204, 0, 3, 0),

	/* Display dividers */
	DIV(CLK_DIV_D0_MIPI, "div_d0_mipi", "mout_dpll",
	    0, 0x0300, 0, 4, 0),

	/* Audio dividers */
	DIV(CLK_DIV_A2M, "div_a2m", "mout_epll",
	    0, 0x0310, 0, 3, 0),
};

/* ── Gate clocks ──────────────────────────────────────────────────────── */

#define GATE(_id, _name, _parent_name, _flags, _reg, _bit) \
	SAMSUNG_GATE(_id, _name, _parent_name, _flags, _reg, _bit)

static struct samsung_gate_clock exynos1280_gate_clks[] __initdata = {
	/* Display gates */
	GATE(CLK_ACLK_DECON0, "aclk_decon0", "aclk_disp",
	     CLK_IGNORE_UNUSED, 0x3000, 0),
	GATE(CLK_SCLK_DECON0, "sclk_decon0", "sclk_disp",
	     0, 0x3000, 1),
	GATE(CLK_ACLK_DSIM0, "aclk_dsim0", "aclk_disp",
	     CLK_IGNORE_UNUSED, 0x3000, 4),
	GATE(CLK_SCLK_MIPI_TX, "sclk_mipi_tx", "sclk_disp",
	     0, 0x3000, 5),
	GATE(CLK_ACLK_DMA_A, "aclk_dma_a", "aclk_disp",
	     0, 0x3000, 8),

	/* Camera gates */
	GATE(CLK_ACLK_CSIS0, "aclk_csis0", "aclk_cam",
	     CLK_IGNORE_UNUSED, 0x3004, 0),
	GATE(CLK_PCLK_CSIS0, "pclk_csis0", "pclk_cam",
	     0, 0x3004, 1),
	GATE(CLK_ACLK_CAM, "aclk_cam", "aclk_cam_noCs",
	     0, 0x3004, 4),

	/* MFC gates */
	GATE(CLK_ACLK_MFC, "aclk_mfc", "aclk_mfc_nocs",
	     CLK_IGNORE_UNUSED, 0x3008, 0),
	GATE(CLK_PCLK_MFC, "pclk_mfc", "pclk_mfc_nocs",
	     0, 0x3008, 1),

	/* GPU gates */
	GATE(CLK_ACLK_GPU, "aclk_gpu", "aclk_gpu_nocs",
	     CLK_IGNORE_UNUSED, 0x300C, 0),
	GATE(CLK_SCLK_GPU, "sclk_gpu", "sclk_gpu_nocs",
	     0, 0x300C, 1),
	GATE(CLK_CLK_GPU, "clk_gpu", "aclk_gpu",
	     0, 0x300C, 4),

	/* Audio gates */
	GATE(CLK_ACLK_AUD, "aclk_audit", "aclk_audit_nocs",
	     0, 0x3010, 0),
	GATE(CLK_SCLK_I2S0, "sclk_i2s0", "aclk_audit",
	     0, 0x3010, 4),
	GATE(CLK_PCLK_I2S0, "pclk_i2s0", "pclk_audit",
	     0, 0x3010, 5),
	GATE(CLK_SCLK_TDM0, "sclk_tdm0", "aclk_audit",
	     0, 0x3010, 8),
	GATE(CLK_PCLK_TDM0, "pclk_tdm0", "pclk_audit",
	     0, 0x3010, 9),
	GATE(CLK_SCLK_SPDIF, "sclk_spdif", "aclk_audit",
	     0, 0x3010, 12),
	GATE(CLK_PCLK_SPDIF, "pclk_spdif", "pclk_audit",
	     0, 0x3010, 13),
	GATE(CLK_PCLK_PCM0, "pclk_pcm0", "pclk_audit",
	     0, 0x3010, 16),
	GATE(CLK_SCLK_SLIF, "sclk_slif", "aclk_audit",
	     0, 0x3010, 20),
	GATE(CLK_PCLK_SLIF, "pclk_slif", "pclk_audit",
	     0, 0x3010, 21),
	GATE(CLK_SCLK_VTS, "sclk_vts", "aclk_audit",
	     CLK_IGNORE_UNUSED, 0x3010, 24),
	GATE(CLK_PCLK_VTS, "pclk_vts", "pclk_audit",
	     CLK_IGNORE_UNUSED, 0x3010, 25),

	/* UART gates */
	GATE(CLK_SCLK_UART0, "sclk_uart0", "div_uart",
	     0, 0x3020, 0),
	GATE(CLK_PCLK_UART0, "pclk_uart0", "pclk_uart",
	     0, 0x3020, 1),
	GATE(CLK_SCLK_UART1, "sclk_uart1", "div_uart",
	     0, 0x3020, 4),
	GATE(CLK_PCLK_UART1, "pclk_uart1", "pclk_uart",
	     0, 0x3020, 5),
	GATE(CLK_SCLK_UART2, "sclk_uart2", "div_uart",
	     0, 0x3020, 8),
	GATE(CLK_PCLK_UART2, "pclk_uart2", "pclk_uart",
	     0, 0x3020, 9),
	GATE(CLK_SCLK_UART3, "sclk_uart3", "div_uart",
	     0, 0x3020, 12),
	GATE(CLK_PCLK_UART3, "pclk_uart3", "pclk_uart",
	     0, 0x3020, 13),
	GATE(CLK_SCLK_UART4, "sclk_uart4", "div_uart",
	     0, 0x3020, 16),
	GATE(CLK_PCLK_UART4, "pclk_uart4", "pclk_uart",
	     0, 0x3020, 17),

	/* I2C gates */
	GATE(CLK_PCLK_I2C0, "pclk_i2c0", "pclk_peri",
	     0, 0x3030, 0),
	GATE(CLK_PCLK_I2C1, "pclk_i2c1", "pclk_peri",
	     0, 0x3030, 4),
	GATE(CLK_PCLK_I2C2, "pclk_i2c2", "pclk_peri",
	     0, 0x3030, 8),
	GATE(CLK_PCLK_I2C3, "pclk_i2c3", "pclk_peri",
	     0, 0x3030, 12),
	GATE(CLK_PCLK_I2C4, "pclk_i2c4", "pclk_peri",
	     0, 0x3030, 16),
	GATE(CLK_PCLK_I2C5, "pclk_i2c5", "pclk_peri",
	     0, 0x3030, 20),
	GATE(CLK_PCLK_I2C6, "pclk_i2c6", "pclk_peri",
	     0, 0x3030, 24),
	GATE(CLK_PCLK_I2C7, "pclk_i2c7", "pclk_peri",
	     0, 0x3030, 28),

	/* SPI gates */
	GATE(CLK_PCLK_SPI0, "pclk_spi0", "pclk_peri",
	     0, 0x3040, 0),
	GATE(CLK_PCLK_SPI1, "pclk_spi1", "pclk_peri",
	     0, 0x3040, 4),
	GATE(CLK_PCLK_SPI2, "pclk_spi2", "pclk_peri",
	     0, 0x3040, 8),
	GATE(CLK_PCLK_SPI3, "pclk_spi3", "pclk_peri",
	     0, 0x3040, 12),

	/* UFS gates */
	GATE(CLK_ACLK_UFS, "aclk_ufs", "aclk_ufs_nocs",
	     CLK_IGNORE_UNUSED, 0x3050, 0),
	GATE(CLK_SCLK_UFS, "sclk_ufs", "fin_pll",
	     0, 0x3050, 1),

	/* MMC gates */
	GATE(CLK_SCLK_MMC0, "sclk_mmc0", "fin_pll",
	     0, 0x3060, 0),
	GATE(CLK_PCLK_MMC0, "pclk_mmc0", "pclk_peri",
	     0, 0x3060, 1),
	GATE(CLK_SCLK_MMC2, "sclk_mmc2", "fin_pll",
	     0, 0x3060, 4),
	GATE(CLK_PCLK_MMC2, "pclk_mmc2", "pclk_peri",
	     0, 0x3060, 5),

	/* USB gates */
	GATE(CLK_ACLK_USB, "aclk_usb", "aclk_usb_nocs",
	     CLK_IGNORE_UNUSED, 0x3070, 0),
	GATE(CLK_SCLK_USB, "sclk_usb", "fin_pll",
	     0, 0x3070, 1),

	/* Modem gates */
	GATE(CLK_ACLK_MODEM, "aclk_modem", "aclk_modem_nocs",
	     0, 0x3080, 0),

	/* NFC gate */
	GATE(CLK_PCLK_NFC, "pclk_nfc", "pclk_peri",
	     0, 0x3090, 0),

	/* PWM gate */
	GATE(CLK_ACLK_PWM, "aclk_pwm", "aclk_peri",
	     0, 0x3090, 4),

	/* ADC gate */
	GATE(CLK_PCLK_ADCIF, "pclk_adcif", "pclk_peri",
	     0, 0x3090, 8),

	/* RTC */
	GATE(CLK_OSC_RTC, "osc_rtc", NULL,
	     CLK_IGNORE_UNUSED, 0x30A0, 0),

	/* DMA gates */
	GATE(CLK_ACLK_DMAC0, "aclk_dmac0", "aclk_bus",
	     CLK_IGNORE_UNUSED, 0x30B0, 0),
	GATE(CLK_ACLK_DMAC1, "aclk_dmac1", "aclk_bus",
	     CLK_IGNORE_UNUSED, 0x30B0, 4),
	GATE(CLK_PCLK_DMAC1, "pclk_dmac1", "pclk_bus",
	     0, 0x30B0, 5),

	/* PMU/TMU */
	GATE(CLK_PCLK_TMU, "pclk_tmu", "pclk_peri",
	     CLK_IGNORE_UNUSED, 0x30C0, 0),
	GATE(CLK_PCLK_WDT, "pclk_wdt", "pclk_peri",
	     0, 0x30C0, 4),

	/* Bus gates */
	GATE(CLK_ACLK_BUS, "aclk_bus", "div_bus",
	     CLK_IGNORE_UNUSED, 0x30D0, 0),
	GATE(CLK_PCLK_BUS, "pclk_bus", "div_bus",
	     CLK_IGNORE_UNUSED, 0x30D0, 1),
	GATE(CLK_ACLK_PERI, "aclk_peri", "aclk_bus",
	     0, 0x30D0, 4),
	GATE(CLK_PCLK_PERI, "pclk_peri", "pclk_bus",
	     0, 0x30D0, 5),

	/* Display sub-gates */
	GATE(CLK_ACLK_DISP, "aclk_disp", "div_bus",
	     0, 0x30E0, 0),
	GATE(CLK_PCLK_DISP, "pclk_disp", "div_bus",
	     0, 0x30E0, 1),
	GATE(CLK_ACLK_DISP_NOCS, "aclk_disp_noCs", "div_bus",
	     0, 0x30E0, 4),
	GATE(CLK_SCLK_DISP, "sclk_disp", "mout_dpll",
	     0, 0x30E0, 5),

	/* Camera sub-gates */
	GATE(CLK_ACLK_CAM_NOCS, "aclk_cam_noCs", "div_bus",
	     0, 0x30F0, 0),
	GATE(CLK_PCLK_CAM, "pclk_cam", "div_bus",
	     0, 0x30F0, 1),

	/* MFC sub-gates */
	GATE(CLK_ACLK_MFC_NOCS, "aclk_mfc_nocs", "div_bus",
	     0, 0x3100, 0),
	GATE(CLK_PCLK_MFC, "pclk_mfc_nocs", "div_bus",
	     0, 0x3100, 1),

	/* GPU sub-gates */
	GATE(CLK_ACLK_GPU_NOCS, "aclk_gpu_nocs", "div_bus",
	     0, 0x3110, 0),
	GATE(CLK_SCLK_GPU_NOCS, "sclk_gpu_nocs", "div_bus",
	     0, 0x3110, 1),

	/* UFS sub-gates */
	GATE(CLK_ACLK_UFS_NOCS, "aclk_ufs_nocs", "div_bus",
	     0, 0x3120, 0),

	/* Modem sub-gates */
	GATE(CLK_ACLK_MODEM_NOCS, "aclk_modem_nocs", "div_bus",
	     0, 0x3130, 0),

	/* USB sub-gates */
	GATE(CLK_ACLK_USB_NOCS, "aclk_usb_nocs", "div_bus",
	     0, 0x3140, 0),

	/* Audit sub-gates */
	GATE(CLK_ACLK_AUD_NOCS, "aclk_audit_nocs", "div_bus",
	     0, 0x3150, 0),
	GATE(CLK_PCLK_AUD, "pclk_audit", "div_bus",
	     0, 0x3150, 1),

	/* CPU sub-gates */
	GATE(CLK_ACLK_CPU, "aclk_cpu", "div_cpu",
	     0, 0x3160, 0),
	GATE(CLK_PCLK_CPU, "pclk_cpu", "div_cpu",
	     0, 0x3160, 1),
};

/* ── Clock init data ──────────────────────────────────────────────────── */

static void __init exynos1280_clk_init(struct device_node *np)
{
	struct samsung_clk_provider *ctx;
	void __iomem *base;

	base = of_iomap(np, 0);
	if (!base) {
		pr_err("exynos1280-clk: failed to map clock registers\n");
		return;
	}

	ctx = samsung_clk_init(np, base, CLK_MAX_CLKS);
	if (IS_ERR(ctx)) {
		pr_err("exynos1280-clk: failed to init clock provider\n");
		iounmap(base);
		return;
	}

	samsung_clk_register_fixed_rate(ctx, exynos1280_fr_clks,
					ARRAY_SIZE(exynos1280_fr_clks));

	samsung_clk_register_pll(ctx, exynos1280_pll_clks,
				 ARRAY_SIZE(exynos1280_pll_clks));

	samsung_clk_register_mux(ctx, exynos1280_mux_clks,
				 ARRAY_SIZE(exynos1280_mux_clks));

	samsung_clk_register_div(ctx, exynos1280_div_clks,
				 ARRAY_SIZE(exynos1280_div_clks));

	samsung_clk_register_gate(ctx, exynos1280_gate_clks,
				  ARRAY_SIZE(exynos1280_gate_clks));

	samsung_clk_add_provider(ctx);

	pr_info("exynos1280-clk: clock controller initialized\n");
}

CLK_OF_DECLARE(exynos1280_clk, "samsung,exynos1280-clock",
	       exynos1280_clk_init);
