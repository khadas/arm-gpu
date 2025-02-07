/*
 * mali_scaling.h
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

/**
 * @file arm_core_scaling.h
 * Example core scaling policy.
 */

#ifndef __ARM_CORE_SCALING_H__
#define __ARM_CORE_SCALING_H__

#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/clk-provider.h>
#include <linux/clk.h>

enum mali_scale_mode_t {
	MALI_PP_SCALING = 0,
	MALI_PP_FS_SCALING,
	MALI_SCALING_DISABLE,
	MALI_TURBO_MODE,
	MALI_SCALING_MODE_MAX
};

enum mali_reset_item_t {
	MALI_RESET_MODULE = 0,
	MALI_RESET_APB_BUS,
	MALI_RESET_MAX
};

typedef struct mali_dvfs_threshold_table {
	uint32_t    freq_index;
	uint32_t    voltage;
	uint32_t    keep_count;
	uint32_t    downthreshold;
	uint32_t    upthreshold;
	uint32_t    clk_freq;
	const char  *clk_parent;
	struct clk  *clkp_handle;
	uint32_t    clkp_freq;
} mali_dvfs_threshold_table;

/**
 *  restrictions on frequency and number of pp.
 */
typedef struct mali_scale_info_t {
	u32 minpp;
	u32 maxpp;
	u32 minclk;
	u32 maxclk;
} mali_scale_info_t;

typedef struct mali_reset_info_t {
	u32 reg_level;
	u32 reg_mask;
	u32 reg_bit;
} mali_reset_info_t;

/**
 * Platform specific data for meson chips.
 */
typedef struct mali_plat_info_t {
	u32 cfg_pp; /* number of pp. */
	u32 cfg_min_pp;
	u32 turbo_clock; /* reserved clock src. */
	u32 def_clock; /* gpu clock used most of time.*/
	u32 cfg_clock; /* max clock could be used.*/
	u32 cfg_clock_bkup; /* same as cfg_clock, for backup. */
	u32 cfg_min_clock;

	u32  sc_mpp; /* number of pp used most of time.*/
	u32  bst_gpu; /* threshold for boosting gpu. */
	u32  bst_pp; /* threshold for boosting PP. */

	u32 *clk;
	u32 *clk_sample;
	u32 clk_len;
	u32 have_switch; /* have clock gate switch or not. */

	mali_dvfs_threshold_table *dvfs_table;
	u32 dvfs_table_size;

	mali_scale_info_t scale_info;
	u32 maxclk_sysfs;
	u32 maxpp_sysfs;

	/* mali external reset reg info */
	u32 reset_flag;
	mali_reset_info_t module_reset;
	mali_reset_info_t apb_reset;

	/* set upper limit of pp or frequency, for THERMAL thermal or band width saving.*/
	u32 limit_on;

	/* for boost up gpu by user. */
	void (*plat_preheat)(void);
#ifdef CONFIG_MALI_DEVFREQ
	enum preheat_status{
		PREHEAT_NULL,
		PREHEAT_START,
		PREHEAT_DOING,
		PREHEAT_END
	}status;
#endif

	struct platform_device *pdev;
	struct work_struct wq_work;
	struct clk *clk_mali;
	struct clk *clk_mali_0;
	struct clk *clk_mali_1;
	struct clk *clk_stack;//G310 in S7D have two clock
	void __iomem *reg_base_reset;
	void __iomem *reg_base_hiu;
	u32 clk_cntl_reg;
} mali_plat_info_t;
mali_plat_info_t* get_mali_plat_data(void);

/**
 * Initialize core scaling policy.
 *
 * @note The core scaling policy will assume that all PP cores are on initially.
 *
 * @param num_pp_cores Total number of PP cores.
 */
int mali_core_scaling_init(mali_plat_info_t*);

/**
 * Terminate core scaling policy.
 */
void mali_core_scaling_term(void);

/**
 * cancel and flush scaling job queue.
 */
void flush_scaling_job(void);

/* get current state(pp, clk). */
void get_mali_rt_clkpp(u32* clk, u32* pp);
u32 set_mali_rt_clkpp(u32 clk, u32 pp, u32 flush);
void revise_mali_rt(void);
/* get max gpu clk level of this chip*/
int get_gpu_max_clk_level(void);

/* get or set the scale mode. */
u32 get_mali_schel_mode(void);
void set_mali_schel_mode(u32 mode);

/* for frequency reporter in DS-5 streamline. */
u32 get_current_frequency(void);

#endif /* __ARM_CORE_SCALING_H__ */
