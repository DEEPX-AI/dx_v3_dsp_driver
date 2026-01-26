// SPDX-License-Identifier: GPL-2.0
/*
 * Deepx Runtime Driver
 *
 * Copyright (C) 2023 Deepx, Inc.
 *
 */
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/cdev.h>
//#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>

#if DEVICE_TYPE==1
/* L2 cache flush api */
//#include <asm/sbi.h>
#endif

#include "dxrt_drv.h"

static const u64 dmamask = DMA_BIT_MASK(32);

static int dxrt_dev_open(struct inode *i, struct file *f)
{
    struct dxdev *dx;
    //int num = iminor(f->f_inode);
    pr_debug( "%s: %s\n", f->f_path.dentry->d_iname, __func__);
    dx = container_of(i->i_cdev, struct dxdev, cdev);
    f->private_data = dx;

    dx->response.req_id = 0;
    dx->dsp->irq_event = 0;
    return 0;
}
static int dxrt_dev_release(struct inode *i, struct file *f)
{    
    pr_debug( "%s: %s\n", f->f_path.dentry->d_iname, __func__);

    return 0;
}
static ssize_t dxrt_dev_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{
    struct dxdev *dx = f->private_data;
    pr_debug( "%s: %s\n", f->f_path.dentry->d_iname, __func__);

    if (len < sizeof(dxrt_response_t)) {
        pr_err( "%s: buffer too small: %lu < %lu\n", 
                f->f_path.dentry->d_iname, len, sizeof(dxrt_response_t));
        return -EINVAL;
    }

    if(copy_to_user(buf, &dx->response, sizeof(dxrt_response_t))) {
        pr_err( "%s: failed to copy response\n", f->f_path.dentry->d_iname);
        return -EFAULT;
    }

    return sizeof(dxrt_response_t);
}
static ssize_t dxrt_dev_write(struct file *f, const char __user *buf, size_t len, loff_t *off)
{
    // unsigned long flags;
    struct dxdev *dx = f->private_data;
    pr_debug( "%s: %s\n", f->f_path.dentry->d_iname, __func__);
    if(dx->request_handler)
    {
        dxrt_request_list_t *entry;
        entry = kmalloc(sizeof(dxrt_request_list_t), GFP_KERNEL);
        if(!entry)
        {
            printk(KERN_ALERT "Failed to allocate memory for request queue entry\n");
            return -ENOMEM;
        }    
        if (len != sizeof(dxrt_dsp_request_t)) {
            printk(KERN_ALERT "Invalid request size: %lu\n", len);
            kfree(entry);
            return -EINVAL;
        }
        if (copy_from_user(&(entry->request), buf, len)) {
            printk(KERN_ALERT "Failed to copy request data from user space\n");
            kfree(entry);
            return -EFAULT;
        }        
        spin_lock(&dx->requests_lock);
        list_add_tail(&entry->list, &dx->requests.list);
        spin_unlock(&dx->requests_lock);
        // {
        //     dxrt_dsp_request_t *req = &entry->request;
        //     pr_debug( MODULE_NAME "%d: %s: req %d\n", 
        //         num, __func__, req->req_id);
        // }
        wake_up_interruptible(&dx->request_wq);
        return len;
    }
    return 0;
}
static int dxrt_dev_mmap(struct file *f, struct vm_area_struct *vma)
{
    int ret = -1;
    struct dxdev *dx = f->private_data;
    pr_debug( "%s: %s\n", f->f_path.dentry->d_iname, __func__);
    
    struct dxdsp *dsp = dx->dsp;
    unsigned long size = vma->vm_end - vma->vm_start;
    
    if (vma->vm_pgoff == 0)// Memory mapping for DRAM
    {        
        unsigned long pfn = dsp->reg_dsp_base_phy_addr_dram >> PAGE_SHIFT;        
#if 1//use non-cached area
        vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
#endif        
        ret = remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot);    
    }
    else if (vma->vm_pgoff == 1)// Memory mapping for SRAM
    {        
        unsigned long pfn = dsp->reg_dsp_base_phy_addr_sram >> PAGE_SHIFT;
        vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
        ret = remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot);
    }
	else if (vma->vm_pgoff == 2)// Memory mapping for DMA buffer
	{        
        unsigned long size = vma->vm_end - vma->vm_start;
        ret = dma_mmap_coherent(dx->dev, vma, dsp->dma_buf, dsp->dma_buf_addr, size);
        pr_debug( "%s: mmap 0x%lx bytes, returned %d\n", f->f_path.dentry->d_iname, size, ret);        
    }
    else
    {
        pr_debug( "not supported vm_pgoff %ld : %s\n", vma->vm_pgoff, __func__);
    }

	pr_debug( "%s dram phy_addr = %x \n", __func__, dsp->reg_dsp_base_phy_addr_dram);
    if (ret == 0) {
        pr_debug( "%s: mmap 0x%lx bytes succeeded\n", f->f_path.dentry->d_iname, size);
    } else {
        pr_err( "%s: mmap 0x%lx bytes failed with %d\n", f->f_path.dentry->d_iname, size, ret);
    }
    
    return ret;
}
static unsigned int dxrt_dev_poll(struct file *f, poll_table *wait)
{
    struct dxdev *dx = f->private_data;
    
    unsigned int mask = 0;
    unsigned long flags;
    pr_debug( "%s: %s\n", f->f_path.dentry->d_iname, __func__);
        
    struct dxdsp *dsp = dx->dsp;
    poll_wait(f, &dsp->irq_wq, wait);
    spin_lock_irqsave(&dsp->irq_event_lock, flags);
    if(dsp->irq_event)
    {
        mask = POLLIN | POLLRDNORM;
        dsp->irq_event = 0;
    }
    spin_unlock_irqrestore(&dsp->irq_event_lock, flags);
    return mask;
}

