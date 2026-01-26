// SPDX-License-Identifier: GPL-2.0
/*
 * Deepx Runtime Driver
 *
 * Copyright (C) 2023 Deepx, Inc.
 *
 */

#include <asm/cacheflush.h>
#include "dxrt_drv.h"
#include "dxrt_version.h"

/*
* Initialization function to drive the device
*/
void dxrt_device_init(struct dxdev* dev)
{
    int num = dev->id;
    pr_debug("%s:%d type:%d\n", __func__, num, dev->type);    
}

/**
 * dxrt_msg_general - Read/Write data from/to the dxrt device 
 * @dev: The deepx device on kernel
 * @msg: User-space pointer including the data buffer
 *
 * This function copies the user-space datas to deepx device provided by the ioctl command.
 * Also, this function is general interface to communicate with the deepx device.
 * 
 * Return: 0 on success,
 *        -EFAULT    if an error occurs during the copy(user <-> kernel).
 *        -ETIMEDOUT if an error occurs during waiting from response of deepx device
 *        -ENOMEM    if an error occurs during memory allocation on kernel space
 */
static int dxrt_msg_general(struct dxdev *dev, dxrt_message_t *msg)
{
    int ret = 0;//, num = dev->id;
    pr_debug("%s: %d, %d: %llx %d\n", __func__, dev->id, dev->type, (uint64_t)msg->data, msg->size);
            
    return ret;
}

/**
 * dxrt_identify_device - Read data from the dxrt device
 * @dev: The deepx device on kernel
 * @msg: User-space pointer including the data buffer
 *
 * This function reads data[memory / size..] from the deepx device and
 * copies it to the user-space buffer provided by the ioctl command.
 *
 * Return: 0 on success,
 *        -EFAULT    if an error occurs during the copy(user <-> kernel).
 *        -ETIMEDOUT if an error occurs during waiting from response of deepx device
 *        -ENOMEM    if an error occurs during memory allocation on kernel space
 *        -ECOMM     if an error occurs because of pcie data transaction fail
 */
static int dxrt_identify_device(struct dxdev* dev, dxrt_message_t *msg)
{
    int ret = 0, num = dev->id;
    dxrt_device_info_t info;
    pr_debug("%s: %d, %d: %llx, %d\n", __func__, dev->id, dev->type, (uint64_t)msg->data, msg->size);
    info.type = dev->type;
    info.variant = dev->variant;
    memset(&dev->response, 0, sizeof(dxrt_response_t));
   
    {
        dxrt_response_list_t *entry, *tmp;
        spin_lock(&dev->responses_lock);
        list_for_each_entry_safe(entry, tmp, &dev->responses.list, list) {
            list_del(&entry->list);
            kfree(entry);
        }
        spin_unlock(&dev->responses_lock);
    }
    dev->mem_addr = dev->dsp->reg_dsp_base_phy_addr_dram;//dev->dsp->dma_buf_addr
    dev->mem_size = DSP_DRAM_SIZE;//dev->dsp->dma_buf_size
    dev->num_dma_ch = 1;
    info.fw_ver = 100;
#if 0//use dma_buf	
    info.mem_addr = dev->dsp->dma_buf_addr;
    info.mem_size = dev->dsp->dma_buf_size;
#else//use user defined dram address	
    info.mem_addr = dev->dsp->reg_dsp_base_phy_addr_dram;
    info.mem_size = DSP_BUFFER_UNIT_SIZE*DSP_BUFFER_MAX_NUM;
#endif	
    info.num_dma_ch = 1;
    pr_debug("%d: %s: [%llx, %llx], %d\n", num, __func__,
        info.mem_addr, info.mem_size,
        info.num_dma_ch);
    ret = dev->dsp->prepare_inference(dev->dsp);
    if (msg->data!=NULL)
    {
        if (copy_to_user((void __user*)msg->data, &info, sizeof(info))) {
            pr_debug("%d: %s failed.\n", num, __func__);
            return -EFAULT;
        }
    }
    return ret;        
}

