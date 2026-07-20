/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause) */
/*
 * Samsung Exynos 1280 (s5e8825) — clock ID definitions
 *
 * Single source of truth for clock IDs used by both the DT and the
 * clock controller driver.
 */

#ifndef _DT_BINDINGS_CLK_SAMSUNG_EXYNOS1280_H
#define _DT_BINDINGS_CLK_SAMSUNG_EXYNOS1280_H

/* PLL clocks */
#define CLK_FIN_PLL		0
#define CLK_FOUT_APLL		1
#define CLK_FOUT_DPLL		2
#define CLK_FOUT_EPLL		3
#define CLK_FOUT_VPLL		4

/* MUX clocks */
#define CLK_MOUT_APLL		10
#define CLK_MOUT_DPLL		11
#define CLK_MOUT_EPLL		12
#define CLK_MOUT_VPLL		13
#define CLK_MOUT_CPU		14
#define CLK_MOUT_BUS		15

/* DIV clocks */
#define CLK_DIV_CPU		30
#define CLK_DIV_BUS		31
#define CLK_DIV_D0_MIPI		32
#define CLK_DIV_A2M		33

/* CPU clocks */
#define CLK_CPU_ACLK		51
#define CLK_CPU_PCLK		52

/* Bus / peri clocks */
#define CLK_ACLK_BUS		60
#define CLK_PCLK_BUS		61
#define CLK_ACLK_PERI		62
#define CLK_PCLK_PERI		63

/* Display clocks */
#define CLK_ACLK_DISP		70
#define CLK_PCLK_DISP		71
#define CLK_ACLK_DISP_NOCS	72
#define CLK_SCLK_DISP		73
#define CLK_ACLK_DECON0		74
#define CLK_SCLK_DECON0		75
#define CLK_ACLK_DSIM0		76
#define CLK_SCLK_MIPI_TX	77
#define CLK_ACLK_DMA_A		78

/* Camera clocks */
#define CLK_ACLK_CAM		80
#define CLK_ACLK_CAM_NOCS	81
#define CLK_PCLK_CAM		82
#define CLK_ACLK_CSIS0		83
#define CLK_PCLK_CSIS0		84

/* MFC clocks */
#define CLK_ACLK_MFC		90
#define CLK_ACLK_MFC_NOCS	91
#define CLK_PCLK_MFC		92

/* GPU clocks */
#define CLK_ACLK_GPU		100
#define CLK_ACLK_GPU_NOCS	101
#define CLK_SCLK_GPU		102
#define CLK_SCLK_GPU_NOCS	103
#define CLK_CLK_GPU		104

/* Audio clocks */
#define CLK_ACLK_AUD		110
#define CLK_ACLK_AUD_NOCS	111
#define CLK_PCLK_AUD		112
#define CLK_SCLK_I2S0		113
#define CLK_PCLK_I2S0		114
#define CLK_SCLK_TDM0		115
#define CLK_PCLK_TDM0		116
#define CLK_SCLK_SPDIF		117
#define CLK_PCLK_SPDIF		118
#define CLK_PCLK_PCM0		119
#define CLK_SCLK_SLIF		120
#define CLK_PCLK_SLIF		121
#define CLK_SCLK_VTS		122
#define CLK_PCLK_VTS		123

/* UART clocks */
#define CLK_SCLK_UART0		130
#define CLK_PCLK_UART0		131
#define CLK_SCLK_UART1		132
#define CLK_PCLK_UART1		133
#define CLK_SCLK_UART2		134
#define CLK_PCLK_UART2		135
#define CLK_SCLK_UART3		136
#define CLK_PCLK_UART3		137
#define CLK_SCLK_UART4		138
#define CLK_PCLK_UART4		139

/* I2C clocks */
#define CLK_PCLK_I2C0		140
#define CLK_PCLK_I2C1		141
#define CLK_PCLK_I2C2		142
#define CLK_PCLK_I2C3		143
#define CLK_PCLK_I2C4		144
#define CLK_PCLK_I2C5		145
#define CLK_PCLK_I2C6		146
#define CLK_PCLK_I2C7		147

/* SPI clocks */
#define CLK_PCLK_SPI0		150
#define CLK_PCLK_SPI1		151
#define CLK_PCLK_SPI2		152
#define CLK_PCLK_SPI3		153

/* UFS clocks */
#define CLK_ACLK_UFS		160
#define CLK_ACLK_UFS_NOCS	161
#define CLK_SCLK_UFS		162

/* MMC clocks */
#define CLK_SCLK_MMC0		170
#define CLK_PCLK_MMC0		171
#define CLK_SCLK_MMC2		172
#define CLK_PCLK_MMC2		173

/* USB clocks */
#define CLK_ACLK_USB		180
#define CLK_ACLK_USB_NOCS	181
#define CLK_SCLK_USB		182

/* Modem clocks */
#define CLK_ACLK_MODEM		190
#define CLK_ACLK_MODEM_NOCS	191

/* Misc peripherals */
#define CLK_PCLK_NFC		200
#define CLK_ACLK_PWM		210
#define CLK_PCLK_ADCIF		220
#define CLK_OSC_RTC		230

/* DMA clocks */
#define CLK_ACLK_DMAC0		240
#define CLK_ACLK_DMAC1		241
#define CLK_PCLK_DMAC1		242

/* TMU / WDT */
#define CLK_PCLK_TMU		250
#define CLK_PCLK_WDT		251

#endif /* _DT_BINDINGS_CLK_SAMSUNG_EXYNOS1280_H */