/**
 * dxrt_dev_ioctl - Handle ioctl commands for the deepx device
 * @f: Pointer to the file structure
 * @cmd: The ioctl command
 * @arg: The argument for the ioctl command
 *
 * This function handles various ioctl commands for the deepx device.
 * It performs different actions based on the command received.
 *
 * Supported commands:
 *   - DXRT_IOCTL_MESSAGE: Get device information
 *
 * Return: 0 on success,
 *        -EINVAL if the command is invalid,
 *        or appropriate error code for specific commands.
 */
static long dxrt_dev_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    int num = iminor(f->f_inode);
    struct dxdev *dx = f->private_data;
    dxrt_message_t msg;
    pr_debug( "%s: ioctl() cmd %d\n", f->f_path.dentry->d_iname, cmd);    
    if (_IOC_TYPE(cmd) != DXRT_IOCTL_MAGIC || \
        _IOC_NR(cmd) >= DXRT_IOCTL_MAX)
    {
        pr_debug( "%s: ioctl() invalid cmd %d\n", f->f_path.dentry->d_iname, cmd);
        return -EINVAL;
    }
    switch(cmd) {
        case DXRT_IOCTL_MESSAGE:
            if (copy_from_user(&msg, (void __user*)arg, sizeof(msg)))
            {
                pr_debug( MODULE_NAME "%d: ioctl cmd 0x%x failed.\n", num, cmd);
                return -EFAULT;
            }
            if(msg.cmd >=0 && msg.cmd < DXRT_CMD_MAX)
            {
                pr_debug( MODULE_NAME "%d: message %d\n", num, msg.cmd);

                return message_handler_general(dx, &msg);
                // return message_handler[msg.cmd](dx, msg.data);
            }
            else
            {
                pr_debug( MODULE_NAME "%d: invalid message %d\n", num, msg.cmd);
                return -EINVAL;
            }
            break;
        default:
            pr_debug( MODULE_NAME "%d: ioctl cmd 0x%x: unknown command.\n", num, cmd);
            break;
    }
    return 0;
}

static struct file_operations dxrt_cdev_fops =
{
    .owner = THIS_MODULE,
    .open = dxrt_dev_open,
    .release = dxrt_dev_release,
    .read = dxrt_dev_read,
    .write = dxrt_dev_write,
    .mmap = dxrt_dev_mmap,
    .unlocked_ioctl = dxrt_dev_ioctl,
    .poll = dxrt_dev_poll
};

static struct dxdev* create_dxrt_device(int id, struct dxrt_driver *drv, struct file_operations *fops)
{
    int ret;
    struct dxdev* dxdev = kmalloc(sizeof(struct dxdev), GFP_KERNEL);
    if(!dxdev)
    {
        pr_err( "%s: failed to allocate memory\n", __func__);
        return NULL;
    }
    memset(dxdev, 0, sizeof(struct dxdev));