/**
 * dxrt_schedule - Send scheduler datas to the dxrt device
 * @dev: The deepx device on kernel structure
 * @msg: User-space pointer including the data buffer
 *
 * This function copies it to the user-space buffer provided by the ioctl command.
 *
 * Return: 0 on success,
 *        -EFAULT   if an error occurs during the copy(user <-> kernel)
 *        -ENOMEM   if an error occurs as memory allocation fail
 *                  This return is only for sub command of 'DX_SCHED_ADD'
 *        -EBUSY    There are no queues to assign to the user.
 *                  User scheduler options need modification.
 *                  This return is only for sub command of 'DX_SCHED_ADD'
 *        -ENOENT   There are no matching queues in the list.
 *                  This return is only for sub command of 'DX_SCHED_DELETE'
 *        -EINVAL   if an error occurs as sub-command is not supported
 *        -ETIMEDOUT if an error occurs during waiting from response of deepx device
 */
static int dxrt_schedule(struct dxdev* dev, dxrt_message_t *msg)
{
    int ret = 0;//, num = dev->id;
    
    return ret;
}

/**
 * dxrt_write_mem - Write data to the dxrt device
 * @dev: The deepx device on kernel structure
 * @msg: User-space pointer including the data buffer
 *
 * This function copies it to the user-space buffer provided by the ioctl command.
 *
 * Return: 0 on success,
 *        -EFAULT    if an error occurs during the copy(user <-> kernel)
 *        -ECOMM     if an error occurs because of pcie data transaction fail
 */
static int dxrt_write_mem(struct dxdev* dev, dxrt_message_t *msg)
{
    int ret = 0, num = dev->id;
    uint32_t ch;
    dxrt_req_meminfo_t meminfo;
    pr_debug("%d: %s\n", num, __func__);
    if (msg->data!=NULL)
    {
        if (copy_from_user(&meminfo, (void __user*)msg->data, sizeof(dxrt_req_meminfo_t))) {
            pr_debug("%d: %s: failed.\n", num, __func__);
            return -EFAULT;
        }
        ch = meminfo.ch;
        pr_debug( MODULE_NAME "%d:%d %s: [%llx, %llx + %x(%x)]\n",
            num, ch,
            __func__,
            meminfo.data,
            meminfo.base,
            meminfo.offset,
            meminfo.size
        );
        if (ch>2) {
            pr_err( MODULE_NAME "%d: %s: invalid channel.\n", num, __func__);
            return -EINVAL;
        }
        if ( meminfo.base + meminfo.offset < dev->mem_addr ||
            meminfo.base + meminfo.offset + meminfo.size > dev->mem_addr + dev->mem_size )
        {
            pr_debug("%d: %s: invalid address: %llx + %x (%llx + %llx)\n", num, __func__, 
                meminfo.base, meminfo.offset, dev->mem_addr, dev->mem_size);
            return -EINVAL;
        }
                
        if (copy_from_user(dev->dsp->dma_buf + meminfo.offset, (void __user*)meminfo.data, meminfo.size)) {
            pr_debug("%d: %s: failed.\n", num, __func__);
            return -EFAULT;
        }
        //sbi_l2cache_flush(meminfo.base + meminfo.offset, meminfo.size);
        ret = 0;                
    }
    return ret;
}

/**
 * dxrt_write_input 
 *  - Write input data to the dxrt device and model meta-datas insert queue
 * @dev: The deepx device on kernel structure
 * @msg: User-space pointer including the data buffer
 *
 * This function copies user datas to memory of deepx device by the ioctl command.
 * Model meta datas insert to queue via pcie if user datas copy to device successfully.
 *
 * Return: 0 on success,
 *        -EFAULT   if an error occurs during the copy(user <-> kernel)
 *        -EBUSY    if an error occurs inserting queue as the queue is full (retry) 
 *        -EINVAL   if an error occurs inserting queue as the queue is disable
 *                  if an error occurs as interrupt handshake is fail with device
 *                  if an error occurs as sub-command is not supported
 *        -ECOMM    if an error occurs because of pcie data transaction fail
 *        -ENOENT   There are no matching queues in the list.
 */
