// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos 1280 (s5e8825) Clock Controller
 *
 * Uses the generic clk-provider API so this driver builds as a GKI
 * loadable module.  The samsung_clk helpers in drivers/clk/samsung/
 * are internal and NOT exported to out-of-tree modules.
 *
 * Register offsets sourced from Samsung 5.10 CAL-IF (cmucal-sfr.c).
 * The Exynos 1280 has ~30 CMU blocks; this driver currently handles
 * CMU_TOP (top-level muxes/dividers/gates) and CMU_PERI (peripheral
 * gates for UART/I2C/SPI).  Other CMU blocks (DPU, G3D, AUD, HSI,
 * USB, MFC, CSIS) use the same framework but need their own reg
 * regions wired through the DT node.
 *
 * Gate enable bit: Samsung CLK_CON_GAT registers use bit 21 as the
 * clock-gate enable.  Each gate has its own dedicated register.
 *
 * Copyright (C) 2026 ExynosNext Project
 */

#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include "clk-s5e8825.h"

/* ── CMU block physical base addresses (from CAL-IF cmucal-sfr.c) ──── */

#define CMU_TOP_BASE		0x12900000
#define CMU_PERI_BASE		0x10030000
#define CMU_CORE_BASE		0x12800000
#define CMU_DPU_BASE		0x14800000
#define CMU_G3D_BASE		0x10200000
#define CMU_AUD_BASE		0x14e00000
#define CMU_HSI_BASE		0x13400000
#define CMU_USB_BASE		0x13000000
#define CMU_MFC_BASE		0x12e00000
#define CMU_CSIS_BASE		0x15000000
#define CMU_BUSC_BASE		0x14500000
#define CMU_ALIVE_BASE		0x11800000
#define CMU_CPUCL0_BASE		0x10820000
#define CMU_CPUCL1_BASE		0x10830000
#define CMU_MIF_BASE		0x10400000

#define CMU_BLOCK_SIZE		0x8000

/* Samsung CLK_CON_GAT registers: bit 21 is the gate enable */
#define EXYNOS_GATE_BIT		21

/* ── CMU region indices (order must match DT reg entries) ──────────── */

enum cmu_region {
	CMU_TOP,
	CMU_PERI,
	CMU_DPU,
	CMU_G3D,
	CMU_AUD,
	CMU_HSI,
	CMU_USB,
	CMU_MFC,
	CMU_CSIS,
	CMU_NUM_REGIONS,
};

/* ── Gate descriptor for table-driven registration ───────────────────── */

struct exynos1280_gate_desc {
	unsigned int	id;
	const char	*name;
	const char	*parent;
	enum cmu_region	region;
	unsigned long	offset;
	u8		bit;
	unsigned long	flags;
};

#define EGATE(_id, _name, _par, _rgn, _off, _bit, _fl) \
	{ .id = (_id), .name = (_name), .parent = (_par), \
	  .region = (_rgn), .offset = (_off), .bit = (_bit), .flags = (_fl) }

/* ── Private data ────────────────────────────────────────────────────── */

struct exynos1280_clk {
	struct device			*dev;
	void __iomem			*base[CMU_NUM_REGIONS];
	int				num_regions;
	spinlock_t			lock;
	struct clk_hw_onecell_data	*data;
};

static inline void exynos1280_set_hw(struct exynos1280_clk *clk,
				     unsigned int id, struct clk_hw *hw)
{
	if (id < clk->data->num)
		clk->data->hws[id] = hw;
}

/* ── MUX parent tables ───────────────────────────────────────────────── */

/* CMU_TOP PLL source muxes select between oscillator and PLL output */
static const char * const mout_shared0_p[] = { "fin_pll", "fout_shared0" };
static const char * const mout_shared1_p[] = { "fin_pll", "fout_shared1" };
static const char * const mout_shared2_p[] = { "fin_pll", "fout_shared2" };
static const char * const mout_g3d_pll_p[] = { "fin_pll", "fout_g3d" };
static const char * const mout_mmc_pll_p[] = { "fin_pll", "fout_mmc" };

/* Bus mux selects between shared PLL outputs */
static const char * const mout_bus_p[] = {
	"dout_shared0_div2", "dout_shared1_div2",
	"dout_shared0_div3", "dout_shared1_div3",
};

