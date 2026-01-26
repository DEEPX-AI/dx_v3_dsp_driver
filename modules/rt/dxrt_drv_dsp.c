// SPDX-License-Identifier: GPL-2.0
/*
 * Deepx Runtime Driver
 *
 * Copyright (C) 2023 Deepx, Inc.
 *
 */
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/spinlock.h>

#include "dxrt_drv.h"

struct dxdsp_cfg dsp_cfg = {
#if DEVICE_VARIANT==DX_V3
    /* DX-V3 */
    .clock_khz = 800000,//74250,
    .default_values = {0x3C0, 0x200, 0x2C0},
    .init = dx_v3_dsp_init,
    .prepare_inference = dx_v3_dsp_prepare_inference,
    .run = dx_v3_dsp_run,
    .reg_dump = dx_v3_dsp_reg_dump,
    .deinit = dx_v3_dsp_deinit,    
#else
    0,
#endif
};

struct dxdsp *dxrt_dsp_init(void *dxdev_)
{
    struct dxdev *dxdev = (struct dxdev *)dxdev_;
    struct dxdsp *dsp = NULL;
    pr_debug( "%s\n", __func__);
    {
        int variant = dxdev->variant;
        int idx = variant - 100;
        int i;
        dsp = kmalloc(sizeof(struct dxdsp), GFP_KERNEL);
        pr_debug( "%s: %d, %d\n", __func__, variant, idx);
        if (!dsp)
        {
            pr_err( "%s: failed to allocate memory\n", __func__);
            return NULL;
        }
        dsp->clock_khz = dsp_cfg.clock_khz;
        for(i=0;i<3;i++) dsp->default_values[i] = dsp_cfg.default_values[i];
        dsp->dev = dxdev->dev;
        dsp->init = dsp_cfg.init;
        dsp->prepare_inference = dsp_cfg.prepare_inference;
        dsp->run = dsp_cfg.run;
        dsp->reg_dump = dsp_cfg.reg_dump;
        dsp->deinit = dsp_cfg.deinit;        
        dsp->dx = dxdev;
        dsp->response = &dxdev->response;
        // dsp->status = 0;
        spin_lock_init(&dsp->irq_event_lock);
        // setup from platform device
        {
            struct platform_device *pdev = dxdev->pdev;
            struct device_node *np = pdev->dev.of_node;
            struct resource *res;
            const __be32 *prop;

            /* Register */
            // 0.DSP sys 
            res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
            if (res==NULL) {
                pr_err( "%s: failed to find IO resource0 for dsp.\n", __func__);
                return NULL;
            }
            dsp->reg_dsp_base_phy_addr = res->start;

            // 1.DSP power on for debug
            res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
            if (res==NULL) {
                pr_err( "%s: failed to find IO resource1 for dsp.\n", __func__);
                return NULL;
            }
            dsp->reg_dsp_base_phy_addr_debug_pwr = res->start;

            // 2.DSP debug
            res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
            if (res==NULL) {
                pr_err( "%s: failed to find IO resource2 for dsp.\n", __func__);
                return NULL;
            }
            dsp->reg_dsp_base_phy_addr_debug = res->start;

            // 3.DSP mailbox & UART
            res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
            if (res==NULL) {
                pr_err( "%s: failed to find IO resource3 for dsp.\n", __func__);
                return NULL;
            }
            dsp->reg_dsp_base_phy_addr_mailbox = res->start;

            // 4.DSP sram
            res = platform_get_resource(pdev, IORESOURCE_MEM, 4);
            if (res==NULL) {
                pr_err( "%s: failed to find IO resource4 for dsp.\n", __func__);
                return NULL;
            }
            dsp->reg_dsp_base_phy_addr_sram = res->start;			
            pr_debug( "%s sram phy_addr = %x \n", __func__, dsp->reg_dsp_base_phy_addr_sram);

            // 5.DSP rom & ram code
            res = platform_get_resource(pdev, IORESOURCE_MEM, 5);
            if (res==NULL) {
                pr_err( "%s: failed to find IO resource5 for dsp.\n", __func__);
                return NULL;
            }
            dsp->reg_dsp_base_phy_addr_rom_ram = res->start;
            pr_debug( "%s rom_ram phy_addr = %x \n", __func__, dsp->reg_dsp_base_phy_addr_rom_ram);

            // 6.DSP dram
            res = platform_get_resource(pdev, IORESOURCE_MEM, 6);
            if (res==NULL) {
                pr_err( "%s: failed to find IO resource6 for dsp.\n", __func__);
                return NULL;
            }
            dsp->reg_dsp_base_phy_addr_dram = res->start;			
			pr_debug( "%s dram phy_addr = %x \n", __func__, dsp->reg_dsp_base_phy_addr_dram);

            /* IRQ */
            dsp->irq_num = platform_get_irq(pdev, 0);//HOST_EXTSYS0_MHU0_SENDER_INTR = 43
            if (dsp->irq_num < 0) {
                pr_err( "%s: failed to find IRQ number for dsp.\n", __func__);
                return NULL;
            }
            
            {
                prop = of_get_property(np, "device-id", NULL);
                if (prop==NULL)
                {
                    pr_err( "%s: failed to find device-id for dsp.\n", __func__);
                    return NULL;
                }
                dsp->id = be32_to_cpup(prop);
            }
            {
                prop = of_get_property(np, "dma-buf-size", NULL);
                if (prop==NULL)
                {
                    pr_err( "%s: failed to find dma-buf-size for dsp.\n", __func__);
                    return NULL;
                }
                dsp->dma_buf_size = be32_to_cpup(prop);
            }
        }
    }
    if (dsp) {
        dsp->init(dsp);
    }
    return dsp;
}
void dxrt_dsp_deinit(void *dxdev_)
{
    struct dxdsp *dsp = NULL;    
    pr_debug( "%s\n", __func__);

    struct dxdev *dx = dxdev_;    
    dsp = dx->dsp;

    if (dsp) {
        dsp->deinit(dsp);
        kfree(dsp);
    }
}