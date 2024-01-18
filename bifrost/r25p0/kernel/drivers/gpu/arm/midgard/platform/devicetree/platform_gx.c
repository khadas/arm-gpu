/*
 * platform_gx.c
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
#include <linux/version.h>
#include <linux/pm.h>
#include <linux/of.h>
#include <linux/module.h>            /* kernel module definitions */
#include <linux/ioport.h>            /* request_mem_region */
#include <linux/slab.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 29))
#include <mach/register.h>
#include <mach/irqs.h>
#include <mach/io.h>
#endif
#include <asm/io.h>
#ifdef CONFIG_AMLOGIC_GPU_THERMAL
#include <linux/amlogic/gpu_cooling.h>
#include <linux/amlogic/gpucore_cooling.h>
//#include <linux/amlogic/aml_thermal_hw.h>
#include <linux/amlogic/meson_cooldev.h>
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
#include <linux/units.h>
#endif
#include <mali_kbase.h>
#include <mali_kbase_defs.h>
#ifdef CONFIG_MALI_DEVFREQ
#include <../drivers/devfreq/governor.h>
#endif
#include "mali_scaling.h"
#include "mali_clock.h"
#include "meson_main2.h"

/*
 *    For Meson 8 M2.
 *
 */
static void mali_plat_preheat(void);
static mali_plat_info_t mali_plat_data = {
    .bst_gpu = 210, /* threshold for boosting gpu. */
    .bst_pp = 160, /* threshold for boosting PP. */
    .have_switch = 1,
    .limit_on = 1,
    .plat_preheat = mali_plat_preheat,
};

static void mali_plat_preheat(void)
{
#ifdef CONFIG_MALI_DEVFREQ
    mali_plat_info_t* pmali_plat = get_mali_plat_data();
    struct platform_device* ptr_plt_dev = pmali_plat->pdev;
    struct kbase_device *kbdev = dev_get_drvdata(&ptr_plt_dev->dev);
    struct devfreq *devfreq = kbdev->devfreq;

    if (!devfreq) {
        dev_warn(kbdev->dev, "%s, kbdev->devfreq is NULL\n", __func__);
        return;
    }
    if (strncmp(devfreq->governor->name, DEVFREQ_GOV_SIMPLE_ONDEMAND,
        strlen(DEVFREQ_GOV_SIMPLE_ONDEMAND))) {
        dev_warn(kbdev->dev, "%s, current governor is not ondemand\n", __func__);
        return;
    }

    mutex_lock(&devfreq->lock);
    pmali_plat->status = PREHEAT_START;
    devfreq->profile->target(devfreq->dev.parent, &devfreq->scaling_max_freq, 0);
    mutex_unlock(&devfreq->lock);
#else
    u32 pre_fs;
    u32 clk, pp;

    if (get_mali_schel_mode() != MALI_PP_FS_SCALING)
        return;

    get_mali_rt_clkpp(&clk, &pp);
    pre_fs = mali_plat_data.def_clock + 1;
    if (clk < pre_fs)
        clk = pre_fs;
    if (pp < mali_plat_data.sc_mpp)
        pp = mali_plat_data.sc_mpp;
    set_mali_rt_clkpp(clk, pp, 1);
#endif
}

mali_plat_info_t* get_mali_plat_data(void) {
    return &mali_plat_data;
}

int get_mali_freq_level(int freq)
{
#ifdef CONFIG_MALI_DEVFREQ
    mali_plat_info_t* pmali_plat = get_mali_plat_data();
    struct platform_device* ptr_plt_dev = pmali_plat->pdev;
    struct kbase_device *kbdev = dev_get_drvdata(&ptr_plt_dev->dev);
    struct devfreq *devfreq = kbdev->devfreq;
    unsigned long *freq_table;
    int i = 0, level = -1;
    int mali_freq_num;

    if (freq < 0)
        return level;

    mali_freq_num = devfreq->profile->max_state - 1;
    freq_table = devfreq->profile->freq_table;
    if (freq <= freq_table[mali_freq_num] / 1000000)
        level = mali_freq_num;
    else if (freq >= freq_table[0] / 1000000)
        level = 0;
    else {
        for (i = 0; i < mali_freq_num; i++) {
            if (freq < freq_table[i] / 1000000 && freq >= freq_table[i + 1] / 1000000)
                level = i + 1;
        }
    }
    return level;
#else
    int i = 0, level = -1;
    int mali_freq_num;

    if (freq < 0)
        return level;

    mali_freq_num = mali_plat_data.dvfs_table_size - 1;
    if (freq <= mali_plat_data.clk_sample[0])
        level = mali_freq_num - 1;
    else if (freq >= mali_plat_data.clk_sample[mali_freq_num - 1])
        level = 0;
    else {
        for (i = 0; i < mali_freq_num - 1; i++) {
            if (freq >= mali_plat_data.clk_sample[i] && freq <= mali_plat_data.clk_sample[i + 1]) {
                level = i;
                level = mali_freq_num - level - 1;
            }
        }
    }
    return level;
#endif
}

