/*
 * Copyright (C) 2020-2025 Loongson Technology Corporation Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-03-10     LoongsonLab  the first version
 * 2025-03-13     LoongsonLab  fix bugs
 */

#include <rtthread.h>

#include "loongarch.h"
#include "rthw.h"

#define BIT(x) (1UL << x)

/* FIXME: Is this even legal? */
extern struct rt_irq_desc irq_desc[];

static int ls2k0300_extended_iointerrupt_enable(void) {
	uint64_t iocsr = iocsr_read64(0x100);
	iocsr |= BIT(19);
	iocsr_write64(iocsr, 0x100);
}

static void writeb(char value, volatile void *addr)
{
	*(volatile char *)addr = value;
}

static void writel(unsigned int value, volatile void *addr)
{
	*(volatile unsigned int *)addr = value;
}

static void handle_ext_interrupt(uint32_t iocsr_idx, uint32_t base_int_index) {
	uint32_t status = iocsr_read32(iocsr_idx);

	/* Exit if no interrupt on this line */
	if (status == 0) {
		return;
	}

	for (uint32_t i = 0; i < 32; ++i) {
		if (status & BIT(i)) {
			uint32_t irq_idx = i + base_int_index;
			void *param = irq_desc[irq_idx].param;

			/* Call ISR */
			irq_desc[irq_idx].handler(irq_idx, param);
		}
	}

	/* Clear IOCSR */
	iocsr_write32(status, iocsr_idx);
}

// void rt_hw_interrupt_umask(int vector) {
// 	// RT_ASSERT(vector > 0 && vector < 128);

// 	uint32_t dest_iocsr = 0x1600 + 4 * (vector / 32);
// 	uint32_t iocsr_val = iocsr_read32(dest_iocsr);
// 	iocsr_val |= BIT(vector % 32);
// 	iocsr_write32(dest_iocsr, iocsr_val);
// }

void platform_irq_init() {

	int i, bit, cores, index, node;
	rt_uint32_t data;

	/* Enable Extended IO Interrupt */
	ls2k0300_extended_iointerrupt_enable();

	/* Mask all interrupts */
	iocsr_write32(1, 0x1600); /* HACK: Allow Interrupt 0 (UART0) */
	iocsr_write32(0, 0x1604);
	iocsr_write32(0, 0x1608);
	iocsr_write32(0, 0x160c);
	
	/* Route each 32-entry interrupt group to a separate hardware interrupt */
	uint32_t extioi_map = 0;
	extioi_map |= (BIT(0)      ); /* [31:0] -> HWI[0] */
	extioi_map |= (BIT(1) <<  8); /* [63:32] -> HWI[1] */
	extioi_map |= (BIT(2) << 16); /* [95:64] -> HWI[2] */
	extioi_map |= (BIT(3) << 24); /* [127:96] -> HWI[3] */
	iocsr_write32(extioi_map, 0x14c0);
}

void platform_generic_irq(void) {
	/* Check for each interrupt source. */
	handle_ext_interrupt(0x1800, 0);
	handle_ext_interrupt(0x1804, 32);
	handle_ext_interrupt(0x1808, 64);
	handle_ext_interrupt(0x180c, 96);
}
