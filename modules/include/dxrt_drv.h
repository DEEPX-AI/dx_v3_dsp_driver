// SPDX-License-Identifier: GPL-2.0
/*
 * Deepx Runtime Driver
 *
 * Copyright (C) 2023 Deepx, Inc.
 *
 */
#ifndef __DXRT_DRV_H
#define __DXRT_DRV_H

#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/reset.h>
#include <linux/poll.h>
#include <linux/wait.h>
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
#include <linux/platform_device.h>

#include "dxrt_drv_common.h"
#include "dxrt_drv_dsp.h"
#include "dsp_reg_DX_V3.h"

#define MODULE_NAME "dxrt_dsp"

#define DX_ACC              (0) /* accelator device */
#define DX_STD              (1) /* stand alone device */
#define DX_DEVICE_MAX_NUM   (4)

#define DSP_DRAM_SIZE 0x1EE00000
#define DSP_BUFFER_UNIT_SIZE (4096*2048*3) // 24MB = 0x0180_0000
#define DSP_BUFFER_MAX_NUM 8 // 24MB(4096*2048*3)x8 => 192MB

/**********************/
/* RT/driver sync     */

typedef enum {
    DXRT_EVENT_ERROR,
    DXRT_EVENT_NOTIFY_THROT,
    DXRT_EVENT_NUM,
} dxrt_event_t;

typedef enum _dxrt_error_t {
    ERR_NONE      = 0,
    ERR_DSP0_HANG = 1,
} dxrt_error_t;

typedef enum _dxrt_notify_throt_t {
    NTFY_NONE       = 0,
    NTFY_THROT_FREQ_DOWN,
    NTFY_THROT_FREQ_UP,
    NTFY_THROT_VOLT_DOWN,
    NTFY_THROT_VOLT_UP,
    NTFY_EMERGENCY_BLOCK,
    NTFY_EMERGENCY_RELEASE,
} dxrt_notify_throt_t;

typedef struct device_info
{
    uint32_t type; /* 0: ACC type, 1: STD type */
    uint32_t variant; /* 100: L1, 101: L2, 102: L3, 103: L4, 104: V3,
                        200: M1, 201: M1A */
    uint64_t mem_addr;
    uint64_t mem_size;
    uint32_t num_dma_ch;
    uint16_t fw_ver;                // firmware version. A.B.C (e.g. 103)
    uint16_t bd_rev;                // board revision. /10 (e.g. 1 = 0.1, 2 = 0.2)
    uint16_t bd_type;               // board type. (1 = SOM, 2 = M.2, 3 = H1)
    uint16_t ddr_freq;              // ddr frequency. (e.g. 4200, 5500)
    uint16_t ddr_type;              // ddr type. (1 = lpddr4, 2= lpddr5)
    uint16_t interface;
} dxrt_device_info_t;

typedef struct _dxrt_meminfo_t
{
    uint64_t data;
    uint64_t base;
    uint32_t offset;
    uint32_t size;
} dxrt_meminfo_t;

typedef struct _dxrt_req_meminfo_t
{
    uint64_t data;
    uint64_t base;
    uint32_t offset;
    uint32_t size;
    uint32_t ch;
} dxrt_req_meminfo_t;

typedef struct _dxrt_request_t {
    uint32_t  req_id;
    dxrt_meminfo_t input;
    dxrt_meminfo_t output;
    uint32_t  model_type;
    uint32_t  model_format;
    uint32_t  model_cmds;
    uint32_t  cmd_offset;
    uint32_t  weight_offset;
    uint32_t  last_output_offset;
} dxrt_request_t;

typedef struct _dxrt_dsp_message_header {
    unsigned short req_id;
    unsigned short message_size;
    unsigned short func_id;        
    unsigned char data_valid;        
    unsigned char reserved;
} dxrt_dsp_message_header_t;//8B

typedef struct _dxrt_dsp_request_t {
    uint32_t  req_id;                    //4B
    dxrt_dsp_message_header_t msg_header;//8B    
    uint32_t  msg_data[29];              //116B        
} dxrt_dsp_request_t;//128B

typedef struct _dxrt_response_t {
    uint32_t  req_id;
    uint32_t  inf_time;
    uint16_t  argmax;
    uint16_t  model_type;
    int32_t   status;
    uint32_t  ppu_filter_num;
    uint32_t  proc_id;
    uint32_t  queue;
    int32_t   dma_ch;
    uint32_t  ddr_wr_bw;
    uint32_t  ddr_rd_bw;
} dxrt_response_t;