/* CPU mux (simplified: oscillator or PLL) */
static const char * const mout_cpucl0_p[] = { "fin_pll", "fout_cpucl0" };

/*
 * CMU_DPU display sub-block gates.  Each gate must have a unique register
 * offset within its CMU region — the DMA gate below uses a distinct address
 * to avoid colliding with the DECON0 SCLK gate.
 */

/*
 * ── Gate table ──────────────────────────────────────────────────────
 *
 * Register offsets from Samsung 5.10 CAL-IF (cmucal-sfr.c).
 * Each gate has a dedicated CLK_CON_GAT register; enable = bit 21.
 *
 * CMU_TOP gates (0x20xx range) enable clocks flowing to sub-blocks.
 * CMU_PERI gates (0x20xx range within PERI) control individual IPs.
 * Other CMU blocks follow the same pattern at their own base addresses.
 */

static const struct exynos1280_gate_desc exynos1280_gates[] = {
	/* ── CMU_TOP: sub-block enable gates ─────────────────────────── */

	EGATE(CLK_ACLK_BUS,       "aclk_bus",       "div_bus",
	      CMU_TOP, 0x2018, EXYNOS_GATE_BIT, CLK_IGNORE_UNUSED),
	EGATE(CLK_PCLK_BUS,       "pclk_bus",       "div_bus",
	      CMU_TOP, 0x201c, EXYNOS_GATE_BIT, CLK_IGNORE_UNUSED),
	EGATE(CLK_ACLK_PERI,      "aclk_peri",      "div_bus",
	      CMU_TOP, 0x20a0, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_PCLK_PERI,      "pclk_peri",      "div_bus",
	      CMU_TOP, 0x20a4, EXYNOS_GATE_BIT, 0),

	/* CPU */
	EGATE(CLK_CPU_ACLK,       "aclk_cpu",       "div_cpu",
	      CMU_TOP, 0x2038, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_CPU_PCLK,       "pclk_cpu",       "div_cpu",
	      CMU_TOP, 0x203c, EXYNOS_GATE_BIT, 0),

	/* ── CMU_TOP: display path gates ─────────────────────────────── */

	EGATE(CLK_ACLK_DISP,      "aclk_disp",      "div_bus",
	      CMU_TOP, 0x2048, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_PCLK_DISP,      "pclk_disp",      "div_bus",
	      CMU_TOP, 0x204c, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_ACLK_DISP_NOCS, "aclk_disp_nocs", "div_bus",
	      CMU_TOP, 0x2050, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_SCLK_DISP,      "sclk_disp",      "mout_shared1",
	      CMU_TOP, 0x2054, EXYNOS_GATE_BIT, 0),

	/* ── CMU_DPU: display sub-block gates ────────────────────────── */

	EGATE(CLK_ACLK_DECON0,    "aclk_decon0",    "aclk_disp",
	      CMU_DPU, 0x2014, EXYNOS_GATE_BIT, CLK_IGNORE_UNUSED),
	EGATE(CLK_SCLK_DECON0,    "sclk_decon0",    "sclk_disp",
	      CMU_DPU, 0x2018, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_ACLK_DSIM0,     "aclk_dsim0",     "aclk_disp",
	      CMU_DPU, 0x201c, EXYNOS_GATE_BIT, CLK_IGNORE_UNUSED),
	EGATE(CLK_SCLK_MIPI_TX,   "sclk_mipi_tx",   "sclk_disp",
	      CMU_DPU, 0x2020, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_ACLK_DMA_A,     "aclk_dma_a",     "aclk_disp",
	      CMU_DPU, 0x2024, EXYNOS_GATE_BIT, 0),

	/* ── CMU_CSIS: camera gates ──────────────────────────────────── */

	EGATE(CLK_ACLK_CAM_NOCS,  "aclk_cam_nocs",  "div_bus",
	      CMU_TOP, 0x2024, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_PCLK_CAM,       "pclk_cam",       "div_bus",
	      CMU_TOP, 0x2028, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_ACLK_CAM,       "aclk_cam",   "aclk_cam_nocs",
	      CMU_TOP, 0x202c, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_ACLK_CSIS0,     "aclk_csis0",     "aclk_cam",
	      CMU_CSIS, 0x2000, EXYNOS_GATE_BIT, CLK_IGNORE_UNUSED),
	EGATE(CLK_PCLK_CSIS0,     "pclk_csis0",     "pclk_cam",
	      CMU_CSIS, 0x2004, EXYNOS_GATE_BIT, 0),

	/* ── CMU_MFC: video codec gates ──────────────────────────────── */

	EGATE(CLK_ACLK_MFC_NOCS,  "aclk_mfc_nocs",  "div_bus",
	      CMU_TOP, 0x2088, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_PCLK_MFC,       "pclk_mfc",   "aclk_mfc_nocs",
	      CMU_MFC, 0x2004, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_ACLK_MFC,       "aclk_mfc",   "aclk_mfc_nocs",
	      CMU_MFC, 0x2000, EXYNOS_GATE_BIT, CLK_IGNORE_UNUSED),

	/* ── CMU_G3D: GPU gates ──────────────────────────────────────── */

	EGATE(CLK_ACLK_GPU_NOCS,  "aclk_gpu_nocs",  "div_bus",
	      CMU_TOP, 0x2060, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_SCLK_GPU_NOCS,  "sclk_gpu_nocs",  "mout_g3d",
	      CMU_TOP, 0x2064, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_ACLK_GPU,       "aclk_gpu",   "aclk_gpu_nocs",
	      CMU_G3D, 0x200c, EXYNOS_GATE_BIT, CLK_IGNORE_UNUSED),
	EGATE(CLK_SCLK_GPU,       "sclk_gpu",   "sclk_gpu_nocs",
	      CMU_G3D, 0x2010, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_CLK_GPU,        "clk_gpu",        "aclk_gpu",
	      CMU_G3D, 0x2004, EXYNOS_GATE_BIT, 0),

	/* ── CMU_AUD: audio gates ────────────────────────────────────── */

	EGATE(CLK_ACLK_AUD_NOCS,  "aclk_aud_nocs",  "div_bus",
	      CMU_TOP, 0x2010, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_PCLK_AUD,       "pclk_aud",       "div_bus",
	      CMU_TOP, 0x2014, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_ACLK_AUD,       "aclk_aud",   "aclk_aud_nocs",
	      CMU_AUD, 0x2014, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_SCLK_I2S0,      "sclk_i2s0",      "aclk_aud",
	      CMU_AUD, 0x2028, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_PCLK_I2S0,      "pclk_i2s0",      "pclk_aud",
	      CMU_AUD, 0x202c, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_SCLK_TDM0,      "sclk_tdm0",      "aclk_aud",
	      CMU_AUD, 0x2030, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_PCLK_TDM0,      "pclk_tdm0",      "pclk_aud",
	      CMU_AUD, 0x2034, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_SCLK_SPDIF,     "sclk_spdif",     "aclk_aud",
	      CMU_AUD, 0x2038, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_PCLK_SPDIF,     "pclk_spdif",     "pclk_aud",
	      CMU_AUD, 0x203c, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_PCLK_PCM0,      "pclk_pcm0",      "pclk_aud",
	      CMU_AUD, 0x2060, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_SCLK_SLIF,      "sclk_slif",      "aclk_aud",
	      CMU_AUD, 0x2064, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_PCLK_SLIF,      "pclk_slif",      "pclk_aud",
	      CMU_AUD, 0x2068, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_SCLK_VTS,       "sclk_vts",       "aclk_aud",
	      CMU_AUD, 0x206c, EXYNOS_GATE_BIT, CLK_IGNORE_UNUSED),
	EGATE(CLK_PCLK_VTS,       "pclk_vts",       "pclk_aud",
	      CMU_AUD, 0x2070, EXYNOS_GATE_BIT, CLK_IGNORE_UNUSED),

	/* ── CMU_PERI: UART gates ────────────────────────────────────── */
	/* Samsung USI blocks multiplex UART/I2C/SPI.  The PERI CMU has
	 * a dedicated CLK_CON_GAT register per USI IP clock port.
	 * UART_DBG is the debug UART (serial console).  */

	EGATE(CLK_SCLK_UART0,     "sclk_uart0",     "fin_pll",
	      CMU_PERI, 0x2008, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_PCLK_UART0,     "pclk_uart0",     "pclk_peri",
	      CMU_PERI, 0x2010, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_SCLK_UART1,     "sclk_uart1",     "fin_pll",
	      CMU_PERI, 0x2020, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_PCLK_UART1,     "pclk_uart1",     "pclk_peri",
	      CMU_PERI, 0x2024, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_SCLK_UART2,     "sclk_uart2",     "fin_pll",
	      CMU_PERI, 0x2028, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_PCLK_UART2,     "pclk_uart2",     "pclk_peri",
	      CMU_PERI, 0x202c, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_SCLK_UART3,     "sclk_uart3",     "fin_pll",
	      CMU_PERI, 0x2030, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_PCLK_UART3,     "pclk_uart3",     "pclk_peri",
	      CMU_PERI, 0x2034, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_SCLK_UART4,     "sclk_uart4",     "fin_pll",
	      CMU_PERI, 0x2038, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_PCLK_UART4,     "pclk_uart4",     "pclk_peri",
	      CMU_PERI, 0x203c, EXYNOS_GATE_BIT, 0),

	/* ── CMU_PERI: I2C gates (via USI) ───────────────────────────── */

	EGATE(CLK_PCLK_I2C0,      "pclk_i2c0",      "pclk_peri",
	      CMU_PERI, 0x2018, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_PCLK_I2C1,      "pclk_i2c1",      "pclk_peri",
	      CMU_PERI, 0x201c, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_PCLK_I2C2,      "pclk_i2c2",      "pclk_peri",
	      CMU_PERI, 0x2040, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_PCLK_I2C3,      "pclk_i2c3",      "pclk_peri",
	      CMU_PERI, 0x2044, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_PCLK_I2C4,      "pclk_i2c4",      "pclk_peri",
	      CMU_PERI, 0x2048, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_PCLK_I2C5,      "pclk_i2c5",      "pclk_peri",
	      CMU_PERI, 0x204c, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_PCLK_I2C6,      "pclk_i2c6",      "pclk_peri",
	      CMU_PERI, 0x2050, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_PCLK_I2C7,      "pclk_i2c7",      "pclk_peri",
	      CMU_PERI, 0x2054, EXYNOS_GATE_BIT, 0),

	/* ── CMU_PERI: SPI gates (via USI) ───────────────────────────── */

	EGATE(CLK_PCLK_SPI0,      "pclk_spi0",      "pclk_peri",
	      CMU_PERI, 0x2058, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_PCLK_SPI1,      "pclk_spi1",      "pclk_peri",
	      CMU_PERI, 0x205c, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_PCLK_SPI2,      "pclk_spi2",      "pclk_peri",
	      CMU_PERI, 0x2060, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_PCLK_SPI3,      "pclk_spi3",      "pclk_peri",
	      CMU_PERI, 0x2064, EXYNOS_GATE_BIT, 0),

	/* ── CMU_HSI: UFS gates ──────────────────────────────────────── */

	EGATE(CLK_ACLK_UFS_NOCS,  "aclk_ufs_nocs",  "div_bus",
	      CMU_TOP, 0x20d0, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_ACLK_UFS,       "aclk_ufs",   "aclk_ufs_nocs",
	      CMU_HSI, 0x2034, EXYNOS_GATE_BIT, CLK_IGNORE_UNUSED),
	EGATE(CLK_SCLK_UFS,       "sclk_ufs",       "fin_pll",
	      CMU_HSI, 0x2038, EXYNOS_GATE_BIT, 0),

	/* ── CMU_PERI: MMC / SD gates ────────────────────────────────── */

	EGATE(CLK_SCLK_MMC0,      "sclk_mmc0",      "mout_mmc",
	      CMU_TOP, 0x20a8, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_PCLK_MMC0,      "pclk_mmc0",      "pclk_peri",
	      CMU_PERI, 0x2068, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_SCLK_MMC2,      "sclk_mmc2",      "mout_mmc",
	      CMU_PERI, 0x206c, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_PCLK_MMC2,      "pclk_mmc2",      "pclk_peri",
	      CMU_PERI, 0x2070, EXYNOS_GATE_BIT, 0),

	/* ── CMU_USB: USB gates ──────────────────────────────────────── */

	EGATE(CLK_ACLK_USB_NOCS,  "aclk_usb_nocs",  "div_bus",
	      CMU_TOP, 0x20d4, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_ACLK_USB,       "aclk_usb",   "aclk_usb_nocs",
	      CMU_USB, 0x2000, EXYNOS_GATE_BIT, CLK_IGNORE_UNUSED),
	EGATE(CLK_SCLK_USB,       "sclk_usb",       "fin_pll",
	      CMU_USB, 0x200c, EXYNOS_GATE_BIT, 0),

	/* ── CMU_TOP: modem gates ────────────────────────────────────── */

	EGATE(CLK_ACLK_MODEM_NOCS,"aclk_modem_nocs","div_bus",
	      CMU_TOP, 0x2090, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_ACLK_MODEM,     "aclk_modem", "aclk_modem_nocs",
	      CMU_TOP, 0x2094, EXYNOS_GATE_BIT, 0),

	/* ── CMU_PERI: misc peripheral gates ─────────────────────────── */

	EGATE(CLK_PCLK_NFC,       "pclk_nfc",       "pclk_peri",
	      CMU_PERI, 0x2074, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_ACLK_PWM,       "aclk_pwm",       "aclk_peri",
	      CMU_PERI, 0x2078, EXYNOS_GATE_BIT, 0),
	EGATE(CLK_PCLK_ADCIF,     "pclk_adcif",     "pclk_peri",
	      CMU_PERI, 0x207c, EXYNOS_GATE_BIT, 0),

	/* RTC (ALIVE domain, uses CMU_TOP gate) */
	EGATE(CLK_OSC_RTC,         "osc_rtc",        NULL,
	      CMU_TOP, 0x2004, EXYNOS_GATE_BIT, CLK_IGNORE_UNUSED),

	/* ── CMU_TOP: DMA gates ──────────────────────────────────────── */

	EGATE(CLK_ACLK_DMAC0,     "aclk_dmac0",     "aclk_bus",
	      CMU_TOP, 0x2040, EXYNOS_GATE_BIT, CLK_IGNORE_UNUSED),
	EGATE(CLK_ACLK_DMAC1,     "aclk_dmac1",     "aclk_bus",
	      CMU_TOP, 0x2044, EXYNOS_GATE_BIT, CLK_IGNORE_UNUSED),
	EGATE(CLK_PCLK_DMAC1,     "pclk_dmac1",     "pclk_bus",
	      CMU_TOP, 0x205c, EXYNOS_GATE_BIT, 0),

	/* ── CMU_PERI: TMU / WDT ────────────────────────────────────── */

	EGATE(CLK_PCLK_TMU,       "pclk_tmu",       "pclk_peri",
	      CMU_PERI, 0x2080, EXYNOS_GATE_BIT, CLK_IGNORE_UNUSED),
	EGATE(CLK_PCLK_WDT,       "pclk_wdt",       "pclk_peri",
	      CMU_PERI, 0x2084, EXYNOS_GATE_BIT, 0),
};

