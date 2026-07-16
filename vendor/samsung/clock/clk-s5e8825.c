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
 *
 * NOTE: All register offsets below are placeholders.
 * Real offsets must be obtained from Samsung kernel source or TRM.
 * The structure follows samsung_clk patterns from clk-exynos850.c.
 */

/* ── Fixed-rate clocks ────────────────────────────────────────────────── */

static SAMSUNG_FIXED_CLK(xin26m_clk, CLK_IGNORE_UNUSED, 26000000);

static struct samsung_fixed_rate_clock exynos1280_fr_clks[] __initdata = {
	SAMSUNG_FIXED_CLK_INIT(xin26m_clk),
};

/* ── PLL clocks ───────────────────────────────────────────────────────── */

static const struct samsung_pll_rate_table apll_rates[] = {
	/* APLL: CPU clock (APLL_CON0 at 0x0000) */
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
	/* DPLL: Display clock (DPLL_CON0 at 0x10000) */
	{ .rate = 1400000000, .m = 233, .p = 2, .s = 0, .k = 0 },
	{ .rate = 1200000000, .m = 200, .p = 2, .s = 0, .k = 0 },
	{ .rate =  800000000, .m = 133, .p = 2, .s = 0, .k = 0 },
	{ .rate =  600000000, .m = 200, .p = 4, .s = 0, .k = 0 },
	{ /* sentinel */ }
};

static const struct samsung_pll_rate_table epll_rates[] = {
	/* EPLL: Audio clock (EPLL_CON0 at 0x14000) */
	{ .rate = 491520000, .m = 19, .p = 3, .s = 1, .k = 0 },
	{ .rate = 245760000, .m = 19, .p = 3, .s = 2, .k = 0 },
	{ .rate = 122880000, .m = 19, .p = 3, .s = 3, .k = 0 },
	{ .rate =  61440000, .m = 19, .p = 3, .s = 4, .k = 0 },
	{ /* sentinel */ }
};

static const struct samsung_pll_rate_table vpll_rates[] = {
	/* VPLL: Video/MIPI clock (VPLL_CON0 at 0x18000) */
	{ .rate = 800000000, .m = 133, .p = 2, .s = 0, .k = 0 },
	{ .rate = 600000000, .m = 200, .p = 4, .s = 0, .k = 0 },
	{ .rate = 540000000, .m = 90,  .p = 2, .s = 0, .k = 0 },
	{ /* sentinel */ }
};

static SAMSUNG_PLL(apll_pll, CLK_IGNORE_UNUSED, "fin_pll",
		   0x0000, apll_rates);
static SAMSUNG_PLL(dpll_pll, CLK_IGNORE_UNUSED, "fin_pll",
		   0x10000, dpll_rates);
static SAMSUNG_PLL(epll_pll, 0, "fin_pll",
		   0x14000, epll_rates);
static SAMSUNG_PLL(vpll_pll, 0, "fin_pll",
		   0x18000, vpll_rates);

static struct samsung_pll_clock exynos1280_pll_clks[] __initdata = {
	SAMSUNG_PLL_INIT(apll_pll),
	SAMSUNG_PLL_INIT(dpll_pll),
	SAMSUNG_PLL_INIT(epll_pll),
	SAMSUNG_PLL_INIT(vpll_pll),
};

/* ── MUX clocks ───────────────────────────────────────────────────────── */

static const char * const mout_apll_p[] = { "fin_pll", "fout_apll", };
static const char * const mout_dpll_p[] = { "fin_pll", "fout_dpll", };
static const char * const mout_epll_p[] = { "fin_pll", "fout_epll", };
static const char * const mout_vpll_p[] = { "fin_pll", "fout_vpll", };
static const char * const mout_cpu_p[] = { "fin_pll", "fout_apll", };

static SAMSUNG_MUX_F(mout_apll, "mout_apll", mout_apll_p,
		     CLK_SET_RATE_PARENT, 0x0100, 0, 1, CLK_MUX_READ_ONLY);