static int dxrt_write_input(struct dxdev* dev, dxrt_message_t *msg)
{
    int ret = 0, num = dev->id;
    pr_debug("%d: %s\n", num, __func__);
    
    dxrt_request_t req;
    if (msg->data!=NULL) {
        if (copy_from_user(&req, (void __user*)msg->data, sizeof(req))) {
            pr_debug("%d: %s: failed.\n", num, __func__);
            return -EFAULT;
        }
        pr_debug( MODULE_NAME "%d: %s: req %d\n", 
            num, __func__, req.req_id
        );
        dev->dsp->run(dev->dsp, &req);
    }        
    return ret;
    
}

/**
 * dxrt_dsp_run_request 
 *  - Write model meta-datas insert queue
 * @dev: The deepx device on kernel structure
 * @msg: User-space pointer including the data buffer
 *
 * This function copies user datas to memory of deepx device by the ioctl command.
 * Model meta datas insert to queue via pcie if user datas copy to device successfully.
 *
 * Return: 0 on success,
 *        -EFAULT   if an error occurs during the copy(user <-> kernel)
 *        -EBUSY    if an error occurs inserting queue as the queue is full (retry) 
 *        -EINVAL   if an error occurs inserting queue as the queue is disable
 *                  if an error occurs as interrupt handshake is fail with device
 *                  if an error occurs as sub-command is not supported
 *        -ENOENT   There are no matching queues in the list.
 */
static int dxrt_dsp_run_request(struct dxdev* dev, dxrt_message_t *msg)
{
    int ret = -1, num = dev->id;
    pr_debug("%d: %s\n", num, __func__);
        
    /* TODO */
    return ret;
    
}


/**
 * dxrt_dsp_run_response 
 *  - Pop device response data from queue
 * @dev: The deepx device on kernel structure
 * @msg: User-space pointer including the data buffer
 *
 * This function copies data on deepx device memory to user buffer by the ioctl command.
 * If there is data in the internal response queue,
 * 
 * Return: 0 on success,
 *        -EFAULT   if an error occurs during the copy(user <-> kernel)
 *        -ENODATA  if an error occurs inserting queue as the queue is full (retry)
 *        -EINVAL   if an error occurs because the pcie dma channel is not supported
 */
static int dxrt_dsp_run_response(struct dxdev* dev, dxrt_message_t *msg)
{
    int num = dev->id;
    pr_debug("%d: %s\n", num, __func__);
    
    int ret = -1;
    /* TODO */
    return ret;    
}

/**
 * dxrt_read_output 
 *  - Read output data from the dxrt device and pop device response data from queue
 * @dev: The deepx device on kernel structure
 * @msg: User-space pointer including the data buffer
 *
 * This function copies data on deepx device memory to user buffer by the ioctl command.
 * If there is data in the internal response queue,
 * the data is read from the deepx device immediately.
 * Otherwise, it waits until the queue is not empty.
 * 
 * Return: 0 on success,
 *        -EFAULT   if an error occurs during the copy(user <-> kernel)
 *        -ENODATA  if an error occurs inserting queue as the queue is full (retry)
 *        -EINVAL   if an error occurs because the pcie dma channel is not supported
 *        -ECOMM    if an error occurs because of pcie data transaction fail
 */
static int dxrt_read_output(struct dxdev* dev, dxrt_message_t* msg)
{
    int num = dev->id;
    pr_debug("%d: %s\n", num, __func__);
    
    dxdsp_t *dsp = dev->dsp;
    
    int ret = -1;
    unsigned long flags;
    if (msg->data!=NULL) {
        if (list_empty(&dev->responses.list)) {
            pr_debug(MODULE_NAME "%d: %s: start to wait.\n", num, __func__);
            ret = wait_event_interruptible(dsp->irq_wq, dsp->irq_event==1);
            pr_debug(MODULE_NAME "%d: %s: wake up.\n", num, __func__);
            spin_lock_irqsave(&dsp->irq_event_lock, flags);
            dsp->irq_event = 0;
            spin_unlock_irqrestore(&dsp->irq_event_lock, flags);
        }

        spin_lock_irqsave(&dev->responses_lock, flags);
        if (!list_empty(&dev->responses.list)) {
            dxrt_response_list_t *entry = list_first_entry(&dev->responses.list, dxrt_response_list_t, list);
            pr_debug(MODULE_NAME "%d: %s: %d\n", num, __func__, entry->response.req_id);
            if (copy_to_user((void __user*)msg->data, &entry->response, sizeof(dxrt_response_t))) {
                pr_err( MODULE_NAME "%d: %s: memcpy failed.\n", num, __func__);
                return -EFAULT;
            }
            list_del(&entry->list);
            kfree(entry);
            ret = 0;
        } else {
            pr_debug(MODULE_NAME "%d: %s: empty\n", num, __func__);
            ret = -1;
        }
        spin_unlock_irqrestore(&dev->responses_lock, flags);

    }
    return ret;    
}