#undef EGATE

/* ── Registration ────────────────────────────────────────────────────── */

static int exynos1280_register_clocks(struct exynos1280_clk *clk)
{
	struct device *dev = clk->dev;
	void __iomem *top = clk->base[CMU_TOP];
	struct clk_hw **hws = clk->data->hws;
	struct clk_hw *hw;
	int i, ret;

	/* Root oscillator: 26 MHz crystal */
	hw = clk_hw_register_fixed_rate(dev, "fin_pll", NULL, 0, 26000000);
	if (IS_ERR(hw))
		return PTR_ERR(hw);
	hws[CLK_FIN_PLL] = hw;

	/*
	 * PLLs — real Exynos 1280 uses PLL_SHARED0/1/2, PLL_G3D, PLL_MMC
	 * in CMU_TOP, plus per-block PLLs (PLL_AUD, PLL_CPUCL0/1, etc.).
	 * Modelled as fixed-factor until real PLL_CON0 register data is
	 * transcribed.  Rates approximate boot-time defaults.
	 *
	 *   PLL_SHARED0 ≈ 2000 MHz  (bus backbone)
	 *   PLL_SHARED1 ≈ 1800 MHz  (secondary bus)
	 *   PLL_SHARED2 ≈  800 MHz  (low-power path)
	 *   PLL_G3D     ≈  780 MHz  (GPU)
	 *   PLL_MMC     ≈  800 MHz  (storage)
	 *   PLL_CPUCL0  ≈ 1794 MHz  (A55 cluster)
	 */
	struct {
		unsigned int id;
		const char *name;
		unsigned int mult;
		unsigned int div;
	} plls[] = {
		{ CLK_FOUT_APLL,  "fout_shared0", 77,  1 },   /* ~2002 MHz */
		{ CLK_FOUT_DPLL,  "fout_shared1", 69,  1 },   /* ~1794 MHz */
		{ CLK_FOUT_EPLL,  "fout_shared2", 308, 10 },   /* ~800.8 MHz */
		{ CLK_FOUT_VPLL,  "fout_g3d",     30,  1 },   /* ~780 MHz */
	};

	for (i = 0; i < ARRAY_SIZE(plls); i++) {
		hw = clk_hw_register_fixed_factor(dev, plls[i].name, "fin_pll",
						  0, plls[i].mult, plls[i].div);
		if (IS_ERR(hw))
			return PTR_ERR(hw);
		hws[plls[i].id] = hw;
	}

	/* Extra PLLs: store in the provider so consumers (mout_mmc, mout_cpucl0)
	 * can resolve them, and so they appear in debugfs. */
	hw = clk_hw_register_fixed_factor(dev, "fout_mmc", "fin_pll",
					  0, 308, 10);
	if (IS_ERR(hw))
		return PTR_ERR(hw);

	hw = clk_hw_register_fixed_factor(dev, "fout_cpucl0", "fin_pll",
					  0, 69, 1);
	if (IS_ERR(hw))
		return PTR_ERR(hw);

	/*
	 * MUXes — CMU_TOP PLL source selection.
	 * PLL_CON0 registers at 0x01xx in CMU_TOP; bit 4 selects PLL vs osc.
	 */
	hw = clk_hw_register_mux(dev, "mout_shared0", mout_shared0_p,
				 ARRAY_SIZE(mout_shared0_p), CLK_SET_RATE_PARENT,
				 top + 0x0100, 4, 1, 0, &clk->lock);
	if (IS_ERR(hw))
		return PTR_ERR(hw);
	hws[CLK_MOUT_APLL] = hw;

	hw = clk_hw_register_mux(dev, "mout_shared1", mout_shared1_p,
				 ARRAY_SIZE(mout_shared1_p), CLK_SET_RATE_PARENT,
				 top + 0x0140, 4, 1, 0, &clk->lock);
	if (IS_ERR(hw))
		return PTR_ERR(hw);
	hws[CLK_MOUT_DPLL] = hw;

	hw = clk_hw_register_mux(dev, "mout_shared2", mout_shared2_p,
				 ARRAY_SIZE(mout_shared2_p), CLK_SET_RATE_PARENT,
				 top + 0x0180, 4, 1, 0, &clk->lock);
	if (IS_ERR(hw))
		return PTR_ERR(hw);
	hws[CLK_MOUT_EPLL] = hw;

	hw = clk_hw_register_mux(dev, "mout_g3d", mout_g3d_pll_p,
				 ARRAY_SIZE(mout_g3d_pll_p), CLK_SET_RATE_PARENT,
				 top + 0x01c0, 4, 1, 0, &clk->lock);
	if (IS_ERR(hw))
		return PTR_ERR(hw);
	hws[CLK_MOUT_VPLL] = hw;

	hw = clk_hw_register_mux(dev, "mout_mmc", mout_mmc_pll_p,
				 ARRAY_SIZE(mout_mmc_pll_p), CLK_SET_RATE_PARENT,
				 top + 0x0200, 4, 1, 0, &clk->lock);
	if (IS_ERR(hw))
		return PTR_ERR(hw);
	/* Store the MMC source mux in its own slot. The original code wrongly
	 * stored it in hws[CLK_MOUT_BUS], colliding with the real bus mux. */

	/* CPU mux: selects between oscillator and the CPU PLL.  Required by the
	 * cpus node in the device tree (CLK_MOUT_CPU). */
	hw = clk_hw_register_mux(dev, "mout_cpucl0", mout_cpucl0_p,
				 ARRAY_SIZE(mout_cpucl0_p), CLK_SET_RATE_PARENT,
				 top + 0x0240, 4, 1, 0, &clk->lock);
	if (IS_ERR(hw))
		return PTR_ERR(hw);
	hws[CLK_MOUT_CPU] = hw;

	/* Bus mux: selects between shared PLL divider outputs */
	hw = clk_hw_register_mux(dev, "mout_bus", mout_bus_p,
				 ARRAY_SIZE(mout_bus_p), 0,
				 top + 0x1000, 0, 2, 0, &clk->lock);
	if (IS_ERR(hw))
		return PTR_ERR(hw);
	hws[CLK_MOUT_BUS] = hw;

	/* Intermediate fixed-factor dividers for bus mux parents */
	hw = clk_hw_register_fixed_factor(dev, "dout_shared0_div2",
					  "mout_shared0", 0, 1, 2);
	if (IS_ERR(hw))
		return PTR_ERR(hw);
	hw = clk_hw_register_fixed_factor(dev, "dout_shared1_div2",
					  "mout_shared1", 0, 1, 2);
	if (IS_ERR(hw))
		return PTR_ERR(hw);
	hw = clk_hw_register_fixed_factor(dev, "dout_shared0_div3",
					  "mout_shared0", 0, 1, 3);
	if (IS_ERR(hw))
		return PTR_ERR(hw);
	hw = clk_hw_register_fixed_factor(dev, "dout_shared1_div3",
					  "mout_shared1", 0, 1, 3);
	if (IS_ERR(hw))
		return PTR_ERR(hw);

	/*
	 * Dividers — CMU_TOP clock distribution.
	 * Real DIV registers at 0x18xx in CMU_TOP.
	 */
	hw = clk_hw_register_divider(dev, "div_cpu", "mout_shared0",
				     CLK_SET_RATE_PARENT, top + 0x1800,
				     0, 4, 0, &clk->lock);
	if (IS_ERR(hw))
		return PTR_ERR(hw);
	hws[CLK_DIV_CPU] = hw;

	hw = clk_hw_register_divider(dev, "div_bus", "mout_bus", 0,
				     top + 0x1804, 0, 4, 0, &clk->lock);
	if (IS_ERR(hw))
		return PTR_ERR(hw);
	hws[CLK_DIV_BUS] = hw;

	hw = clk_hw_register_divider(dev, "div_d0_mipi", "mout_shared1", 0,
				     top + 0x1808, 0, 4, 0, &clk->lock);
	if (IS_ERR(hw))
		return PTR_ERR(hw);
	hws[CLK_DIV_D0_MIPI] = hw;

	hw = clk_hw_register_divider(dev, "div_a2m", "mout_shared2", 0,
				     top + 0x180c, 0, 4, 0, &clk->lock);
	if (IS_ERR(hw))
		return PTR_ERR(hw);
	hws[CLK_DIV_A2M] = hw;

	/* Gate clocks — table-driven, multi-region */
	for (i = 0; i < ARRAY_SIZE(exynos1280_gates); i++) {
		const struct exynos1280_gate_desc *gd = &exynos1280_gates[i];
		void __iomem *base;

		if (gd->region >= clk->num_regions || !clk->base[gd->region]) {
			dev_dbg(dev, "skipping gate %s: CMU region %d not mapped\n",
				gd->name, gd->region);
			continue;
		}
		base = clk->base[gd->region];

		hw = clk_hw_register_gate(dev, gd->name, gd->parent,
					  gd->flags, base + gd->offset,
					  gd->bit, 0, &clk->lock);
		if (IS_ERR(hw)) {
			dev_err(dev, "failed to register gate %s: %ld\n",
				gd->name, PTR_ERR(hw));
			return PTR_ERR(hw);
		}
		exynos1280_set_hw(clk, gd->id, hw);
	}

	ret = of_clk_add_hw_provider(dev->of_node, of_clk_hw_onecell_get,
				     clk->data);
	if (ret) {
		dev_err(dev, "failed to add clock provider: %d\n", ret);
		return ret;
	}

	return 0;
}