static SAMSUNG_MUX(mout_cpu, "mout_cpu", mout_cpu_p,
		   CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_RECALC,
		   0x0104, 0, 1);
static SAMSUNG_MUX(mout_dpll, "mout_dpll", mout_dpll_p,
		   CLK_SET_RATE_PARENT, 0x10000, 0, 1);
static SAMSUNG_MUX(mout_epll, "mout_epll", mout_epll_p,
		   CLK_SET_RATE_PARENT, 0x14000, 0, 1);
static SAMSUNG_MUX(mout_vpll, "mout_vpll", mout_vpll_p,
		   CLK_SET_RATE_PARENT, 0x18000, 0, 1);

static struct samsung_mux_clock exynos1280_mux_clks[] __initdata = {
	SAMSUNG_MUX_INIT(mout_apll),
	SAMSUNG_MUX_INIT(mout_cpu),
	SAMSUNG_MUX_INIT(mout_dpll),
	SAMSUNG_MUX_INIT(mout_epll),
	SAMSUNG_MUX_INIT(mout_vpll),
};

/* ── Divider clocks ───────────────────────────────────────────────────── */

static SAMSUNG_DIV(div_cpu, "div_cpu", "mout_cpu",
		   CLK_SET_RATE_PARENT, 0x0200, 0, 3);
static SAMSUNG_DIV(div_bus, "div_bus", "mout_dpll",
		   0, 0x0204, 0, 3);
static SAMSUNG_DIV(div_d0_mipi, "div_d0_mipi", "mout_dpll",
		   0, 0x0300, 0, 4);
static SAMSUNG_DIV(div_a2m, "div_a2m", "mout_epll",
		   0, 0x0310, 0, 3);

static struct samsung_div_clock exynos1280_div_clks[] __initdata = {
	SAMSUNG_DIV_INIT(div_cpu),
	SAMSUNG_DIV_INIT(div_bus),
	SAMSUNG_DIV_INIT(div_d0_mipi),
	SAMSUNG_DIV_INIT(div_a2m),
};

/* ── Gate clocks ──────────────────────────────────────────────────────── */

static SAMSUNG_GATE(aclk_decon0, "aclk_decon0", "aclk_disp",
		    CLK_IGNORE_UNUSED, 0x3000, 0);
static SAMSUNG_GATE(sclk_decon0, "sclk_decon0", "sclk_disp",
		    0, 0x3000, 1);
static SAMSUNG_GATE(aclk_dsim0, "aclk_dsim0", "aclk_disp",
		    CLK_IGNORE_UNUSED, 0x3000, 4);
static SAMSUNG_GATE(sclk_mipi_tx, "sclk_mipi_tx", "sclk_disp",
		    0, 0x3000, 5);
static SAMSUNG_GATE(aclk_dma_a, "aclk_dma_a", "aclk_disp",
		    0, 0x3000, 8);

static SAMSUNG_GATE(aclk_csis0, "aclk_csis0", "aclk_cam",
		    CLK_IGNORE_UNUSED, 0x3004, 0);
static SAMSUNG_GATE(pclk_csis0, "pclk_csis0", "pclk_cam",
		    0, 0x3004, 1);
static SAMSUNG_GATE(aclk_cam, "aclk_cam", "aclk_cam_noCs",
		    0, 0x3004, 4);

static SAMSUNG_GATE(aclk_mfc, "aclk_mfc", "aclk_mfc_nocs",
		    CLK_IGNORE_UNUSED, 0x3008, 0);
static SAMSUNG_GATE(pclk_mfc, "pclk_mfc", "pclk_mfc_nocs",
		    0, 0x3008, 1);

static SAMSUNG_GATE(aclk_gpu, "aclk_gpu", "aclk_gpu_nocs",
		    CLK_IGNORE_UNUSED, 0x300C, 0);
static SAMSUNG_GATE(sclk_gpu, "sclk_gpu", "sclk_gpu_nocs",
		    0, 0x300C, 1);
static SAMSUNG_GATE(clk_gpu, "clk_gpu", "aclk_gpu",
		    0, 0x300C, 4);