typedef struct {
    unsigned int dsp_buf_offset;   // Offset from DSP memory base address
    unsigned int alloc_size; // Size of the allocated buffer (if this is 0, the buffer is free)    
} dxrt_dsp_buffer_metadata_t;

// --- DSP Memory Manager Structure (assumed to be located at the beginning of shared memory region) ---
typedef struct {
    // Semaphore ID or pointer. Used to synchronize shared memory access between CPU and DSP.
    // Can be a mutex or spinlock depending on the actual kernel environment.    
    dxrt_dsp_buffer_metadata_t buffers[DSP_BUFFER_MAX_NUM];
} dxrt_dsp_buffer_manager_t;

extern dxrt_dsp_buffer_manager_t g_dsp_memory_manager;

typedef struct
{
    int32_t     cmd;
    int32_t     sub_cmd;
    void*       data;
    uint32_t    size;
} dxrt_message_t;

typedef struct
{
    uint32_t cmd;       /* Command */
    uint32_t sub_cmd;   /* Sub command */
    uint32_t ack;       /* Response from device */
    uint32_t size;      /* Data Size */
    uint32_t data[1000];
} dxrt_device_message_t;

typedef enum {
    DXRT_CMD_IDENTIFY_DEVICE    = 0, /* Sub-command */
    DXRT_CMD_GET_STATUS         ,
    DXRT_CMD_RESET              ,
    DXRT_CMD_UPDATE_CONFIG      ,
    DXRT_CMD_UPDATE_FIRMWARE    , /* Sub-command */
    DXRT_CMD_GET_LOG            ,
    DXRT_CMD_DUMP               ,
    DXRT_CMD_WRITE_MEM          ,
    DXRT_CMD_READ_MEM           ,
    DXRT_CMD_CPU_CACHE_FLUSH    ,
    DXRT_CMD_SOC_CUSTOM         ,
    DXRT_CMD_WRITE_INPUT_DMA_CH0,
    DXRT_CMD_WRITE_INPUT_DMA_CH1,
    DXRT_CMD_WRITE_INPUT_DMA_CH2,
    DXRT_CMD_READ_OUTPUT_DMA_CH0,
    DXRT_CMD_READ_OUTPUT_DMA_CH1,
    DXRT_CMD_READ_OUTPUT_DMA_CH2,
    DXRT_CMD_TERMINATE          ,
    DXRT_CMD_EVENT              ,
    DXRT_CMD_DRV_INFO           , /* Sub-command */
    DXRT_CMD_SCHEDULE           , /* Sub-command */
    DXRT_CMD_UPLOAD_FIRMWARE    ,
    DXRT_CMD_DSP_RUN_REQ        ,
    DXRT_CMD_DSP_RUN_RESP       ,
    DXRT_CMD_UPDATE_CONFIG_JSON ,
    DXRT_CMD_RECOVERY           ,
    DXRT_CMD_SET_DDR_FREQ       ,
    DXRT_CMD_ALLOC_DSP_BUF      ,
    DXRT_CMD_FREE_DSP_BUF       ,
    DXRT_CMD_MAX,
} dxrt_cmd_t;

/* CMD : DXRT_CMD_IDENTIFY_DEVICE*/
typedef enum {
    DX_IDENTIFY_NONE        = 0,
    DX_IDENTIFY_FWUPLOAD    = 1,
} dxrt_ident_sub_cmd_t;

/* CMD : DXRT_CMD_SCHEDULE */
typedef enum {
    DX_SCHED_ADD    = 1,
    DX_SCHED_DELETE = 2
} dxrt_sche_sub_cmd_t;

/* CMD : DXRT_CMD_DRV_INFO*/
typedef enum {
    DRVINFO_CMD_GET_RT_INFO     = 0,
    DRVINFO_CMD_GET_PCIE_INFO   = 1,
} dxrt_drvinfo_sub_cmd_t;

/* CMD : DXRT_CMD_UPDATE_FIRMWARE */
typedef enum {
    FWUPDATE_ONLY        = 0,
    FWUPDATE_DEV_UNRESET = BIT(1),
    FWUPDATE_FORCE       = BIT(2),
} dxrt_fwupdate_sub_cmd_t;

