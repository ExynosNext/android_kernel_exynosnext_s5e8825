/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos ACPM IPC Control Interface
 *
 * Public API for kernel subsystems (clock, DVFS, thermal, PMIC) to communicate
 * with the ACPM (Audio Co-Processor Manager) via IPC channels.
 *
 * Transcribed from Samsung's 5.10 kernel (drivers/soc/samsung/acpm/),
 * adapted for Linux 6.18 and the GKI module framework.
 *
 * Copyright (C) 2026 ExynosNext Project
 * Copyright (C) Samsung Electronics Co., Ltd.
 */

#ifndef __ACPM_IPC_CTRL_H
#define __ACPM_IPC_CTRL_H

#include <linux/types.h>

/**
 * ipc_callback - callback invoked when an IPC response arrives on a channel
 * @cmd: pointer to the response data (array of u32 words)
 * @size: number of u32 words in the response
 */
typedef void (*ipc_callback)(unsigned int *cmd, unsigned int size);

/**
 * struct ipc_config - configuration for an IPC message
 * @cmd:         message payload (array of u32 words)
 * @indirection_base: base address for indirection data (NULL if not used)
 * @indirection_size: size of indirection data
 * @response:    true if this message expects a response
 * @indirection: true if this message uses indirection
 */
struct ipc_config {
	unsigned int *cmd;
	unsigned int *indirection_base;
	unsigned int indirection_size;
	bool response;
	bool indirection;
};

/* IPC protocol field positions in the first word of each message */
#define ACPM_IPC_PROTOCOL_OWN			(31)
#define ACPM_IPC_PROTOCOL_RSP			(30)
#define ACPM_IPC_PROTOCOL_INDIRECTION		(29)
#define ACPM_IPC_PROTOCOL_STOP_WDT		(27)
#define ACPM_IPC_PROTOCOL_ID			(26)
#define ACPM_IPC_PROTOCOL_IDX			(0x7 << ACPM_IPC_PROTOCOL_ID)
#define ACPM_IPC_PROTOCOL_DP_A			(25)
#define ACPM_IPC_PROTOCOL_DP_D			(24)
#define ACPM_IPC_PROTOCOL_DP_CMD		(0x3 << ACPM_IPC_PROTOCOL_DP_D)
#define ACPM_IPC_PROTOCOL_TEST			(23)
#define ACPM_IPC_PROTOCOL_STOP			(22)
#define ACPM_IPC_PROTOCOL_SEQ_NUM		(16)

#if defined(CONFIG_EXYNOS_ACPM) || defined(CONFIG_EXYNOS_ACPM_MODULE)

unsigned int acpm_ipc_request_channel(struct device_node *np, ipc_callback handler,
				      unsigned int *id, unsigned int *size);
unsigned int acpm_ipc_release_channel(struct device_node *np, unsigned int channel_id);
int acpm_ipc_send_data(unsigned int channel_id, struct ipc_config *cfg);
int acpm_ipc_send_data_sync(unsigned int channel_id, struct ipc_config *cfg);
int acpm_ipc_set_ch_mode(struct device_node *np, bool polling);
bool is_acpm_ipc_busy(unsigned int ch_id);
u32 acpm_get_peri_timer(void);
void exynos_acpm_set_fast_switch(unsigned int ch);

#else /* !CONFIG_EXYNOS_ACPM */

static inline unsigned int acpm_ipc_request_channel(struct device_node *np,
		ipc_callback handler, unsigned int *id, unsigned int *size)
{ return 0; }

static inline unsigned int acpm_ipc_release_channel(struct device_node *np,
		unsigned int channel_id)
{ return 0; }

static inline int acpm_ipc_send_data(unsigned int channel_id, struct ipc_config *cfg)
{ return 0; }

static inline int acpm_ipc_send_data_sync(unsigned int channel_id, struct ipc_config *cfg)
{ return 0; }

static inline int acpm_ipc_set_ch_mode(struct device_node *np, bool polling)
{ return 0; }

static inline bool is_acpm_ipc_busy(unsigned int ch_id)
{ return false; }

static inline u32 acpm_get_peri_timer(void)
{ return 0; }

static inline void exynos_acpm_set_fast_switch(unsigned int ch) {}

#endif /* CONFIG_EXYNOS_ACPM */

#endif /* __ACPM_IPC_CTRL_H */