static SAMSUNG_GATE(aclk_aud, "aclk_audit", "aclk_audit_nocs",
		    0, 0x3010, 0);
static SAMSUNG_GATE(sclk_i2s0, "sclk_i2s0", "aclk_audit",
		    0, 0x3010, 4);
static SAMSUNG_GATE(pclk_i2s0, "pclk_i2s0", "pclk_audit",
		    0, 0x3010, 5);
static SAMSUNG_GATE(sclk_tdm0, "sclk_tdm0", "aclk_audit",
		    0, 0x3010, 8);
static SAMSUNG_GATE(pclk_tdm0, "pclk_tdm0", "pclk_audit",
		    0, 0x3010, 9);
static SAMSUNG_GATE(sclk_spdif, "sclk_spdif", "aclk_audit",
		    0, 0x3010, 12);
static SAMSUNG_GATE(pclk_spdif, "pclk_spdif", "pclk_audit",
		    0, 0x3010, 13);
static SAMSUNG_GATE(pclk_pcm0, "pclk_pcm0", "pclk_audit",
		    0, 0x3010, 16);
static SAMSUNG_GATE(sclk_slif, "sclk_slif", "aclk_audit",
		    0, 0x3010, 20);
static SAMSUNG_GATE(pclk_slif, "pclk_slif", "pclk_audit",
		    0, 0x3010, 21);
static SAMSUNG_GATE(sclk_vts, "sclk_vts", "aclk_audit",
		    CLK_IGNORE_UNUSED, 0x3010, 24);
static SAMSUNG_GATE(pclk_vts, "pclk_vts", "pclk_audit",
		    CLK_IGNORE_UNUSED, 0x3010, 25);

static SAMSUNG_GATE(sclk_uart0, "sclk_uart0", "div_uart",
		    0, 0x3020, 0);
static SAMSUNG_GATE(pclk_uart0, "pclk_uart0", "pclk_uart",
		    0, 0x3020, 1);
static SAMSUNG_GATE(sclk_uart1, "sclk_uart1", "div_uart",
		    0, 0x3020, 4);
static SAMSUNG_GATE(pclk_uart1, "pclk_uart1", "pclk_uart",
		    0, 0x3020, 5);
static SAMSUNG_GATE(sclk_uart2, "sclk_uart2", "div_uart",
		    0, 0x3020, 8);
static SAMSUNG_GATE(pclk_uart2, "pclk_uart2", "pclk_uart",
		    0, 0x3020, 9);
static SAMSUNG_GATE(sclk_uart3, "sclk_uart3", "div_uart",
		    0, 0x3020, 12);
static SAMSUNG_GATE(pclk_uart3, "pclk_uart3", "pclk_uart",
		    0, 0x3020, 13);
static SAMSUNG_GATE(sclk_uart4, "sclk_uart4", "div_uart",
		    0, 0x3020, 16);
static SAMSUNG_GATE(pclk_uart4, "pclk_uart4", "pclk_uart",
		    0, 0x3020, 17);

static SAMSUNG_GATE(pclk_i2c0, "pclk_i2c0", "pclk_peri",
		    0, 0x3030, 0);
static SAMSUNG_GATE(pclk_i2c1, "pclk_i2c1", "pclk_peri",
		    0, 0x3030, 4);
static SAMSUNG_GATE(pclk_i2c2, "pclk_i2c2", "pclk_peri",
		    0, 0x3030, 8);
static SAMSUNG_GATE(pclk_i2c3, "pclk_i2c3", "pclk_peri",
		    0, 0x3030, 12);
static SAMSUNG_GATE(pclk_i2c4, "pclk_i2c4", "pclk_peri",
		    0, 0x3030, 16);
static SAMSUNG_GATE(pclk_i2c5, "pclk_i2c5", "pclk_peri",
		    0, 0x3030, 20);
static SAMSUNG_GATE(pclk_i2c6, "pclk_i2c6", "pclk_peri",
		    0, 0x3030, 24);
