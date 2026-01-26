// SPDX-License-Identifier: GPL-2.0
/*
 * Deepx Runtime Driver
 *
 * Copyright (C) 2023 Deepx, Inc.
 *
 */
#ifndef __DXRT_DRV_DSP_H
#define __DXRT_DRV_DSP_H
#include <linux/types.h>
#include <linux/sizes.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include "dxrt_drv_common.h"

//#include "npu_reg_sys_DX_V3.h"
//#include "npu_reg_dma_DX_V3.h"
//#include "npu_reg_debug_DX_V3.h"

//typedef dxSYSTEM_t dsp_reg_sys_t;
//typedef dxDMA_t dsp_reg_dma_t;

struct dxdev;
struct _dxrt_response_t;
struct dxdsp {
    int id;
    struct dxdev *dx;
    struct device *dev;
    uint32_t clock_khz;
    //uint32_t reg_base_phy_addr;
    uint32_t reg_dsp_base_phy_addr;
    uint32_t reg_dsp_base_phy_addr_debug_pwr;
    uint32_t reg_dsp_base_phy_addr_debug;
    uint32_t reg_dsp_base_phy_addr_mailbox;
    uint32_t reg_dsp_base_phy_addr_sram;
    uint32_t reg_dsp_base_phy_addr_rom_ram;    
    uint32_t reg_dsp_base_phy_addr_dram;
    //volatile void __iomem *reg_base;
    volatile void __iomem *reg_dsp_base;
    volatile void __iomem *reg_dsp_base_debug_pwr;
    volatile void __iomem *reg_dsp_base_debug;
    volatile void __iomem *reg_dsp_base_mailbox;
    volatile void __iomem *reg_dsp_base_sram;    
    volatile void __iomem *reg_dsp_base_dram;
    //dsp_reg_sys_t *reg_sys;
    //dsp_reg_dma_t *reg_dma;
    //dsp_reg_sys_t *reg_dsp_sys;
    //dsp_reg_sys_t *reg_dsp_debug_pwr;
    //dsp_reg_sys_t *reg_dsp_debug;
    //dsp_reg_sys_t *reg_dsp_mailbox;
    //dsp_reg_sys_t *reg_dsp_sram;
    //dsp_reg_sys_t *reg_dsp_code;
    //dsp_reg_sys_t *reg_dsp_dram;
    dma_addr_t dma_buf_addr;
    size_t dma_buf_size;
    void *dma_buf;
    uint32_t req_id;
    int irq_num;    
    int irq_event;
    // spinlock_t status_lock;
    // int status;
    spinlock_t irq_event_lock;
    wait_queue_head_t irq_wq;
    //struct mutex run_lock;
    uint32_t default_values[3];
    struct _dxrt_response_t *response;
    int (*init)(struct dxdsp*);
    int (*prepare_inference)(struct dxdsp*);
    int (*run)(struct dxdsp*, void *);
    int (*reg_dump)(struct dxdsp*);
    int (*deinit)(struct dxdsp*);
};
struct dxdsp_cfg {
    uint32_t clock_khz;
    //uint32_t reg_base_phy_addr;
    size_t dma_buf_size;        
    int irq_num;
    uint32_t default_values[3];
    int (*init)(struct dxdsp*);
    int (*prepare_inference)(struct dxdsp*);
    int (*run)(struct dxdsp*, void *);
    int (*reg_dump)(struct dxdsp*);
    int (*deinit)(struct dxdsp*);
};
typedef struct dxdsp dxdsp_t;
typedef struct dxdsp_cfg dxdsp_cfg_t;

struct dxdsp *dxrt_dsp_init(void *);
void dxrt_dsp_deinit(void *dxdev_);
int dx_v3_dsp_init(dxdsp_t *dsp);
int dx_v3_dsp_reset_and_start(dxdsp_t *dsp);
int dx_v3_dsp_prepare_inference(dxdsp_t *dsp);
int dx_v3_dsp_run(dxdsp_t *dsp, void*);
int dx_v3_dsp_reg_dump(dxdsp_t *dsp);
int dx_v3_dsp_deinit(dxdsp_t *dsp);
#endif // __DXRT_DRV_DSP_H
