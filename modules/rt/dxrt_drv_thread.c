// SPDX-License-Identifier: GPL-2.0
/*
 * Deepx Runtime Driver
 *
 * Copyright (C) 2023 Deepx, Inc.
 *
 */
#include <linux/io.h>
#include <linux/delay.h>
#include "dxrt_drv.h"

int dxrt_is_request_list_empty(dxrt_request_list_t *requests, spinlock_t *lock)
{
    int empty;
    spin_lock(lock);
    empty = list_empty(&requests->list);
    spin_unlock(lock);
    return empty;
}

int dxrt_request_handler(void *data)
{
	struct dxdev *dx = (struct dxdev*)data;
	struct dxdsp *dsp = dx->dsp;
    int num = dx->id;
    dxrt_request_list_t *entry;
    // dxrt_dsp_request_t *req;
    pr_debug( MODULE_NAME "%d: %s start.\n", num, __func__);
    while(!kthread_should_stop())
    {
        wait_event_interruptible(
            dx->request_wq,
            !dxrt_is_request_list_empty(&dx->requests, &dx->requests_lock) || kthread_should_stop()            
        );
        if(kthread_should_stop()) break;
        pr_debug( MODULE_NAME "%d: %s wake up.\n", num, __func__);        
        spin_lock(&dx->requests_lock);
        entry = list_first_entry(&dx->requests.list, dxrt_request_list_t, list);            
            // req = &entry->request;
            // pr_debug( MODULE_NAME "%d: %s: req %d\n", 
            //     num, __func__, req->req_id
            // );
        spin_unlock(&dx->requests_lock);
        dsp->run(dsp, &entry->request);
        spin_lock(&dx->requests_lock);
        list_del(&entry->list);
        spin_unlock(&dx->requests_lock);
        kfree(entry);
    }
    pr_debug( MODULE_NAME "%d: %s end.\n", num, __func__);
    return 0;
}