static SAMSUNG_GATE(pclk_i2c7, "pclk_i2c7", "pclk_peri",
		    0, 0x3030, 28);

static SAMSUNG_GATE(pclk_spi0, "pclk_spi0", "pclk_peri",
		    0, 0x3040, 0);
static SAMSUNG_GATE(pclk_spi1, "pclk_spi1", "pclk_peri",
		    0, 0x3040, 4);
static SAMSUNG_GATE(pclk_spi2, "pclk_spi2", "pclk_peri",
		    0, 0x3040, 8);
static SAMSUNG_GATE(pclk_spi3, "pclk_spi3", "pclk_peri",
		    0, 0x3040, 12);

static SAMSUNG_GATE(aclk_ufs, "aclk_ufs", "aclk_ufs_nocs",
		    CLK_IGNORE_UNUSED, 0x3050, 0);
static SAMSUNG_GATE(sclk_ufs, "sclk_ufs", "fin_pll",
		    0, 0x3050, 1);

static SAMSUNG_GATE(sclk_mmc0, "sclk_mmc0", "fin_pll",
		    0, 0x3060, 0);
static SAMSUNG_GATE(pclk_mmc0, "pclk_mmc0", "pclk_peri",
		    0, 0x3060, 1);
static SAMSUNG_GATE(sclk_mmc2, "sclk_mmc2", "fin_pll",
		    0, 0x3060, 4);
static SAMSUNG_GATE(pclk_mmc2, "pclk_mmc2", "pclk_peri",
		    0, 0x3060, 5);

static SAMSUNG_GATE(aclk_usb, "aclk_usb", "aclk_usb_nocs",
		    CLK_IGNORE_UNUSED, 0x3070, 0);
static SAMSUNG_GATE(sclk_usb, "sclk_usb", "fin_pll",
		    0, 0x3070, 1);

static SAMSUNG_GATE(aclk_modem, "aclk_modem", "aclk_modem_nocs",
		    0, 0x3080, 0);

static SAMSUNG_GATE(pclk_nfc, "pclk_nfc", "pclk_peri",
		    0, 0x3090, 0);
static SAMSUNG_GATE(aclk_pwm, "aclk_pwm", "aclk_peri",
		    0, 0x3090, 4);
static SAMSUNG_GATE(pclk_adcif, "pclk_adcif", "pclk_peri",
		    0, 0x3090, 8);

static SAMSUNG_GATE(osc_rtc, "osc_rtc", NULL,
		    CLK_IGNORE_UNUSED, 0x30A0, 0);

static SAMSUNG_GATE(aclk_dmac0, "aclk_dmac0", "aclk_bus",
		    CLK_IGNORE_UNUSED, 0x30B0, 0);
static SAMSUNG_GATE(aclk_dmac1, "aclk_dmac1", "aclk_bus",
		    CLK_IGNORE_UNUSED, 0x30B0, 4);
static SAMSUNG_GATE(pclk_dmac1, "pclk_dmac1", "pclk_bus",
		    0, 0x30B0, 5);

static SAMSUNG_GATE(pclk_tmu, "pclk_tmu", "pclk_peri",
		    CLK_IGNORE_UNUSED, 0x30C0, 0);
static SAMSUNG_GATE(pclk_wdt, "pclk_wdt", "pclk_peri",
		    0, 0x30C0, 4);

static SAMSUNG_GATE(aclk_bus, "aclk_bus", "div_bus",
		    CLK_IGNORE_UNUSED, 0x30D0, 0);
static SAMSUNG_GATE(pclk_bus, "pclk_bus", "div_bus",
		    CLK_IGNORE_UNUSED, 0x30D0, 1);
static SAMSUNG_GATE(aclk_peri, "aclk_peri", "aclk_bus",
		    0, 0x30D0, 4);
static SAMSUNG_GATE(pclk_peri, "pclk_peri", "pclk_bus",
		    0, 0x30D0, 5);

static SAMSUNG_GATE(aclk_disp, "aclk_disp", "div_bus",
		    0, 0x30E0, 0);