/**
 * dxrt_terminate - Notifies the device to terminate.
 * @dev: The deepx device on kernel structure
 * @msg: User-space pointer including the data buffer
 *
 * If the user wants to terminate normally,
 * the corresponding API is called and the driver is notified of termination.
 * 
 * Return: 0 on success,
 *        Currently no other return values ​​are defined. 
 */
static int dxrt_terminate(struct dxdev* dev, dxrt_message_t* msg)
{
    int num = dev->id;
    pr_debug(MODULE_NAME "%d: %s\n", num, __func__);
    
    dxdsp_t *dsp = dev->dsp;
    unsigned long flags;
    pr_debug(MODULE_NAME "%d: %s start \n", num, __func__);
    spin_lock_irqsave(&dsp->irq_event_lock, flags);

    dsp->irq_event = 1;
    wake_up_interruptible(&dsp->irq_wq);
    spin_unlock_irqrestore(&dsp->irq_event_lock, flags);
    {
        spin_lock_irqsave(&dev->error_lock, flags);
        dev->error = 99;
        wake_up_interruptible(&dev->error_wq);
        spin_unlock_irqrestore(&dev->error_lock, flags);
    }
    pr_debug(MODULE_NAME "%d: %s done.\n", num, __func__);
    return 0;    
}

/**
 * dxrt_read_mem - Read data from the dxrt device
 * @dev: The deepx device on kernel structure
 * @msg: User-space pointer including the data buffer
 *
 * This function copies data on deepx device memory to user buffer by the ioctl command
 *
 * Return: 0 on success,
 *        -EFAULT    if an error occurs during the copy(user <-> kernel)
 *        -EINVAL    if an error occurs because of invalid address from user
 *        -ECOMM     if an error occurs because of pcie data transaction fail
 */
static int dxrt_read_mem(struct dxdev* dev, dxrt_message_t* msg)
{
    int num = dev->id;
    uint32_t ch;
    dxrt_req_meminfo_t meminfo;
    pr_debug("%d: %s\n", num, __func__);
    if (msg->data!=NULL) {
        if (copy_from_user(&meminfo, (void __user*)msg->data, sizeof(meminfo))) {
            pr_debug("%d: %s: failed.\n", num, __func__);
            return -EFAULT;
        }
        ch = meminfo.ch;
        pr_debug( MODULE_NAME "%d:%d %s: [%llx, %llx + %x(%x)]\n",
            num, ch,
            __func__,
            meminfo.data,
            meminfo.base,
            meminfo.offset,
            meminfo.size
        );
        if (ch>2) {
            pr_err( MODULE_NAME "%d: %s: invalid channel.\n", num, __func__);
            return -EINVAL;
        }
        if ( meminfo.base + meminfo.offset < dev->mem_addr ||
            meminfo.base + meminfo.offset + meminfo.size > dev->mem_addr + dev->mem_size )
        {
            pr_debug("%d: %s: invalid address: %llx + %x\n", num, __func__, meminfo.base, meminfo.offset);
            return -EINVAL;
        }
        
        if (copy_to_user((void __user*)meminfo.data, dev->dsp->dma_buf + meminfo.offset,  meminfo.size)) {
            pr_debug("%d: %s: failed.\n", num, __func__);
            return -EFAULT;
        }        
    }
    return 0;
}

/**
 * dxrt_cpu_cache_flush - Execute cpu cache flush
 * @dev: The deepx device on kernel structure
 * @msg: User-space pointer including the data buffer
 *
 * This function is executed by the ioctl command
 * If you feel that copying data between master devices
 * causes a data inconsistency problem recognized by the CPU,
 * you can call the corresponding API.
 * 
 * Return: 0 on success,
 *        -EFAULT    if an error occurs during the copy(user <-> kernel)
 *        -EINVAL    if an error occurs because of invalid address from user
 */