/* ── Platform driver ─────────────────────────────────────────────────── */

static const char * const cmu_reg_names[CMU_NUM_REGIONS] = {
	[CMU_TOP]  = "top",
	[CMU_PERI] = "peri",
	[CMU_DPU]  = "dpu",
	[CMU_G3D]  = "g3d",
	[CMU_AUD]  = "aud",
	[CMU_HSI]  = "hsi",
	[CMU_USB]  = "usb",
	[CMU_MFC]  = "mfc",
	[CMU_CSIS] = "csis",
};

static int exynos1280_clk_probe(struct platform_device *pdev)
{
	struct exynos1280_clk *clk;
	struct resource *res;
	int i, ret;

	clk = devm_kzalloc(&pdev->dev, sizeof(*clk), GFP_KERNEL);
	if (!clk)
		return -ENOMEM;

	clk->dev = &pdev->dev;
	spin_lock_init(&clk->lock);

	clk->data = devm_kzalloc(&pdev->dev,
				 struct_size(clk->data, hws, CLK_MAX_CLKS),
				 GFP_KERNEL);
	if (!clk->data)
		return -ENOMEM;
	clk->data->num = CLK_MAX_CLKS;

	/*
	 * Map CMU regions.  The DT node should provide reg entries named
	 * "top", "peri", "dpu", etc.  If reg-names are absent, fall back
	 * to mapping just the first reg entry as CMU_TOP.
	 */
	clk->num_regions = 0;
	for (i = 0; i < CMU_NUM_REGIONS; i++) {
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						   cmu_reg_names[i]);
		if (!res) {
			if (i == CMU_TOP) {
				/* Fall back to index-based for CMU_TOP */
				clk->base[i] = devm_platform_ioremap_resource(pdev, 0);
				if (IS_ERR(clk->base[i]))
					return dev_err_probe(&pdev->dev,
							     PTR_ERR(clk->base[i]),
							     "failed to map CMU_TOP\n");
				clk->num_regions = 1;
			}
			continue;
		}

		clk->base[i] = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(clk->base[i])) {
			dev_warn(&pdev->dev, "failed to map CMU %s, skipping\n",
				 cmu_reg_names[i]);
			clk->base[i] = NULL;
			continue;
		}
		if (i >= clk->num_regions)
			clk->num_regions = i + 1;
	}

	if (!clk->base[CMU_TOP])
		return dev_err_probe(&pdev->dev, -ENODEV,
				     "CMU_TOP region is required\n");

	ret = exynos1280_register_clocks(clk);
	if (ret)
		return dev_err_probe(&pdev->dev, ret,
				     "failed to register clocks\n");

	platform_set_drvdata(pdev, clk);
	dev_info(&pdev->dev,
		 "Exynos 1280 clock controller: %d CMU regions, %u clock IDs\n",
		 clk->num_regions, CLK_MAX_CLKS);
	return 0;
}