static SAMSUNG_GATE(pclk_disp, "pclk_disp", "div_bus",
		    0, 0x30E0, 1);
static SAMSUNG_GATE(aclk_disp_nocs, "aclk_disp_noCs", "div_bus",
		    0, 0x30E0, 4);
static SAMSUNG_GATE(sclk_disp, "sclk_disp", "mout_dpll",
		    0, 0x30E0, 5);

static SAMSUNG_GATE(aclk_cam_nocs, "aclk_cam_noCs", "div_bus",
		    0, 0x30F0, 0);
static SAMSUNG_GATE(pclk_cam, "pclk_cam", "div_bus",
		    0, 0x30F0, 1);

static SAMSUNG_GATE(aclk_mfc_nocs, "aclk_mfc_nocs", "div_bus",
		    0, 0x3100, 0);
static SAMSUNG_GATE(pclk_mfc_nocs, "pclk_mfc_nocs", "div_bus",
		    0, 0x3100, 1);

static SAMSUNG_GATE(aclk_gpu_nocs, "aclk_gpu_nocs", "div_bus",
		    0, 0x3110, 0);
static SAMSUNG_GATE(sclk_gpu_nocs, "sclk_gpu_nocs", "div_bus",
		    0, 0x3110, 1);

static SAMSUNG_GATE(aclk_ufs_nocs, "aclk_ufs_nocs", "div_bus",
		    0, 0x3120, 0);

static SAMSUNG_GATE(aclk_modem_nocs, "aclk_modem_nocs", "div_bus",
		    0, 0x3130, 0);

static SAMSUNG_GATE(aclk_usb_nocs, "aclk_usb_nocs", "div_bus",
		    0, 0x3140, 0);

static SAMSUNG_GATE(aclk_audit_nocs, "aclk_audit_nocs", "div_bus",
		    0, 0x3150, 0);
static SAMSUNG_GATE(pclk_audit, "pclk_audit", "div_bus",
		    0, 0x3150, 1);

static SAMSUNG_GATE(aclk_cpu, "aclk_cpu", "div_cpu",
		    0, 0x3160, 0);
static SAMSUNG_GATE(pclk_cpu, "pclk_cpu", "div_cpu",
		    0, 0x3160, 1);

static struct samsung_gate_clock exynos1280_gate_clks[] __initdata = {
	SAMSUNG_GATE_INIT(aclk_decon0),
	SAMSUNG_GATE_INIT(sclk_decon0),
	SAMSUNG_GATE_INIT(aclk_dsim0),
	SAMSUNG_GATE_INIT(sclk_mipi_tx),
	SAMSUNG_GATE_INIT(aclk_dma_a),

	SAMSUNG_GATE_INIT(aclk_csis0),
	SAMSUNG_GATE_INIT(pclk_csis0),
	SAMSUNG_GATE_INIT(aclk_cam),

	SAMSUNG_GATE_INIT(aclk_mfc),
	SAMSUNG_GATE_INIT(pclk_mfc),

	SAMSUNG_GATE_INIT(aclk_gpu),
	SAMSUNG_GATE_INIT(sclk_gpu),
	SAMSUNG_GATE_INIT(clk_gpu),

	SAMSUNG_GATE_INIT(aclk_aud),
	SAMSUNG_GATE_INIT(sclk_i2s0),
	SAMSUNG_GATE_INIT(pclk_i2s0),
	SAMSUNG_GATE_INIT(sclk_tdm0),
	SAMSUNG_GATE_INIT(pclk_tdm0),
	SAMSUNG_GATE_INIT(sclk_spdif),
	SAMSUNG_GATE_INIT(pclk_spdif),
	SAMSUNG_GATE_INIT(pclk_pcm0),
	SAMSUNG_GATE_INIT(sclk_slif),
	SAMSUNG_GATE_INIT(pclk_slif),
	SAMSUNG_GATE_INIT(sclk_vts),
	SAMSUNG_GATE_INIT(pclk_vts),

