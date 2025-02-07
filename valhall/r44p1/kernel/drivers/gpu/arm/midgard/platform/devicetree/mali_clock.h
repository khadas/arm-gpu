/*
 * mali_clock.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __MALI_CLOCK_H__
#define __MALI_CLOCK_H__
#include <linux/version.h>
#include <linux/pm.h>
#include <linux/io.h>
#include <linux/types.h>
#include <linux/of.h>

#include <asm/io.h>
#include <linux/clk.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 29)) && (LINUX_VERSION_CODE <= KERNEL_VERSION(4, 10, 0))
#include <linux/amlogic/iomap.h>
#endif

#ifndef HHI_MALI_CLK_CNTL
#define HHI_MALI_CLK_CNTL   0x6C
#endif

//extern int mali_clock_init(struct platform_device *dev);
int mali_clock_init_clk_tree(struct platform_device *pdev);

typedef int (*critical_t)(size_t param);
int mali_clock_critical(critical_t critical, size_t param);

int mali_clock_init(mali_plat_info_t*);
int mali_clock_set(unsigned int index);
u32 get_mali_freq(u32 idx);
int mali_dt_info(struct platform_device *pdev,
        struct mali_plat_info_t *mpdata);
#endif