static int dxrt_cpu_cache_flush(struct dxdev* dev, dxrt_message_t* msg)
{
    int num = dev->id;
    dxrt_meminfo_t meminfo;
    pr_debug("%d: %s: %llx\n", num, __func__, (uint64_t)msg->data);
    if (msg->data!=NULL) {
        if (copy_from_user(&meminfo, (void __user*)msg->data, sizeof(meminfo))) {
            pr_debug("%d: %s: failed.\n", num, __func__);
            return -EFAULT;
        }
        pr_debug(MODULE_NAME "%d: %s: [%llx, %llx + %x(%x)]\n",
            num,
            __func__,
            meminfo.data,
            meminfo.base,
            meminfo.offset,
            meminfo.size
        );
        if ( meminfo.base + meminfo.offset < dev->mem_addr ||            
            meminfo.base + meminfo.offset + meminfo.size > dev->mem_addr + dev->mem_size ) {
            pr_debug("%d: %s: invalid address: %llx + %x\n", num, __func__, meminfo.base, meminfo.offset);
            return -EINVAL;
        }

        //flush_dcache_range(meminfo.data + meminfo.offset, meminfo.data + meminfo.offset + meminfo.size); 
		//dcache_clean_poc(meminfo.data + meminfo.offset, meminfo.data + meminfo.offset + meminfo.size);
		caches_clean_inval_pou(meminfo.data + meminfo.offset, meminfo.data + meminfo.offset + meminfo.size);//virtual address		
    }
    return 0;
}
static int dxrt_soc_custom(struct dxdev* dev, dxrt_message_t* msg)
{
    int ret = 0, num = dev->id;
    pr_info("%d: %s: %llx\n", num, __func__, (uint64_t)msg->data);
    
    return ret;
}
static int dxrt_get_log(struct dxdev* dev, dxrt_message_t* msg)
{
    int ret = 0;//, num = dev->id;
    pr_debug("%s: %d, %d: %llx\n", __func__, dev->id, dev->type, (uint64_t)msg->data);

    return ret;
}

/**
 * dxrt_reset_device - Reset device
 * @dev: The deepx device on kernel structure
 * @msg: User-space pointer including the data buffer
 *
 * This function is executed by the ioctl command
 * User can reset device by using this function.
 *  [reset level]
 *    0 : DSP IP
 *    1 : entire device
 * 
 * Return: 0 on success,
 *        -EFAULT    if an error occurs during the copy(user <-> kernel)
 *        -ENOMEM    if an error occurs during memory allocation on kernel space
*/
static int dxrt_reset_device(struct dxdev* dev, dxrt_message_t* msg)
{
    int ret;    
	struct dxdsp *dsp = dev->dsp;      
    pr_info("%s\n", __func__);
    ret = dx_v3_dsp_reset_and_start(dsp);    
    return ret;
}

/**
 * dxrt_handle_rt_drv_info_sub - Get device driver version
 * @dev: The deepx device on kernel structure
 * @msg: User-space pointer including the data buffer
 *
 * This function is executed by the ioctl command
 * 
 * Return: 0 on success,
 *        -EFAULT    if an error occurs during the copy(user <-> kernel)
 *        -EINVAL    if an error occurs because of unsupported command from user
*/
static int dxrt_handle_rt_drv_info_sub(struct dxdev* dev, dxrt_message_t* msg)
{
    int num = dev->id;
    int ret = 0;
    pr_debug(MODULE_NAME "%d: %s, [%d]\n", num, __func__, msg->sub_cmd);
    switch (msg->sub_cmd) {
        case DRVINFO_CMD_GET_RT_INFO:
            if (msg->data!=NULL) {
                struct dxrt_drv_info info;
                info.driver_version = DXRT_MOD_VERSION_NUMBER;
                if (copy_to_user((void __user*)msg->data, &info, sizeof(info))) {
                    pr_err("%d: %s cmd:%d failed.\n", num, __func__, msg->sub_cmd);
                    ret = -EFAULT;
                }
            }
            break;
        case DRVINFO_CMD_GET_PCIE_INFO:
            {
                pr_err("CMD(%d) is not supported for device type(%d)\n", msg->cmd, dev->type);
            }
            break;
    default:
        pr_err("Unsupported sub command(%d/%d)\n", msg->cmd, msg->sub_cmd);
        ret = -EINVAL;
        break;
    }
    return ret;
}