	SAMSUNG_GATE_INIT(sclk_uart0),
	SAMSUNG_GATE_INIT(pclk_uart0),
	SAMSUNG_GATE_INIT(sclk_uart1),
	SAMSUNG_GATE_INIT(pclk_uart1),
	SAMSUNG_GATE_INIT(sclk_uart2),
	SAMSUNG_GATE_INIT(pclk_uart2),
	SAMSUNG_GATE_INIT(sclk_uart3),
	SAMSUNG_GATE_INIT(pclk_uart3),
	SAMSUNG_GATE_INIT(sclk_uart4),
	SAMSUNG_GATE_INIT(pclk_uart4),

	SAMSUNG_GATE_INIT(pclk_i2c0),
	SAMSUNG_GATE_INIT(pclk_i2c1),
	SAMSUNG_GATE_INIT(pclk_i2c2),
	SAMSUNG_GATE_INIT(pclk_i2c3),
	SAMSUNG_GATE_INIT(pclk_i2c4),
	SAMSUNG_GATE_INIT(pclk_i2c5),
	SAMSUNG_GATE_INIT(pclk_i2c6),
	SAMSUNG_GATE_INIT(pclk_i2c7),

	SAMSUNG_GATE_INIT(pclk_spi0),
	SAMSUNG_GATE_INIT(pclk_spi1),
	SAMSUNG_GATE_INIT(pclk_spi2),
	SAMSUNG_GATE_INIT(pclk_spi3),

	SAMSUNG_GATE_INIT(aclk_ufs),
	SAMSUNG_GATE_INIT(sclk_ufs),

	SAMSUNG_GATE_INIT(sclk_mmc0),
	SAMSUNG_GATE_INIT(pclk_mmc0),
	SAMSUNG_GATE_INIT(sclk_mmc2),
	SAMSUNG_GATE_INIT(pclk_mmc2),

	SAMSUNG_GATE_INIT(aclk_usb),
	SAMSUNG_GATE_INIT(sclk_usb),

	SAMSUNG_GATE_INIT(aclk_modem),

	SAMSUNG_GATE_INIT(pclk_nfc),
	SAMSUNG_GATE_INIT(aclk_pwm),
	SAMSUNG_GATE_INIT(pclk_adcif),

	SAMSUNG_GATE_INIT(osc_rtc),

	SAMSUNG_GATE_INIT(aclk_dmac0),
	SAMSUNG_GATE_INIT(aclk_dmac1),
	SAMSUNG_GATE_INIT(pclk_dmac1),

	SAMSUNG_GATE_INIT(pclk_tmu),
	SAMSUNG_GATE_INIT(pclk_wdt),

	SAMSUNG_GATE_INIT(aclk_bus),
	SAMSUNG_GATE_INIT(pclk_bus),
	SAMSUNG_GATE_INIT(aclk_peri),
	SAMSUNG_GATE_INIT(pclk_peri),

	SAMSUNG_GATE_INIT(aclk_disp),
	SAMSUNG_GATE_INIT(pclk_disp),
	SAMSUNG_GATE_INIT(aclk_disp_nocs),
	SAMSUNG_GATE_INIT(sclk_disp),

	SAMSUNG_GATE_INIT(aclk_cam_nocs),
	SAMSUNG_GATE_INIT(pclk_cam),

	SAMSUNG_GATE_INIT(aclk_mfc_nocs),
	SAMSUNG_GATE_INIT(pclk_mfc_nocs),

	SAMSUNG_GATE_INIT(aclk_gpu_nocs),
	SAMSUNG_GATE_INIT(sclk_gpu_nocs),

	SAMSUNG_GATE_INIT(aclk_ufs_nocs),

	SAMSUNG_GATE_INIT(aclk_modem_nocs),

	SAMSUNG_GATE_INIT(aclk_usb_nocs),

	SAMSUNG_GATE_INIT(aclk_audit_nocs),
	SAMSUNG_GATE_INIT(pclk_audit),

	SAMSUNG_GATE_INIT(aclk_cpu),
	SAMSUNG_GATE_INIT(pclk_cpu),
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