#define DXRT_IOCTL_MAGIC     'D'
typedef enum {
    DXRT_IOCTL_MESSAGE = _IOW(DXRT_IOCTL_MAGIC, 0, dxrt_message_t),
    DXRT_IOCTL_DUMMY = _IOW(DXRT_IOCTL_MAGIC, 1, dxrt_message_t),
    DXRT_IOCTL_MAX
} dxrt_ioctl_t;

/**********************/
#define DX_DLMSG_MASIC_S   (8)
#define DX_DLMSG_SIZE      (0x40)
#define DX_DLMSG_BAR_MASIC (0xDEE1DEE1)
typedef enum {
    DW_NONE         = 0,
    DW_READY        = 1,
    DW_DONE         = 2, /* download done */
    DW_MAGIC_ERR    = 3,
    DW_HEADER_ERR   = 4,
    DW_CRC_ERR      = 5,
    DW_CERT_ERR     = 6,
    DW_EDMA_ERR     = 7, /* PCIe DMA */
    DW_CONN_ERR     = 8, /* Connection error */
    DW_TIMEOUT_ERR  = 9, /* Timeout error */
} dw_status;

typedef enum {
    DX_NONE        = 0,
    DX_ROM         = 1,
    DX_2ND_BOOT    = 2,
    DX_RTOS        = 3,
} boot_step_t;

typedef enum {
    DW_DEV_NONE = 0,
    DW_DEV_PCIE = 1,
    DW_DEV_UART = 2,
    DW_DEV_USB  = 3,
    DW_DEV_MAX  = 4,
} dw_dev_t;

typedef struct {
    uint8_t      magic[DX_DLMSG_MASIC_S]; /* 0x00 */
    uint32_t     mode;                    /* 0x08 */
    uint8_t      sts;                     /* 0x0C */ /*dw_status*/
    uint8_t      prev_sts;                /* 0x0D */ /*dw_status*/
    uint8_t      dev_type;                /* 0x0E */ /*dw_dev_t*/
    uint8_t      bt_step;                 /* 0x0F */ /*boot_step_t*/
    uint32_t     dl_size;                 /* 0x10 */
    uint32_t     dl_addr_s;               /* 0x14 */
    uint32_t     dl_addr_e;               /* 0x18 */
    uint32_t     bar_magic;               /* 0x1C */
} __attribute__ ((packed,aligned(4))) dx_download_msg;

typedef struct dxrt_request_list
{
    struct list_head list;
    dxrt_dsp_request_t request;
} dxrt_request_list_t;
typedef struct dxrt_response_list
{
    struct list_head list;
    dxrt_response_t response;
} dxrt_response_list_t;

struct dxdev {
    int id;
    struct cdev cdev;
    struct platform_device *pdev;
    struct device *dev;    
    uint32_t type; /* 0: ACC type, 1: STD type */
    uint32_t variant; /* 100: L1, 101: L2, 102: L3, 103: L4, 104: V3, 200: M1, 201: M1A */
    uint64_t mem_addr;
    uint64_t mem_size;
    uint32_t num_dma_ch;
    dxdsp_t *dsp;
    // pcie : TODO
    dxrt_device_message_t *msg;
    struct mutex msg_lock;
    uint32_t *log;
    dx_download_msg *dl;

    //struct list_head sched;
    //spinlock_t       sched_lock;

    struct task_struct *request_handler;
    wait_queue_head_t request_wq;
    dxrt_request_list_t requests;
    spinlock_t requests_lock;

    dxrt_response_list_t responses;
    spinlock_t responses_lock;

    dxrt_response_t response;
    wait_queue_head_t error_wq;
    dxrt_error_t error;
    dxrt_notify_throt_t notify;
    spinlock_t error_lock;
};

struct dxrt_driver {
    dev_t dev_num;
    struct class *dev_class;
    int num_devices;
    struct dxdev *devices[DX_DEVICE_MAX_NUM];
    struct platform_device *pdev;
};

typedef int (*dxrt_message_handler)(struct dxdev*, dxrt_message_t*);

int dxrt_dsp_driver_cdev_init(struct dxrt_driver *drv);
void dxrt_dsp_driver_cdev_deinit(struct dxrt_driver *drv);
int dxrt_request_handler(void *data);
int dxrt_is_request_list_empty(dxrt_request_list_t *requests, spinlock_t *lock);
int message_handler_general(struct dxdev *dx, dxrt_message_t *msg);
void dxrt_device_init(struct dxdev* dev);

extern dxrt_message_handler message_handler[];

#endif // __DXRT_DRV_H