/**
 * dxrt_recovery_device - Driver and firmware recovery in unusual situations
 * @dev: The deepx device on kernel
 * @msg: User-space pointer including the data buffer
 *
 * This function copies the user-space datas to deepx device provided by the ioctl command.
 * Also, this function is general interface to communicate with the deepx device.
 * 
 * Return: 0 on success,
 *        -EFAULT    if an error occurs during the copy(user <-> kernel).
 *        -ETIMEDOUT if an error occurs during waiting from response of deepx device
 *        -ENOMEM    if an error occurs during memory allocation on kernel space
 */
static int dxrt_recovery_device(struct dxdev* dev, dxrt_message_t* msg)
{
    int ret;    
	struct dxdsp *dsp = dev->dsp;      
    pr_info("%s\n", __func__);
    ret = dx_v3_dsp_reset_and_start(dsp);    
    return ret;
}

static int dxrt_handle_drv_info(struct dxdev* dev, dxrt_message_t* msg)
{
    return dxrt_handle_rt_drv_info_sub(dev, msg);
}

static int dxrt_alloc_buf(struct dxdev* dev, dxrt_message_t* msg)
{
    int num = dev->id;
    dxrt_dsp_buffer_metadata_t dsp_buf_meta;
    int i;
    int found = -1;
    unsigned long flags;

    pr_debug("%d: %s\n", num, __func__);
    
    if (msg->data == NULL) {
        pr_err("%d: %s: data is NULL\n", num, __func__);
        return -EINVAL;
    }

    // Lock the memory manager for thread-safe access
    spin_lock_irqsave(&dev->dsp->irq_event_lock, flags);

    // Find a free buffer slot
    for (i = 0; i < DSP_BUFFER_MAX_NUM; i++) {
        if (g_dsp_memory_manager.buffers[i].alloc_size == 0) {
            found = i;
            break;
        }
    }

    if (found == -1) {
        spin_unlock_irqrestore(&dev->dsp->irq_event_lock, flags);
        pr_err("%d: %s: No free buffer available\n", num, __func__);
        return -ENOMEM;
    }

    // Allocate the buffer
    g_dsp_memory_manager.buffers[found].dsp_buf_offset = found * DSP_BUFFER_UNIT_SIZE;
    g_dsp_memory_manager.buffers[found].alloc_size = DSP_BUFFER_UNIT_SIZE;

    // Prepare buffer info to return to user
    dsp_buf_meta.dsp_buf_offset = g_dsp_memory_manager.buffers[found].dsp_buf_offset;
    dsp_buf_meta.alloc_size = g_dsp_memory_manager.buffers[found].alloc_size;

    spin_unlock_irqrestore(&dev->dsp->irq_event_lock, flags);

    // Copy the allocated buffer metadata back to user space
    if (copy_to_user((void __user*)msg->data, &dsp_buf_meta, sizeof(dsp_buf_meta))) {
        pr_err("%d: %s: copy_to_user failed.\n", num, __func__);
        // Rollback the allocation
        spin_lock_irqsave(&dev->dsp->irq_event_lock, flags);
        g_dsp_memory_manager.buffers[found].alloc_size = 0;
        spin_unlock_irqrestore(&dev->dsp->irq_event_lock, flags);
        return -EFAULT;
    }

    pr_debug("%d: %s: Allocated buffer %d at offset 0x%x, size 0x%x\n", 
             num, __func__, found, dsp_buf_meta.dsp_buf_offset, dsp_buf_meta.alloc_size);

    return 0;
}