    dxdev->pdev = drv->pdev;
    dxdev->id = id;
    dxdev->type = DEVICE_TYPE;
    dxdev->variant = DEVICE_VARIANT;
    cdev_init(&dxdev->cdev, fops);
    dxdev->cdev.owner = THIS_MODULE;
    if ((ret = cdev_add(&dxdev->cdev, drv->dev_num + id, 1)) < 0)
    {
        pr_err( "%s: failed to add character device\n", __func__);
        kfree(dxdev);
        device_destroy(drv->dev_class, drv->dev_num);
        class_destroy(drv->dev_class);
        unregister_chrdev_region(drv->dev_num, drv->num_devices);
        return NULL;
    }

    if (IS_ERR(dxdev->dev = device_create(drv->dev_class, NULL, drv->dev_num + id, NULL, MODULE_NAME"%d", id)))
    {
        pr_err( "%s: failed to create device\n", __func__);
        cdev_del(&dxdev->cdev);
        kfree(dxdev);
        class_destroy(drv->dev_class);
        unregister_chrdev_region(drv->dev_num, drv->num_devices);
        return NULL;
    }
    dxdev->dev->dma_mask = (u64 *)&dmamask;
    dxdev->dev->coherent_dma_mask = DMA_BIT_MASK(32);
    dxdev->dsp = dxrt_dsp_init(dxdev);
    INIT_LIST_HEAD(&dxdev->requests.list);
    INIT_LIST_HEAD(&dxdev->responses.list);
    
    init_waitqueue_head(&dxdev->request_wq);
    init_waitqueue_head(&dxdev->error_wq);
        
    spin_lock_init(&dxdev->requests_lock);
    spin_lock_init(&dxdev->responses_lock);
    spin_lock_init(&dxdev->error_lock);
    mutex_init(&dxdev->msg_lock);
    
    dxdev->request_handler = kthread_run(
        dxrt_request_handler, (void*)dxdev, "dxrt-th%d", dxdev->id
    );
    
    pr_info(" [%d] created device %d:%d:%d, %p, %p\n",
        dxdev->variant, id,
        MAJOR(drv->dev_num + id), MINOR(drv->dev_num + id), dxdev->dev, dxdev->dsp);
    return dxdev;
}
static void remove_dxrt_device(struct dxrt_driver *drv, struct dxdev* dxdev)
{
    dxrt_dsp_deinit(dxdev);
    device_destroy(drv->dev_class, drv->dev_num + dxdev->id);
    cdev_del(&dxdev->cdev);
    kfree(dxdev);    
}

int dxrt_dsp_driver_cdev_init(struct dxrt_driver *drv)
{
    int i, ret;
    pr_debug( "%s\n", __func__);

    drv->num_devices = 1;//NUM_DEVICES;

    pr_info("%s: %d devices\n", __func__, drv->num_devices);
    if ((ret = alloc_chrdev_region(&drv->dev_num, 0, drv->num_devices, MODULE_NAME)) < 0)
    {
        return ret;
    }
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0))
    if (IS_ERR(drv->dev_class = class_create(MODULE_NAME)))
#else
    if (IS_ERR(drv->dev_class = class_create(THIS_MODULE, MODULE_NAME)))
#endif
    {
        unregister_chrdev_region(drv->dev_num, drv->num_devices);
        return PTR_ERR(drv->dev_class);
    }
    for(i=0;i<drv->num_devices;i++)
    {
        drv->devices[i] = create_dxrt_device(i, drv, &dxrt_cdev_fops);
        if(drv->devices[i]==NULL)
        {
            return -1;
        }
    }
    pr_debug( "%s done.\n", __func__);
    return 0;

}
void dxrt_dsp_driver_cdev_deinit(struct dxrt_driver *drv)
{
    int i;

    struct task_struct *request_handler;
    pr_debug( "%s\n", __func__);
    for(i=0;i<drv->num_devices;i++)
    {
        request_handler = drv->devices[i]->request_handler;
        if(request_handler)
        {
            kthread_stop(request_handler);
        }

        remove_dxrt_device(drv, drv->devices[i]);
    }
    class_destroy(drv->dev_class);
    unregister_chrdev_region(drv->dev_num, drv->num_devices);
    pr_debug( "%s done.\n", __func__);
}
