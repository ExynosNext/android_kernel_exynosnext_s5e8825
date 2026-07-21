/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos ACPM IPC — internal structures
 *
 * Copyright (C) 2026 ExynosNext Project
 * Copyright (C) Samsung Electronics Co., Ltd.
 */

#ifndef __ACPM_IPC_H_
#define __ACPM_IPC_H_

#include <linux/completion.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <soc/samsung/acpm_ipc_ctrl.h>

/* Maximum number of IPC channels */
#define ACPM_IPC_MAX_CHANNELS		16

/* Maximum sequence number before wrap */
#define SEQUENCE_NUM_MAX			64

/* IPC timeout in nanoseconds (15 seconds) */
#define IPC_TIMEOUT				(15000000000ULL)

/**
 * struct buff_info - circular buffer descriptor for a channel direction
 * @rear:   pointer to the rear index in SRAM
 * @front:  pointer to the front index in SRAM
 * @base:   pointer to the buffer data area in SRAM
 * @direction: pointer to the direction field in SRAM
 * @size:   total buffer size in bytes
 * @len:    per-message length in bytes
 * @d_buff_size: double-buffer size
 */
struct buff_info {
	void __iomem *rear;
	void __iomem *front;
	void __iomem *base;
	void __iomem *direction;
	unsigned int size;
	unsigned int len;
	unsigned int d_buff_size;
};

/**
 * struct acpm_ipc_ch - per-channel state
 * @rx_ch:   receive buffer info
 * @tx_ch:   transmit buffer info
 * @list:    callback list for this channel
 * @id:      channel ID
 * @type:    channel type
 * @seq_num: current sequence number
 * @seq_num_flag: sequence number acknowledgment flags
 * @cmd:     scratch buffer for the current command
 * @rx_lock: spinlock for receive path
 * @tx_lock: spinlock for transmit path
 * @ch_lock: spinlock for channel state
 * @wait_lock: mutex for synchronous wait
 * @wait:    completion for synchronous IPC
 * @polling: true if channel uses polling mode
 * @interrupt: true if channel uses interrupt mode
 */
struct acpm_ipc_ch {
	struct buff_info rx_ch;
	struct buff_info tx_ch;
	struct list_head list;
	unsigned int id;
	unsigned int type;
	unsigned int seq_num;
	u32 seq_num_flag[SEQUENCE_NUM_MAX];
	unsigned int *cmd;
	spinlock_t rx_lock;
	spinlock_t tx_lock;
	spinlock_t ch_lock;
	struct mutex wait_lock;
	struct completion wait;
	bool polling;
	bool interrupt;
};

/**
 * struct acpm_ipc_info - global ACPM IPC state
 * @num_channels: number of active channels
 * @dev:   platform device
 * @channel: array of channel descriptors
 * @irq:   ACPM interrupt line
 * @intr:  mapped interrupt register base (SFR region)
 * @sram_base: mapped SRAM base
 * @initdata: pointer to initdata in SRAM
 * @initdata_base: offset of initdata in SRAM
 * @intr_status: cached interrupt status
 */
struct acpm_ipc_info {
	unsigned int num_channels;
	struct device *dev;
	struct acpm_ipc_ch *channel;
	unsigned int irq;
	void __iomem *intr;
	void __iomem *sram_base;
	void *initdata;
	unsigned int initdata_base;
	unsigned int intr_status;
};

/* ACPM interrupt register offsets (relative to the SFR base @ 0x11900000) */
#define INTGR0					0x0008
#define INTCR0					0x000C
#define INTMR0					0x0010
#define INTSR0					0x0014
#define INTMSR0					0x0018
#define INTGR1					0x001C
#define INTCR1					0x0020
#define INTMR1					0x0024
#define INTSR1					0x0028
#define INTMSR1					0x002C

/* Shared registers */
#define SR0					0x0080
#define SR1					0x0084
#define SR2					0x0088
#define SR3					0x008C

extern int acpm_ipc_probe(struct platform_device *pdev);
extern int acpm_ipc_remove(struct platform_device *pdev);

#endif /* __ACPM_IPC_H_ */