static int dxrt_free_buf(struct dxdev* dev, dxrt_message_t* msg)
{
    int num = dev->id;
    dxrt_dsp_buffer_metadata_t dsp_buf_meta;
    unsigned long flags;
    int i;
    int found = -1;

    pr_debug("%d: %s\n", num, __func__);
    
    if (msg->data == NULL) {
        pr_err("%d: %s: data is NULL\n", num, __func__);
        return -EINVAL;
    }

    if (copy_from_user(&dsp_buf_meta, (void __user*)msg->data, sizeof(dsp_buf_meta))) {
        pr_err("%d: %s: copy_from_user failed.\n", num, __func__);
        return -EFAULT;
    }

    // Lock the memory manager for thread-safe access
    spin_lock_irqsave(&dev->dsp->irq_event_lock, flags);

    // Find the buffer with matching offset
    for (i = 0; i < DSP_BUFFER_MAX_NUM; i++) {
        if ((g_dsp_memory_manager.buffers[i].alloc_size != 0) &
            (g_dsp_memory_manager.buffers[i].dsp_buf_offset == dsp_buf_meta.dsp_buf_offset)) {
            found = i;
            break;
        }
    }

    if (found == -1) {
        spin_unlock_irqrestore(&dev->dsp->irq_event_lock, flags);
        pr_err("%d: %s: Buffer with offset 0x%x not found or already freed\n", 
               num, __func__, dsp_buf_meta.dsp_buf_offset);               
        return -EINVAL;
    }

    // Free the buffer by setting alloc_size to 0
    g_dsp_memory_manager.buffers[found].alloc_size = 0;

    spin_unlock_irqrestore(&dev->dsp->irq_event_lock, flags);

    pr_debug("%d: %s: Freed buffer %d at offset 0x%x\n", 
             num, __func__, found, dsp_buf_meta.dsp_buf_offset);

    return 0;
}

int message_handler_general(struct dxdev *dx, dxrt_message_t *msg)
{
    return message_handler[msg->cmd](dx, msg);
}

dxrt_message_handler message_handler[] = {
    [DXRT_CMD_IDENTIFY_DEVICE]      = dxrt_identify_device,
    [DXRT_CMD_WRITE_MEM]            = dxrt_write_mem,
    [DXRT_CMD_READ_MEM]             = dxrt_read_mem,
    [DXRT_CMD_CPU_CACHE_FLUSH]      = dxrt_cpu_cache_flush,
    [DXRT_CMD_WRITE_INPUT_DMA_CH0]  = dxrt_write_input,
    [DXRT_CMD_WRITE_INPUT_DMA_CH1]  = dxrt_write_input,
    [DXRT_CMD_WRITE_INPUT_DMA_CH2]  = dxrt_write_input,
    [DXRT_CMD_READ_OUTPUT_DMA_CH0]  = dxrt_read_output,
    [DXRT_CMD_READ_OUTPUT_DMA_CH1]  = dxrt_read_output,
    [DXRT_CMD_READ_OUTPUT_DMA_CH2]  = dxrt_read_output,
    [DXRT_CMD_TERMINATE]            = dxrt_terminate,
    [DXRT_CMD_SOC_CUSTOM]           = dxrt_soc_custom,
    [DXRT_CMD_GET_STATUS]           = dxrt_msg_general,
    [DXRT_CMD_RESET]                = dxrt_reset_device,
    [DXRT_CMD_UPDATE_CONFIG]        = dxrt_msg_general,
    [DXRT_CMD_UPDATE_CONFIG_JSON]   = dxrt_msg_general,
    //[DXRT_CMD_UPDATE_FIRMWARE]      = dxrt_update_firmware,
    [DXRT_CMD_GET_LOG]              = dxrt_get_log,
    [DXRT_CMD_DUMP]                 = dxrt_msg_general,
    //[DXRT_CMD_EVENT]                = dxrt_handle_event,
    [DXRT_CMD_DRV_INFO]             = dxrt_handle_drv_info,
    [DXRT_CMD_SCHEDULE]             = dxrt_schedule,
    //[DXRT_CMD_UPLOAD_FIRMWARE]      = dxrt_upload_firmware,
    [DXRT_CMD_DSP_RUN_REQ]          = dxrt_dsp_run_request,
    [DXRT_CMD_DSP_RUN_RESP]         = dxrt_dsp_run_response,
    [DXRT_CMD_RECOVERY]             = dxrt_recovery_device,
    [DXRT_CMD_SET_DDR_FREQ]         = dxrt_msg_general,
    [DXRT_CMD_ALLOC_DSP_BUF]        = dxrt_alloc_buf,
    [DXRT_CMD_FREE_DSP_BUF]         = dxrt_free_buf,
};