unsigned int get_mali_max_level(void)
{
#ifdef CONFIG_MALI_DEVFREQ
    mali_plat_info_t* pmali_plat = get_mali_plat_data();
    struct platform_device* ptr_plt_dev = pmali_plat->pdev;
    struct kbase_device *kbdev = dev_get_drvdata(&ptr_plt_dev->dev);
    struct devfreq *devfreq = kbdev->devfreq;

    if (!devfreq) {
        dev_warn(kbdev->dev, "%s, devfreq is NULL!\n", __func__);
        return 5;
    }
    return devfreq->profile->max_state;
#else
    return mali_plat_data.dvfs_table_size - 1;
#endif
}

int get_gpu_max_clk_level(void)
{
    return mali_plat_data.cfg_clock;
}

#ifdef CONFIG_AMLOGIC_GPU_THERMAL
static void set_limit_mali_freq(u32 idx)
{
#ifdef CONFIG_MALI_DEVFREQ
    mali_plat_info_t* pmali_plat = get_mali_plat_data();
    struct platform_device* ptr_plt_dev = pmali_plat->pdev;
    struct kbase_device *kbdev = dev_get_drvdata(&ptr_plt_dev->dev);
    struct devfreq *devfreq = kbdev->devfreq;
    unsigned long value;
    unsigned long *freq_table;

    if (!devfreq || (idx >= devfreq->profile->max_state)) {
        dev_warn(kbdev->dev, "%s, idx:%d\n", __func__, idx);
        return;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
    freq_table = devfreq->profile->freq_table;

    /* Get maximum frequency according to sorting order
     * freq_table[0] > freq_table[1] > ... > freq_table[4], so freq_table[0] is max_freq */
    value = freq_table[devfreq->profile->max_state - 1 - idx];

    value = DIV_ROUND_UP(value, HZ_PER_KHZ);
    dev_pm_qos_update_request(&devfreq->user_max_freq_req, value);
    update_devfreq(devfreq);
#else
    mutex_lock(&devfreq->lock);
    freq_table = devfreq->profile->freq_table;

    /* Get maximum frequency according to sorting order */
    if (freq_table[0] < freq_table[devfreq->profile->max_state - 1])
        value = freq_table[idx];
    else
        value = freq_table[devfreq->profile->max_state - 1 - idx];

    devfreq->max_freq = value;
    update_devfreq(devfreq);
    mutex_unlock(&devfreq->lock);
#endif
#else
    if (mali_plat_data.limit_on == 0)
        return;
    if (idx > mali_plat_data.turbo_clock || idx < mali_plat_data.scale_info.minclk)
        return;
    if (idx > mali_plat_data.maxclk_sysfs) {
        printk("idx > max freq\n");
        return;
    }
    mali_plat_data.scale_info.maxclk = idx;
    revise_mali_rt();
#endif
}

static u32 get_limit_mali_freq(void)
{
#ifdef CONFIG_MALI_DEVFREQ
    mali_plat_info_t* pmali_plat = get_mali_plat_data();
    struct platform_device* ptr_plt_dev = pmali_plat->pdev;
    struct kbase_device *kbdev = dev_get_drvdata(&ptr_plt_dev->dev);
    struct devfreq *devfreq = kbdev->devfreq;
    unsigned long *freq_table;
    int i = 0;
    u32 idx;

    freq_table = devfreq->profile->freq_table;
    for (i = 0; i < devfreq->profile->max_state; i++) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
        u32 qos_max_freq = devfreq->user_max_freq_req.data.freq.qos->max_freq.target_value;
        if (qos_max_freq > DIV_ROUND_UP(freq_table[0], HZ_PER_KHZ))
            break;
        if (qos_max_freq == DIV_ROUND_UP(freq_table[i], HZ_PER_KHZ))
            break;
#else
        if (devfreq->max_freq == freq_table[i])
            break;
#endif
    }
    idx = devfreq->profile->max_state - 1 - i;
    return idx;
#else
    return mali_plat_data.scale_info.maxclk;
#endif
}

#ifdef CONFIG_DEVFREQ_THERMAL
static u32 get_mali_utilization(void)
{
    u32 util = mpgpu_get_utilization();
    return (util * 100) / 256;
}
#endif
#endif

