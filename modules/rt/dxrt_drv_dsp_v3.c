// SPDX-License-Identifier: GPL-2.0
/*
 * Deepx Runtime Driver
 *
 * Copyright (C) 2023 Deepx, Inc.
 *
 */

#include "dxrt_drv_dsp.h"
#include "dxrt_drv.h"

// Global DSP memory manager instance
dxrt_dsp_buffer_manager_t g_dsp_memory_manager;

inline void dsp_reg_write(volatile void __iomem *base, uint32_t addr, uint32_t val)
{
    pr_debug("write: %x, %x\n", addr, val);
    iowrite32(val, base + addr);
}
inline void dsp_reg_write_mask(volatile void __iomem *base, uint32_t addr, uint32_t val, uint32_t mask, uint32_t bit_offset)
{
    uint32_t read_val = ioread32(base + addr);
    read_val &= ~mask;
    read_val |= (val << bit_offset) & mask;
    pr_debug("write_mask: %x, %x\n", addr, read_val);
    iowrite32(read_val, base + addr);
}
inline uint32_t dsp_reg_read(volatile void __iomem *base, uint32_t addr)
{
    pr_debug("read: %x\n", addr);
    return ioread32(base + addr);
}
inline uint32_t dsp_reg_read_mask(volatile void __iomem *base, uint32_t addr, uint32_t mask, uint32_t bit_offset)
{
    uint32_t read_val = ioread32(base + addr);
    pr_debug("read_mask: %x, %x, %x\n", addr, mask, bit_offset);
    return (read_val & mask) >> bit_offset;
}

