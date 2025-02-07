/*
 * ../vendor/amlogic/common/gpu/utgard/platform/meson_bu/mali_clock.c
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

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/of_address.h>
#include <linux/delay.h>
#include <linux/version.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
#include <linux/time.h>
#endif
#include <linux/mali/mali_utgard.h>
#include <linux/amlogic/cpu_version.h>
#include "mali_scaling.h"
#include "mali_clock.h"

#ifndef AML_CLK_LOCK_ERROR
#define AML_CLK_LOCK_ERROR 1
#endif
#define GXBBM_MAX_GPU_FREQ 700000000UL
struct clk;
static unsigned gpu_dbg_level = 0;
module_param(gpu_dbg_level, uint, 0644);
MODULE_PARM_DESC(gpu_dbg_level, "gpu debug level");

#define gpu_dbg(level, fmt, arg...)			\
	do {			\
		if (gpu_dbg_level >= (level))		\
		printk("gpu_debug"fmt , ## arg); 	\
	} while (0)

#define GPU_CLK_DBG(fmt, arg...)

//disable print
#define _dev_info(...)

//static DEFINE_SPINLOCK(lock);
static mali_plat_info_t* pmali_plat = NULL;
//static u32 mali_extr_backup = 0;
//static u32 mali_extr_sample_backup = 0;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
struct timespec64 start;
struct timespec64 end;
#else
struct timeval start;
struct timeval end;
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 16))
int mali_clock_init_clk_tree(struct platform_device* pdev)
{
	mali_dvfs_threshold_table *dvfs_tbl = &pmali_plat->dvfs_table[pmali_plat->def_clock];
	struct clk *clk_mali_0_parent = dvfs_tbl->clkp_handle;
	struct clk *clk_mali_0 = pmali_plat->clk_mali_0;
#ifdef AML_CLK_LOCK_ERROR
	struct clk *clk_mali_1 = pmali_plat->clk_mali_1;
#endif
	struct clk *clk_mali = pmali_plat->clk_mali;

	clk_set_parent(clk_mali_0, clk_mali_0_parent);

	clk_prepare_enable(clk_mali_0);

	clk_set_parent(clk_mali, clk_mali_0);

#ifdef AML_CLK_LOCK_ERROR
	clk_set_parent(clk_mali_1, clk_mali_0_parent);
	clk_prepare_enable(clk_mali_1);
#endif

	GPU_CLK_DBG("%s:enable(%d), %s:enable(%d)\n",
	  clk_mali_0->name,  clk_mali_0->enable_count,
	  clk_mali_0_parent->name, clk_mali_0_parent->enable_count);

	return 0;
}

int mali_clock_init(mali_plat_info_t *pdev)
{
	*pdev = *pdev;
	return 0;
}

int mali_clock_critical(critical_t critical, size_t param)
{
	int ret = 0;

	ret = critical(param);

	return ret;
}

static int critical_clock_set(size_t param)
{
	int ret = 0;
	unsigned int idx = param;
	mali_dvfs_threshold_table *dvfs_tbl = &pmali_plat->dvfs_table[idx];

	struct clk *clk_mali_0 = pmali_plat->clk_mali_0;
	struct clk *clk_mali_1 = pmali_plat->clk_mali_1;
	struct clk *clk_mali_x   = NULL;
	struct clk *clk_mali_x_parent = NULL;
	struct clk *clk_mali_x_old = NULL;
	struct clk *clk_mali   = pmali_plat->clk_mali;
	unsigned long time_use=0;

	clk_mali_x_old  = clk_get_parent(clk_mali);

	if (!clk_mali_x_old) {
		printk("gpu: could not get clk_mali_x_old or clk_mali_x_old\n");
		return 0;
	}
	if (clk_mali_x_old == clk_mali_0) {
		clk_mali_x = clk_mali_1;
	} else if (clk_mali_x_old == clk_mali_1) {
		clk_mali_x = clk_mali_0;
	} else {
		printk("gpu: unmatched clk_mali_x_old\n");
		return 0;
	}

	GPU_CLK_DBG("idx=%d, clk_freq=%d\n", idx, dvfs_tbl->clk_freq);
	clk_mali_x_parent = dvfs_tbl->clkp_handle;
	if (!clk_mali_x_parent) {
		printk("gpu: could not get clk_mali_x_parent\n");
		return 0;
	}

	GPU_CLK_DBG();
	ret = clk_set_rate(clk_mali_x_parent, dvfs_tbl->clkp_freq);
	GPU_CLK_DBG();
	ret = clk_set_parent(clk_mali_x, clk_mali_x_parent);
	GPU_CLK_DBG();
	ret = clk_set_rate(clk_mali_x, dvfs_tbl->clk_freq);
	GPU_CLK_DBG();
#ifndef AML_CLK_LOCK_ERROR
	ret = clk_prepare_enable(clk_mali_x);
#endif
	GPU_CLK_DBG("new %s:enable(%d)\n", clk_mali_x->name,  clk_mali_x->enable_count);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	ktime_get_real_ts64(&start);
#else
	do_gettimeofday(&start);
#endif
	udelay(1);// delay 10ns
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	ktime_get_real_ts64(&end);
#else
	do_gettimeofday(&end);
#endif
	ret = clk_set_parent(clk_mali, clk_mali_x);
	GPU_CLK_DBG();

#ifndef AML_CLK_LOCK_ERROR
	clk_disable_unprepare(clk_mali_x_old);
#endif
	GPU_CLK_DBG("old %s:enable(%d)\n", clk_mali_x_old->name,  clk_mali_x_old->enable_count);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	time_use = (end.tv_sec - start.tv_sec)*1000000 + div64_u64(end.tv_nsec - start.tv_nsec, 1000);
#else
	time_use = (end.tv_sec - start.tv_sec)*1000000 + end.tv_usec - start.tv_usec;
#endif
	GPU_CLK_DBG("step 1, mali_mux use: %ld us\n", time_use);

	return 0;
}

int mali_clock_set(unsigned int clock)
{
	return mali_clock_critical(critical_clock_set, (size_t)clock);
}

void disable_clock(void)
{
	struct clk *clk_mali = pmali_plat->clk_mali;
	struct clk *clk_mali_x = NULL;

	clk_mali_x = clk_get_parent(clk_mali);
	GPU_CLK_DBG();
#ifndef AML_CLK_LOCK_ERROR
	clk_disable_unprepare(clk_mali_x);
#endif
	GPU_CLK_DBG();
}

void enable_clock(void)
{
	struct clk *clk_mali = pmali_plat->clk_mali;
	struct clk *clk_mali_x = NULL;

	clk_mali_x = clk_get_parent(clk_mali);
	GPU_CLK_DBG();
#ifndef AML_CLK_LOCK_ERROR
	clk_prepare_enable(clk_mali_x);
#endif
	GPU_CLK_DBG();
}

u32 get_mali_freq(u32 idx)
{
	if (!mali_pm_statue) {
		return pmali_plat->clk_sample[idx];
	} else {
		return 0;
	}
}

void set_str_src(u32 data)
{
	printk("gpu: %s, %s, %d\n", __FILE__, __func__, __LINE__);
}

int mali_dt_info(struct platform_device *pdev, struct mali_plat_info_t *mpdata)
{
	struct device_node *gpu_dn = pdev->dev.of_node;
	struct device_node *gpu_clk_dn;
	struct mali_gpu_clk_item *clk_item;
	phandle dvfs_clk_hdl;
	mali_dvfs_threshold_table *dvfs_tbl = NULL;
	uint32_t *clk_sample = NULL;

	struct property *prop;
	const __be32 *p;
	int length = 0, i = 0;
	u32 u;

	int ret = 0;
	if (!gpu_dn) {
		dev_notice(&pdev->dev, "gpu device node not right\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(gpu_dn,"num_of_pp",
			&mpdata->cfg_pp);
	if (ret) {
		dev_notice(&pdev->dev, "set max pp to default 6\n");
		mpdata->cfg_pp = 6;
	}
	mpdata->scale_info.maxpp = mpdata->cfg_pp;
	mpdata->maxpp_sysfs = mpdata->cfg_pp;
	_dev_info(&pdev->dev, "max pp is %d\n", mpdata->scale_info.maxpp);

	ret = of_property_read_u32(gpu_dn,"min_pp",
			&mpdata->cfg_min_pp);
	if (ret) {
		dev_notice(&pdev->dev, "set min pp to default 1\n");
		mpdata->cfg_min_pp = 1;
	}
	mpdata->scale_info.minpp = mpdata->cfg_min_pp;
	_dev_info(&pdev->dev, "min pp is %d\n", mpdata->scale_info.minpp);

	ret = of_property_read_u32(gpu_dn,"min_clk",
			&mpdata->cfg_min_clock);
	if (ret) {
		dev_notice(&pdev->dev, "set min clk default to 0\n");
		mpdata->cfg_min_clock = 0;
	}
	mpdata->scale_info.minclk = mpdata->cfg_min_clock;
	_dev_info(&pdev->dev, "min clk  is %d\n", mpdata->scale_info.minclk);

	mpdata->reg_base_hiubus = of_iomap(gpu_dn, 3);
	_dev_info(&pdev->dev, "hiu io source  0x%p\n", mpdata->reg_base_hiubus);

	mpdata->reg_base_aobus = of_iomap(gpu_dn, 2);
	_dev_info(&pdev->dev, "hiu io source  0x%p\n", mpdata->reg_base_aobus);

	ret = of_property_read_u32(gpu_dn,"sc_mpp",
			&mpdata->sc_mpp);
	if (ret) {
		dev_notice(&pdev->dev, "set pp used most of time default to %d\n", mpdata->cfg_pp);
		mpdata->sc_mpp = mpdata->cfg_pp;
	}
	_dev_info(&pdev->dev, "num of pp used most of time %d\n", mpdata->sc_mpp);

	of_get_property(gpu_dn, "tbl", &length);

	length = length /sizeof(u32);
	_dev_info(&pdev->dev, "clock dvfs cfg table size is %d\n", length);

	mpdata->dvfs_table = devm_kzalloc(&pdev->dev,
								  sizeof(struct mali_dvfs_threshold_table)*length,
								  GFP_KERNEL);
	dvfs_tbl = mpdata->dvfs_table;
	if (mpdata->dvfs_table == NULL) {
		dev_err(&pdev->dev, "failed to alloc dvfs table\n");
		return -ENOMEM;
	}
	mpdata->clk_sample = devm_kzalloc(&pdev->dev, sizeof(u32)*length, GFP_KERNEL);
	if (mpdata->clk_sample == NULL) {
		dev_err(&pdev->dev, "failed to alloc clk_sample table\n");
		return -ENOMEM;
	}
	clk_sample = mpdata->clk_sample;
///////////
	mpdata->clk_items = devm_kzalloc(&pdev->dev, sizeof(struct mali_gpu_clk_item) * length, GFP_KERNEL);
	if (mpdata->clk_items == NULL) {
		dev_err(&pdev->dev, "failed to alloc clk_item table\n");
		return -ENOMEM;
	}
	clk_item = mpdata->clk_items;
//
	of_property_for_each_u32(gpu_dn, "tbl", prop, p, u) {
		dvfs_clk_hdl = (phandle) u;
		gpu_clk_dn = of_find_node_by_phandle(dvfs_clk_hdl);
		ret = of_property_read_u32(gpu_clk_dn,"clk_freq", &dvfs_tbl->clk_freq);
		if (ret) {
			dev_notice(&pdev->dev, "read clk_freq failed\n");
		}

		ret = of_property_read_string(gpu_clk_dn,"clk_parent",
										&dvfs_tbl->clk_parent);
		if (ret) {
			dev_notice(&pdev->dev, "read clk_parent failed\n");
		}
		dvfs_tbl->clkp_handle = devm_clk_get(&pdev->dev, dvfs_tbl->clk_parent);
		if (IS_ERR(dvfs_tbl->clkp_handle)) {
			dev_notice(&pdev->dev, "failed to get %s's clock pointer\n", dvfs_tbl->clk_parent);
		}
		ret = of_property_read_u32(gpu_clk_dn,"clkp_freq", &dvfs_tbl->clkp_freq);
		if (ret) {
			dev_notice(&pdev->dev, "read clk_parent freq failed\n");
		}
		ret = of_property_read_u32(gpu_clk_dn,"voltage", &dvfs_tbl->voltage);
		if (ret) {
			dev_notice(&pdev->dev, "read voltage failed\n");
		}
		ret = of_property_read_u32(gpu_clk_dn,"keep_count", &dvfs_tbl->keep_count);
		if (ret) {
			dev_notice(&pdev->dev, "read keep_count failed\n");
		}
		//downthreshold and upthreshold shall be u32
		ret = of_property_read_u32_array(gpu_clk_dn,"threshold",
		&dvfs_tbl->downthreshold, 2);
		if (ret) {
			dev_notice(&pdev->dev, "read threshold failed\n");
		}
		dvfs_tbl->freq_index = i;
		clk_item->clock = dvfs_tbl->clk_freq / 1000000;
		clk_item->vol = dvfs_tbl->voltage;

		*clk_sample = dvfs_tbl->clk_freq / 1000000;

		dvfs_tbl ++;
		clk_item ++;
		clk_sample ++;
		i++;
		mpdata->dvfs_table_size ++;
	}

	ret = of_property_read_u32(gpu_dn,"max_clk",
			&mpdata->cfg_clock);
	if (ret) {
		dev_notice(&pdev->dev, "max clk set %d\n", mpdata->dvfs_table_size-2);
		mpdata->cfg_clock = mpdata->dvfs_table_size-2;
	}

	mpdata->cfg_clock_bkup = mpdata->cfg_clock;
	mpdata->maxclk_sysfs = mpdata->cfg_clock;
	mpdata->scale_info.maxclk = mpdata->cfg_clock;
	_dev_info(&pdev->dev, "max clk  is %d\n", mpdata->scale_info.maxclk);

	ret = of_property_read_u32(gpu_dn,"turbo_clk",
			&mpdata->turbo_clock);
	if (ret) {
		dev_notice(&pdev->dev, "turbo clk set to %d\n", mpdata->dvfs_table_size-1);
		mpdata->turbo_clock = mpdata->dvfs_table_size-1;
	}
	_dev_info(&pdev->dev, "turbo clk  is %d\n", mpdata->turbo_clock);

	ret = of_property_read_u32(gpu_dn,"def_clk",
			&mpdata->def_clock);
	if (ret) {
		dev_notice(&pdev->dev, "default clk set to %d\n", mpdata->dvfs_table_size/2-1);
		mpdata->def_clock = mpdata->dvfs_table_size/2 - 1;
	}
	if (mpdata->def_clock > mpdata->scale_info.maxclk)
		mpdata->def_clock = mpdata->scale_info.maxclk;
	_dev_info(&pdev->dev, "default clk  is %d\n", mpdata->def_clock);

	dvfs_tbl = mpdata->dvfs_table;
	clk_sample = mpdata->clk_sample;
	for (i = 0; i< mpdata->dvfs_table_size; i++) {
		_dev_info(&pdev->dev, "====================%d====================\n"
		            "clk_freq=%10d, clk_parent=%9s, voltage=%d, keep_count=%d, threshold=<%d %d>, clk_sample=%d\n",
					i,
					dvfs_tbl->clk_freq, dvfs_tbl->clk_parent,
					dvfs_tbl->voltage,  dvfs_tbl->keep_count,
					dvfs_tbl->downthreshold, dvfs_tbl->upthreshold, *clk_sample);
		dvfs_tbl ++;
		clk_sample ++;
	}
	_dev_info(&pdev->dev, "clock dvfs table size is %d\n", mpdata->dvfs_table_size);

	mpdata->clk_mali = devm_clk_get(&pdev->dev, "clk_mali");
	mpdata->clk_mali_0 = devm_clk_get(&pdev->dev, "clk_mali_0");
	mpdata->clk_mali_1 = devm_clk_get(&pdev->dev, "clk_mali_1");
	if (IS_ERR(mpdata->clk_mali) || IS_ERR(mpdata->clk_mali_0) || IS_ERR(mpdata->clk_mali_1)) {
		dev_err(&pdev->dev, "failed to get clock pointer\n");
		return -EFAULT;
	}

	pmali_plat = mpdata;
	mpdata->pdev = pdev;
	return 0;
}
#else
int mali_clock_init_clk_tree(struct platform_device* pdev)
{
	//mali_dvfs_threshold_table *dvfs_tbl = &pmali_plat->dvfs_table[pmali_plat->def_clock];
	struct clk *clk_mali = pmali_plat->clk_mali;

	clk_prepare_enable(clk_mali);

	return 0;
}

int mali_clock_init(mali_plat_info_t *pdev)
{
	return 0;
}

int mali_clock_critical(critical_t critical, size_t param)
{
	int ret = 0;

	ret = critical(param);

	return ret;
}

static int critical_clock_set(size_t param)
{
	int ret = 0;
	unsigned int idx = param;
	mali_dvfs_threshold_table *dvfs_tbl = &pmali_plat->dvfs_table[idx];

	struct clk *clk_mali   = pmali_plat->clk_mali;
	unsigned long time_use=0;


	GPU_CLK_DBG();
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	ktime_get_real_ts64(&start);
#else
	do_gettimeofday(&start);
#endif
	ret = clk_set_rate(clk_mali, dvfs_tbl->clk_freq);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	ktime_get_real_ts64(&end);
#else
	do_gettimeofday(&end);
#endif
	GPU_CLK_DBG();

#ifndef AML_CLK_LOCK_ERROR
	clk_disable_unprepare(clk_mali_x_old);
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	time_use = (end.tv_sec - start.tv_sec)*1000000 + div64_u64(end.tv_nsec - start.tv_nsec, 1000);
#else
	time_use = (end.tv_sec - start.tv_sec)*1000000 + end.tv_usec - start.tv_usec;
#endif
	GPU_CLK_DBG("step 1, mali_mux use: %ld us\n", time_use);

	return 0;
}

int mali_clock_set(unsigned int clock)
{
	return mali_clock_critical(critical_clock_set, (size_t)clock);
}

void disable_clock(void)
{
	struct clk *clk_mali = pmali_plat->clk_mali;

	GPU_CLK_DBG();
	if (__clk_is_enabled(clk_mali))
		clk_disable_unprepare(clk_mali);
}

void enable_clock(void)
{
	struct clk *clk_mali = pmali_plat->clk_mali;

	GPU_CLK_DBG();
	if (!__clk_is_enabled(clk_mali))
		clk_prepare_enable(clk_mali);
}

u32 get_mali_freq(u32 idx)
{
	if (!mali_pm_statue) {
		return pmali_plat->clk_sample[idx];
	} else {
		return 0;
	}
}

void set_str_src(u32 data)
{
	printk("gpu: %s, %s, %d\n", __FILE__, __func__, __LINE__);
}

int mali_dt_info(struct platform_device *pdev, struct mali_plat_info_t *mpdata)
{
	struct device_node *gpu_dn = pdev->dev.of_node;
	struct device_node *gpu_clk_dn;
	struct mali_gpu_clk_item *clk_item;
	phandle dvfs_clk_hdl;
	mali_dvfs_threshold_table *dvfs_tbl = NULL;
	mali_dvfs_threshold_table *last_dvfs_tbl = NULL;
	uint32_t *clk_sample = NULL;

	struct property *prop;
	const __be32 *p;
	int length = 0, i = 0;
	u32 u;

	int ret = 0;
	if (!gpu_dn) {
		dev_notice(&pdev->dev, "gpu device node not right\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(gpu_dn,"num_of_pp",
			&mpdata->cfg_pp);
	if (ret) {
		dev_notice(&pdev->dev, "set max pp to default 6\n");
		mpdata->cfg_pp = 6;
	}
	mpdata->scale_info.maxpp = mpdata->cfg_pp;
	mpdata->maxpp_sysfs = mpdata->cfg_pp;
	_dev_info(&pdev->dev, "max pp is %d\n", mpdata->scale_info.maxpp);

	ret = of_property_read_u32(gpu_dn,"min_pp",
			&mpdata->cfg_min_pp);
	if (ret) {
		dev_notice(&pdev->dev, "set min pp to default 1\n");
		mpdata->cfg_min_pp = 1;
	}
	mpdata->scale_info.minpp = mpdata->cfg_min_pp;
	_dev_info(&pdev->dev, "min pp is %d\n", mpdata->scale_info.minpp);

	ret = of_property_read_u32(gpu_dn,"min_clk",
			&mpdata->cfg_min_clock);
	if (ret) {
		dev_notice(&pdev->dev, "set min clk default to 0\n");
		mpdata->cfg_min_clock = 0;
	}
	mpdata->scale_info.minclk = mpdata->cfg_min_clock;
	_dev_info(&pdev->dev, "min clk  is %d\n", mpdata->scale_info.minclk);

	mpdata->reg_base_hiubus = of_iomap(gpu_dn, 3);
	_dev_info(&pdev->dev, "hiu io source  0x%p\n", mpdata->reg_base_hiubus);

	mpdata->reg_base_aobus = of_iomap(gpu_dn, 2);
	_dev_info(&pdev->dev, "hiu io source  0x%p\n", mpdata->reg_base_aobus);

	ret = of_property_read_u32(gpu_dn,"sc_mpp",
			&mpdata->sc_mpp);
	if (ret) {
		dev_notice(&pdev->dev, "set pp used most of time default to %d\n", mpdata->cfg_pp);
		mpdata->sc_mpp = mpdata->cfg_pp;
	}
	_dev_info(&pdev->dev, "num of pp used most of time %d\n", mpdata->sc_mpp);

	of_get_property(gpu_dn, "tbl", &length);

	length = length /sizeof(u32);
	_dev_info(&pdev->dev, "clock dvfs cfg table size is %d\n", length);

	mpdata->dvfs_table = devm_kzalloc(&pdev->dev,
								  sizeof(struct mali_dvfs_threshold_table)*length,
								  GFP_KERNEL);
	dvfs_tbl = mpdata->dvfs_table;
	if (mpdata->dvfs_table == NULL) {
		dev_err(&pdev->dev, "failed to alloc dvfs table\n");
		return -ENOMEM;
	}
	mpdata->clk_sample = devm_kzalloc(&pdev->dev, sizeof(u32)*length, GFP_KERNEL);
	if (mpdata->clk_sample == NULL) {
		dev_err(&pdev->dev, "failed to alloc clk_sample table\n");
		return -ENOMEM;
	}
	clk_sample = mpdata->clk_sample;
///////////
	mpdata->clk_items = devm_kzalloc(&pdev->dev, sizeof(struct mali_gpu_clk_item) * length, GFP_KERNEL);
	if (mpdata->clk_items == NULL) {
		dev_err(&pdev->dev, "failed to alloc clk_item table\n");
		return -ENOMEM;
	}
	clk_item = mpdata->clk_items;
//
	of_property_for_each_u32(gpu_dn, "tbl", prop, p, u) {
		dvfs_clk_hdl = (phandle) u;
		gpu_clk_dn = of_find_node_by_phandle(dvfs_clk_hdl);
		ret = of_property_read_u32(gpu_clk_dn,"clk_freq", &dvfs_tbl->clk_freq);
		if (ret) {
			dev_notice(&pdev->dev, "read clk_freq failed\n");
		}
		if (is_meson_gxl_package_805Y()) {
			if (dvfs_tbl->clk_freq >= GXBBM_MAX_GPU_FREQ) {
				continue;
			}
		}

		ret = of_property_read_string(gpu_clk_dn,"clk_parent",
				&dvfs_tbl->clk_parent);
		if (ret) {
			dev_notice(&pdev->dev, "read clk_parent failed\n");
		} else if (0 == strcmp(dvfs_tbl->clk_parent, "gp0_pll")) {
			dvfs_tbl->clkp_handle = devm_clk_get(&pdev->dev, dvfs_tbl->clk_parent);
			if (IS_ERR(dvfs_tbl->clkp_handle)) {
				dev_notice(&pdev->dev, "failed to get %s's clock pointer\n", dvfs_tbl->clk_parent);
			}
			ret = of_property_read_u32(gpu_clk_dn,"clkp_freq", &dvfs_tbl->clkp_freq);
			if (ret) {
				dev_notice(&pdev->dev, "read clk_parent freq failed\n");
			}
		}

		ret = of_property_read_u32(gpu_clk_dn,"voltage", &dvfs_tbl->voltage);
		if (ret) {
			dev_notice(&pdev->dev, "read voltage failed\n");
		}
		ret = of_property_read_u32(gpu_clk_dn,"keep_count", &dvfs_tbl->keep_count);
		if (ret) {
			dev_notice(&pdev->dev, "read keep_count failed\n");
		}
		//downthreshold and upthreshold shall be u32
		ret = of_property_read_u32_array(gpu_clk_dn,"threshold",
		&dvfs_tbl->downthreshold, 2);
		if (ret) {
			dev_notice(&pdev->dev, "read threshold failed\n");
		}
		dvfs_tbl->freq_index = i;
		clk_item->clock = dvfs_tbl->clk_freq / 1000000;
		clk_item->vol = dvfs_tbl->voltage;

		*clk_sample = dvfs_tbl->clk_freq / 1000000;

		last_dvfs_tbl = dvfs_tbl;
		dvfs_tbl ++;
		clk_item ++;
		clk_sample ++;
		i++;
		mpdata->dvfs_table_size ++;
	}

	if ((is_meson_gxl_package_805Y()) && (length > mpdata->dvfs_table_size)) {
		dvfs_tbl = last_dvfs_tbl;
		last_dvfs_tbl = dvfs_tbl - 1;
		if ((dvfs_tbl != NULL) && (dvfs_tbl->clk_freq != last_dvfs_tbl->clk_freq)) {
			dvfs_tbl ++;
			last_dvfs_tbl ++;
			memcpy(dvfs_tbl, last_dvfs_tbl, sizeof(*dvfs_tbl));
			*clk_sample = dvfs_tbl->clk_freq / 1000000;
			dvfs_tbl->freq_index = i;
			mpdata->dvfs_table_size ++;
		}
	}

	ret = of_property_read_u32(gpu_dn,"max_clk",
			&mpdata->cfg_clock);
	if (ret) {
		dev_notice(&pdev->dev, "max clk set %d\n", mpdata->dvfs_table_size-2);
		mpdata->cfg_clock = mpdata->dvfs_table_size-2;
	}

	mpdata->cfg_clock_bkup = mpdata->cfg_clock;
	mpdata->maxclk_sysfs = mpdata->cfg_clock;
	mpdata->scale_info.maxclk = mpdata->cfg_clock;
	_dev_info(&pdev->dev, "max clk  is %d\n", mpdata->scale_info.maxclk);

	ret = of_property_read_u32(gpu_dn,"turbo_clk",
			&mpdata->turbo_clock);
	if (ret) {
		dev_notice(&pdev->dev, "turbo clk set to %d\n", mpdata->dvfs_table_size-1);
		mpdata->turbo_clock = mpdata->dvfs_table_size-1;
	}
	_dev_info(&pdev->dev, "turbo clk  is %d\n", mpdata->turbo_clock);

	ret = of_property_read_u32(gpu_dn,"def_clk",
			&mpdata->def_clock);
	if (ret) {
		dev_notice(&pdev->dev, "default clk set to %d\n", mpdata->dvfs_table_size/2-1);
		mpdata->def_clock = mpdata->dvfs_table_size/2 - 1;
	}
	if (mpdata->def_clock > mpdata->scale_info.maxclk)
		mpdata->def_clock = mpdata->scale_info.maxclk;
	_dev_info(&pdev->dev, "default clk  is %d\n", mpdata->def_clock);

	dvfs_tbl = mpdata->dvfs_table;
	clk_sample = mpdata->clk_sample;
	for (i = 0; i< mpdata->dvfs_table_size; i++) {
		_dev_info(&pdev->dev, "====================%d====================\n"
		            "clk_freq=%10d, clk_parent=%9s, voltage=%d, keep_count=%d, threshold=<%d %d>, clk_sample=%d\n",
					i,
					dvfs_tbl->clk_freq, dvfs_tbl->clk_parent,
					dvfs_tbl->voltage,  dvfs_tbl->keep_count,
					dvfs_tbl->downthreshold, dvfs_tbl->upthreshold, *clk_sample);
		dvfs_tbl ++;
		clk_sample ++;
	}
	_dev_info(&pdev->dev, "clock dvfs table size is %d\n", mpdata->dvfs_table_size);

	mpdata->clk_mali = devm_clk_get(&pdev->dev, "gpu_mux");
#if 0
	mpdata->clk_mali_0 = devm_clk_get(&pdev->dev, "clk_mali_0");
	mpdata->clk_mali_1 = devm_clk_get(&pdev->dev, "clk_mali_1");
#endif
	if (IS_ERR(mpdata->clk_mali)) {
		dev_err(&pdev->dev, "failed to get clock pointer\n");
		return -EFAULT;
	}

	pmali_plat = mpdata;
	mpdata->pdev = pdev;
	return 0;
}

#endif