#ifdef CONFIG_AMLOGIC_GPU_THERMAL
static u32 set_limit_pp_num(u32 num)
{
    u32 ret = -1;
    if (mali_plat_data.limit_on == 0)
        goto quit;
    if (num > mali_plat_data.cfg_pp ||
            num < mali_plat_data.scale_info.minpp)
        goto quit;

    if (num > mali_plat_data.maxpp_sysfs) {
        printk("pp > sysfs set pp\n");
        goto quit;
    }

    mali_plat_data.scale_info.maxpp = num;
    revise_mali_rt();
    ret = 0;
quit:
    return ret;
}
#ifdef CONFIG_DEVFREQ_THERMAL
extern u64 kbase_pm_get_ready_cores(struct kbase_device *kbdev, enum kbase_pm_core_type type);
static u32 mali_get_online_pp(void)
{
    u64 core_ready;
    u64 l2_ready;
    u64 tiler_ready;
    unsigned long flags;
    mali_plat_info_t* pmali_plat = get_mali_plat_data();
    struct platform_device* ptr_plt_dev = pmali_plat->pdev;
    struct kbase_device *kbdev = dev_get_drvdata(&ptr_plt_dev->dev);


    spin_lock_irqsave(&kbdev->hwaccess_lock, flags);
    if (!kbdev->pm.backend.gpu_powered) {
        spin_unlock_irqrestore(&kbdev->hwaccess_lock, flags);
        return 0;
    }

    core_ready = kbase_pm_get_ready_cores(kbdev, KBASE_PM_CORE_SHADER);
    l2_ready = kbase_pm_get_ready_cores(kbdev, KBASE_PM_CORE_L2);
    tiler_ready = kbase_pm_get_ready_cores(kbdev, KBASE_PM_CORE_TILER);
    spin_unlock_irqrestore(&kbdev->hwaccess_lock, flags);

    if (!core_ready && !l2_ready && !tiler_ready) {
        return 0;
    }

    return 2;
}
#endif
#endif

int mali_meson_init_start(struct platform_device* ptr_plt_dev)
{
    struct kbase_device *kbdev = dev_get_drvdata(&ptr_plt_dev->dev);

    mali_dt_info(ptr_plt_dev, &mali_plat_data);
    mali_clock_init_clk_tree(ptr_plt_dev);

    kbdev->platform_context = &mali_plat_data;
    return 0;
}

int mali_meson_init_finish(struct platform_device* ptr_plt_dev)
{
    if (mali_core_scaling_init(&mali_plat_data) < 0)
        return -1;
    return 0;
}

int mali_meson_uninit(struct platform_device* ptr_plt_dev)
{
    mali_core_scaling_term();
    return 0;
}

void mali_post_init(void)
{
#ifdef CONFIG_AMLOGIC_GPU_THERMAL
    int err;
    struct gpufreq_cooling_device *gcdev = NULL;
    struct gpucore_cooling_device *gccdev = NULL;

    gcdev = gpufreq_cooling_alloc();
    register_gpu_freq_info(get_current_frequency);
    if (IS_ERR(gcdev))
        printk("malloc gpu cooling buffer error!!\n");
    else if (!gcdev)
        printk("system does not enable thermal driver\n");
    else {
        gcdev->get_gpu_freq_level = get_mali_freq_level;
        gcdev->get_gpu_max_level = get_mali_max_level;
        gcdev->set_gpu_freq_idx = set_limit_mali_freq;
        gcdev->get_gpu_current_max_level = get_limit_mali_freq;
#ifdef CONFIG_DEVFREQ_THERMAL
        gcdev->get_gpu_freq = get_mali_freq;
        gcdev->get_gpu_loading = get_mali_utilization;
        gcdev->get_online_pp = mali_get_online_pp;
#endif
        err = gpufreq_cooling_register(gcdev);
#ifdef CONFIG_DEVFREQ_THERMAL
        meson_gcooldev_min_update(gcdev->cool_dev);
#endif
        if (err < 0)
            printk("register GPU  cooling error\n");
    }

    gccdev = gpucore_cooling_alloc();
    if (IS_ERR(gccdev))
        printk("malloc gpu core cooling buffer error!!\n");
    else if (!gccdev)
        printk("system does not enable thermal driver\n");
    else {
        gccdev->max_gpu_core_num = mali_plat_data.cfg_pp;
        gccdev->set_max_pp_num = set_limit_pp_num;
        err = (int)gpucore_cooling_register(gccdev);
#ifdef CONFIG_DEVFREQ_THERMAL
        meson_gcooldev_min_update(gccdev->cool_dev);
#endif
        if (err < 0)
            printk("register GPU  cooling error\n");
    }
#endif
}