static irqreturn_t dsp_irq_handler(int irq, void *data)
{
    unsigned long flags;
    dxdsp_t *dsp = (dxdsp_t*)data;    
    volatile void __iomem *reg_dsp_mailbox = dsp->reg_dsp_base_mailbox;
    volatile void __iomem *reg_dsp = dsp->reg_dsp_base;
    dxrt_response_t *response = dsp->response;
    
    uint32_t irq_status_ch0, irq_status_ch1;

    pr_debug("dsp%d irq: %x\n", dsp->id, irq); // this log causes worse latency.

    // IRQ status
#if 1	
    irq_status_ch0 = READ_DSP_IRQ_STATUS_CH0(reg_dsp_mailbox);
    irq_status_ch1 = READ_DSP_IRQ_STATUS_CH1(reg_dsp_mailbox);

    pr_debug("%s irq_status_ch0 =%d\n", __func__, irq_status_ch0);
    pr_debug("%s irq_status_ch1 =%d\n", __func__, irq_status_ch1);
#endif

    // get response
    response->req_id = dsp->req_id;
    
    // clear IRQ    
    WRITE_DSP_IRQ_CLR_CH0(reg_dsp_mailbox, 1);
    WRITE_DSP_IRQ_CLR_CH1(reg_dsp_mailbox, 1);

    // set dsp state to idle
    WRITE_DSP_STATUS(reg_dsp, 0x0);

    // wakeup waitqueue
    spin_lock_irqsave(&dsp->irq_event_lock, flags);
	dsp->irq_event = 1;	
	spin_unlock_irqrestore(&dsp->irq_event_lock, flags);
    wake_up_interruptible(&dsp->irq_wq);

	return IRQ_HANDLED;
}
static void dx_v3_dsp_irq_init(dxdsp_t *dsp)
{  
    volatile void __iomem *reg_dsp_mailbox = dsp->reg_dsp_base_mailbox;
    pr_debug("%s\n", __func__);
    WRITE_DSP_IRQ_INT_EN(reg_dsp_mailbox, (1<<2));// Interrupt enable only CHCOMB
  
    // Enable generation of the Channel clear interrupt
    WRITE_DSP_IRQ_EN_CH0(reg_dsp_mailbox, 1);
    WRITE_DSP_IRQ_EN_CH1(reg_dsp_mailbox, 1);
}
static void dx_v3_dsp_clock_enable(dxdsp_t *dsp)
{      
    pr_debug("%s\n", __func__); 
    WRITE_DSP_CLOCK_CTRL(dsp->reg_dsp_base, 0x07);// DSP clock enable    
}
static void dx_v3_dsp_clock_enable_and_ready(dxdsp_t *dsp)
{    
    pr_debug("%s\n", __func__);  
    WRITE_DSP_CLOCK_CTRL(dsp->reg_dsp_base, 0x07);// DSP clock enable    
    WRITE_DSP_RUNSTALL  (dsp->reg_dsp_base, 0x3C);// DSP halt (ready)
}
static void dx_v3_dsp_clock_disable(dxdsp_t *dsp)
{      
    pr_debug("%s\n", __func__);
    WRITE_DSP_CLOCK_CTRL(dsp->reg_dsp_base, 0x00);// DSP clock disable    
}
static void dx_v3_dsp_start(dxdsp_t *dsp)
{    
    pr_debug("%s\n", __func__);  
#if 0//use default reset vector     
    WRITE_DSP_RUNSTALL  (dsp->reg_dsp_base, 0x3C);// DSP halt
    WRITE_DSP_RUNSTALL  (dsp->reg_dsp_base, 0x1C);// DSP release
#else//use new reset vector
    WRITE_DSP_RESET_CTRL(dsp->reg_dsp_base, dsp->reg_dsp_base_phy_addr_rom_ram);//new reset vector(instruction binary) address    
    WRITE_DSP_RUNSTALL  (dsp->reg_dsp_base, 0x7C);// DSP halt
    WRITE_DSP_RUNSTALL  (dsp->reg_dsp_base, 0x5C);// DSP release
#endif
    dx_v3_dsp_reset_and_start(dsp);//dsp reset and restart    
}
int dx_v3_dsp_reset_and_start(dxdsp_t *dsp)
{    
    volatile void __iomem *reg_dsp = dsp->reg_dsp_base;
    volatile void __iomem *reg_dsp_debug = dsp->reg_dsp_base_debug;

    pr_debug("%s\n", __func__);

    //WRITE_DSP_RESET_CTRL(reg_dsp, 0xE0000000);//reset vector
    WRITE_DSP_RESET(reg_dsp_debug, 0x10000);
    udelay(1);
    WRITE_DSP_RESET(reg_dsp_debug, 0x0);
    
    return 0;
}
int dx_v3_dsp_buf_init(void)
{
    pr_debug("%s\n", __func__);
    memset(&g_dsp_memory_manager, 0, sizeof(g_dsp_memory_manager));

    return 0;
}
int dx_v3_dsp_init(dxdsp_t *dsp)
{
    int ret;

    pr_debug("%s\n", __func__);
    /* Register */
    // 0.DSP sys
    dsp->reg_dsp_base = ioremap(dsp->reg_dsp_base_phy_addr, 0x1000);
    if(!dsp->reg_dsp_base)
    {
        pr_err("Failed to map dsp registers0\n");
        return -ENOMEM;
    }
    //dsp->reg_dsp_sys = (volatile dsp_reg_sys_t*)dsp->reg_dsp_base;    

    // 1.DSP power on for debug
    dsp->reg_dsp_base_debug_pwr = ioremap(dsp->reg_dsp_base_phy_addr_debug_pwr, 0x1000);
    if(!dsp->reg_dsp_base_debug_pwr)
    {
        pr_err("Failed to map dsp registers1\n");
        return -ENOMEM;
    }

    // 2.DSP debug
    dsp->reg_dsp_base_debug = ioremap(dsp->reg_dsp_base_phy_addr_debug, 0x1000);
    if(!dsp->reg_dsp_base_debug)
    {
        pr_err("Failed to map dsp registers2\n");
        return -ENOMEM;
    }

    // 3.DSP mailbox
    dsp->reg_dsp_base_mailbox = ioremap(dsp->reg_dsp_base_phy_addr_mailbox, 0x100000);
    if(!dsp->reg_dsp_base_mailbox)
    {
        pr_err("Failed to map dsp registers3\n");
        return -ENOMEM;
    }

    // 4.DSP sram
    dsp->reg_dsp_base_sram = ioremap(dsp->reg_dsp_base_phy_addr_sram, 0x40000);
    if(!dsp->reg_dsp_base_sram)
    {
        pr_err("Failed to map dsp registers4\n");
        return -ENOMEM;
    }
	// 5.DSP rom & ram code
	// don't use this range (Base + 0x0000_0000 : rom code, Base + 0x0010_0000 : ram code) 

    // 6.dram    
    dsp->reg_dsp_base_dram = ioremap(dsp->reg_dsp_base_phy_addr_dram, DSP_DRAM_SIZE);// = 0x1FE00000
    if(!dsp->reg_dsp_base_dram)
    {
        pr_err("Failed to map dsp registers5\n");
        return -ENOMEM;
    }

    dx_v3_dsp_buf_init();
    
    /* IRQ */
    ret = request_irq(dsp->irq_num, dsp_irq_handler, 0, "deepx-dsp", (void*)dsp);
    if(ret)
    {
        pr_err("Failed to request IRQ (DSP) %d.\n", dsp->irq_num);
        return ret;
    }
    dsp->irq_event = 0;
    init_waitqueue_head(&dsp->irq_wq);
    //mutex_init(&dsp->run_lock);

    dx_v3_dsp_irq_init(dsp);//interrupt enable

    /* DMA Buffer */    
    dsp->dma_buf = dma_alloc_coherent(dsp->dev, dsp->dma_buf_size, &dsp->dma_buf_addr, GFP_KERNEL);
    if (!dsp->dma_buf) {
        pr_err("Failed to allocate dma_buf.\n");
        return -1;
    }        
    pr_info("dma_buf : virt 0x%p, phys 0x%llx, virt_to_phys 0x%lx, size 0x%lx\n", 
        dsp->dma_buf, dsp->dma_buf_addr, virt_to_phys(dsp->dma_buf), dsp->dma_buf_size);
        
    // set dsp state to idle
    WRITE_DSP_STATUS(dsp->reg_dsp_base, 0x0);//dsp unlock 

    /* Init */    
	//pr_info("    dsp%d @ %x: CLOCK: %dKHz, IRQ: %d, ID: %X, MODE: %X, AXI_CFG: %X\n", 
    //    dsp->id, dsp->reg_base_phy_addr, dsp->clock_khz, dsp->irq_num, READ_SYSTEM_ID(dsp->reg_base),
    //    READ_SYSTEM_RUN_OPT(dsp->reg_base), READ_DMA_AXI_CFG0(dsp->reg_base));
    // dsp_reg_dump(dsp); // temp

    dx_v3_dsp_clock_enable_and_ready(dsp);
	dx_v3_dsp_start(dsp);    
    
    pr_info("%s done!\n", __func__);

	return 0;
}
int dx_v3_dsp_prepare_inference(dxdsp_t *dsp)
{    
    pr_debug("%s\n", __func__);
    
    return 0;
}
int dx_v3_dsp_run(dxdsp_t *dsp, void *data)
{	
	volatile void __iomem *reg_dsp_base, *reg_dsp_sram;
    dxrt_dsp_request_t *req = (dxrt_dsp_request_t*)data;	
    pr_debug("%s: %d\n", __func__, req->req_id);
    
    // Acquire mutex to ensure only one thread can execute this function
    //mutex_lock(&dsp->run_lock);
    
    reg_dsp_base = dsp->reg_dsp_base;
    reg_dsp_sram = dsp->reg_dsp_base_sram;

    //wait until DSP is available
    while(READ_DSP_STATUS(reg_dsp_base)==0xFFAA);
    WRITE_DSP_STATUS(reg_dsp_base, 0xFFAA);//dsp lock ==> this setting should be moved to upper line on V3A       

    dsp->req_id = req->req_id;
    //dx_v3_dsp_start(dsp);
    uint32_t *head_u32ptr = (uint32_t *)&req->msg_header; //8B
    uint32_t *data_u32ptr = (uint32_t *)&req->msg_data[0];//Max 52B

    int32_t msg_buf_offset_4 = (MESSAGE_MAX_SIZE/4)*dsp->req_id;

    // Data setting
    for(int i=0;i<req->msg_header.message_size/4;i++) WRITE_DSP_MSG_DATA (reg_dsp_sram, data_u32ptr[i], (msg_buf_offset_4+i));//Max 52B
    // Header setting and DSP start
    for(int i=0;i<2;i++) WRITE_DSP_MSG_HEAD (reg_dsp_sram, head_u32ptr[i], (msg_buf_offset_4+i));//8B
        
    //WRITE_DSP_STATUS(reg_dsp_base, 0xFFAA);//dsp lock ==> this setting should be moved to upper line on V3A
    
    // Release mutex after operation is complete
   // mutex_unlock(&dsp->run_lock);
    
    return 0;
}
int dx_v3_dsp_reg_dump(dxdsp_t *dsp)
{
    //int i;
    //pr_info("==== DSP SYSTEM REG ====\n");
    //for(i=0;i<sizeof(dsp_reg_sys_t)/4;i++)
    //{
    //    pr_info("0x%08X: %X\n", i*4, dsp_reg_read(dsp->reg_base, i*4));
    //}
    //pr_info("==== DSP DMA REG ====\n");
    //for(i=0;i<sizeof(dsp_reg_dma_t)/4;i++)
    //{
    //    pr_info("0x%08X: %X\n", REG_DMA_OFFSET + i*4, dsp_reg_read(dsp->reg_base, REG_DMA_OFFSET + i*4));
    //}
    //mdelay(5000);	
    return 0;
}
int dx_v3_dsp_deinit(dxdsp_t *dsp)
{
    pr_debug("%s\n", __func__);
    //mutex_destroy(&dsp->run_lock);
    dma_free_coherent(dsp->dev, dsp->dma_buf_size, dsp->dma_buf, dsp->dma_buf_addr);
    disable_irq(dsp->irq_num);
    synchronize_irq(dsp->irq_num);
    free_irq(dsp->irq_num, (void*)dsp);

    iounmap(dsp->reg_dsp_base);
    iounmap(dsp->reg_dsp_base_debug_pwr);
    iounmap(dsp->reg_dsp_base_debug);
    iounmap(dsp->reg_dsp_base_mailbox);
    iounmap(dsp->reg_dsp_base_sram);
    iounmap(dsp->reg_dsp_base_dram);

    dx_v3_dsp_buf_init();

    //dx_v3_dsp_clock_disable(dsp);// TODO

    return 0;
}