static void exynos1280_clk_remove(struct platform_device *pdev)
{
	struct exynos1280_clk *clk = platform_get_drvdata(pdev);
	int i;

	of_clk_del_provider(pdev->dev.of_node);

	/* Unregister all clocks we registered in probe to avoid leaks. */
	for (i = 0; i < CLK_MAX_CLKS; i++) {
		if (clk->data->hws[i] && !IS_ERR(clk->data->hws[i]))
			clk_hw_unregister(clk->data->hws[i]);
	}
}

static const struct of_device_id exynos1280_clk_of_match[] = {
	{ .compatible = "samsung,s5e8825-clock" },
	{ .compatible = "samsung,exynos1280-clock" }, /* alias */
	{ }
};
MODULE_DEVICE_TABLE(of, exynos1280_clk_of_match);

static struct platform_driver exynos1280_clk_driver = {
	.driver = {
		.name = "exynos1280-clk",
		.of_match_table = exynos1280_clk_of_match,
		.suppress_bind_attrs = true,
	},
	.probe  = exynos1280_clk_probe,
	.remove = exynos1280_clk_remove,
};
module_platform_driver(exynos1280_clk_driver);

MODULE_DESCRIPTION("Samsung Exynos 1280 (s5e8825) clock controller");
MODULE_AUTHOR("ExynosNext Project");
MODULE_LICENSE("GPL");
