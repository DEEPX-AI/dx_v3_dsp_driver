// SPDX-License-Identifier: GPL-2.0
/*
 * Deepx Runtime Driver
 *
 * Copyright (C) 2023 Deepx, Inc.
 *
 */
#ifndef __DSP_REG_DX_V3_H
#define __DSP_REG_DX_V3_H

#include <linux/types.h>

//<0x49000000 0x00001000>, /* reg: control            */
//<0x1A040000 0x00001000>, /* reg: power on for debug */
//<0x18203000 0x00001000>, /* reg: debug (reset...)   */
//<0x1B000000 0x00100000>, /* reg: mailbox & uart     */
//<0x60000000 0x00040000>, /* mem: sram (256KB)       */
//<0xC0000000 0x00200000>, /* mem: rom & ram code     */
//<0xC0200000 0x1FF00000>, /* mem: accessible dram    */

//#define XRP_REG_RUNSTALL	(0x00)
//#define XRP_REG_CLOCK		(0x04)
//#define XRP_REG_DBGPWR	(0x00)
//#define XRP_REG_RESET		(0x20)
//#define XRP_REG_UART		(0x1000)

#define MESSAGE_MAX_SIZE 256

/* Address */
#define REG_DSP_SYS_OFFSET 0x0
#define REG_DSP_RUNSTALL   (REG_DSP_SYS_OFFSET + 0x00000000)
#define REG_DSP_CLOCK      (REG_DSP_SYS_OFFSET + 0x00000004)
#define REG_DSP_RESET_CTRL (REG_DSP_SYS_OFFSET + 0x0000000C)
#define REG_DSP_STATUS     (REG_DSP_SYS_OFFSET + 0x0000000C)//temporally used ==> plz, replace this addr to dummy register on V3A 

#define REG_DSP_DBGPWR_OFFSET 0x0
#define REG_DSP_DBGPWR   (REG_DSP_DBGPWR_OFFSET + 0x00000000)

#define REG_DSP_DEBUG_OFFSET 0x0
#define REG_DSP_RESET    (REG_DSP_DEBUG_OFFSET + 0x00000020)

#define REG_DSP_MAILBOX_UART_OFFSET 0x0
#define REG_DSP_MAILBOX  (REG_DSP_MAILBOX_UART_OFFSET + 0x00000000)
#define REG_DSP_UART     (REG_DSP_MAILBOX_UART_OFFSET + 0x00001000)

#define REG_DSP_MSG_OFFSET 0x3F000
#define REG_DSP_MSG   (REG_DSP_MSG_OFFSET + 0x00000000)

//#define REG_DSP_DATADRAM_OFFSET 0x001F0000
//#define REG_DSP_STATUS  (REG_DSP_DATADRAM_OFFSET + 0x0000FFF0)

// SYSYTEM
#define WRITE_DSP_RUNSTALL(base, val) dsp_reg_write(base, REG_DSP_RUNSTALL, val)
#define WRITE_DSP_CLOCK_CTRL(base, val) dsp_reg_write(base, REG_DSP_CLOCK, val)
#define WRITE_DSP_RESET_CTRL(base, val) dsp_reg_write(base, REG_DSP_RESET_CTRL, val)
#define READ_DSP_RESET_CTRL(base) dsp_reg_read(base, REG_DSP_RESET_CTRL)

#define WRITE_DSP_STATUS(base, val) dsp_reg_write(base, REG_DSP_STATUS, val)//temporally used ==> plz, replace this addr to dummy register on V3A 
#define READ_DSP_STATUS(base) dsp_reg_read(base, REG_DSP_STATUS)//temporally used ==> plz, replace this addr to dummy register on V3A

// DEBUG POWER
#define WRITE_DSP_DBGPWR_CTRL(base, val) dsp_reg_write(base, REG_DSP_DBGPWR, val)

// DEBUG
#define WRITE_DSP_RESET(base, val) dsp_reg_write(base, REG_DSP_RESET, val)

// MESSEGE
#define WRITE_DSP_MSG_HEAD(base, val, idx)  dsp_reg_write(base, (REG_DSP_MSG + idx*4      ), val)
#define WRITE_DSP_MSG_DATA(base, val, idx)  dsp_reg_write(base, (REG_DSP_MSG + idx*4 + 0x8), val)

// WRITE DUMMY
#define WRITE_DSP_DUMMY(base, offset, val) dsp_reg_write(base, offset, val)

// MAILBOX & UART
//#define WRITE_DSP_IRQ(base, val) dsp_reg_write(base, ???, val)
//#define READ_DSP_IRQ(base) dsp_reg_read(base, ???)

////////////////////////////////////////////////////////////////////////////////////////////////////
// IRQ

#ifdef __cplusplus
  #define   __I     volatile             /*!< Defines 'read only' permissions                 */
#else
  #define   __I     volatile const       /*!< Defines 'read only' permissions                 */
#endif
#define     __O     volatile             /*!< Defines 'write only' permissions                */
#define     __IO    volatile             /*!< Defines 'read / write' permissions              */

#define MAX_NUM_STAT_REG 124
 
typedef struct {
    __I   uint32_t      CH_ST;     //0x00
          uint32_t      Reserved0; //0x04
          uint32_t      Reserved1; //0x08
    __O   uint32_t      CH_SET;    //0x0C
    __I   uint32_t      CH_INT_ST; //0x10
    __IO  uint32_t      CH_INT_CLR;//0x14
    __IO  uint32_t      CH_INT_EN; //0x18
          uint32_t      Reserved2; //0x1C
} MHUV2_SENDER_SLOT_TypeDef;

typedef struct {
          MHUV2_SENDER_SLOT_TypeDef CHANNEL[MAX_NUM_STAT_REG]; //0x0-0xF7C
    __I   uint32_t      MHU_CFG;       //0xF80
    __IO  uint32_t      RESP_CFG;      //0xF84
    __IO  uint32_t      ACCESS_REQUEST;//0xF88
    __I   uint32_t      ACCESS_READY;  //0xF8C
    __I   uint32_t      INT_ST;        //0xF90
    __O   uint32_t      INT_CLR;       //0xF94
    __IO  uint32_t      INT_EN;        //0xF98
          uint32_t      Reserved0;
    __I   uint32_t      CH_INT_ST0;
    __I   uint32_t      CH_INT_ST1;
    __I   uint32_t      CH_INT_ST2;
    __I   uint32_t      CH_INT_ST3;
          uint32_t      Reserved1[6];
    __I   uint32_t      IIDR;
    __I   uint32_t      AIDR;
    __I   uint32_t      PID4;
    __I   uint32_t      Reserved2[3];
    __I   uint32_t      PID0;
    __I   uint32_t      PID1;
    __I   uint32_t      PID2;
    __I   uint32_t      PID3;
    __I   uint32_t      CID0;
    __I   uint32_t      CID1;
    __I   uint32_t      CID2;
    __I   uint32_t      CID3;
} MHUV2_SND_TypeDef;

typedef struct {
    __I   uint32_t      STAT;      //0x00
    __I   uint32_t      STAT_PEND; //0x04
    __O   uint32_t      STAT_CLEAR;//0x08 
          uint32_t      Reserved0; //0x0C
    __I   uint32_t      MASK;      //0x10
    __O   uint32_t      MASK_SET;  //0x14
    __O   uint32_t      MASK_CLEAR;//0x18
          uint32_t      Reserved1; //0x1C
} MHUV2_RECEIVER_SLOT_TypeDef;

typedef struct {
          MHUV2_RECEIVER_SLOT_TypeDef CHANNEL[MAX_NUM_STAT_REG];
    __I   uint32_t      MHU_CFG;
          uint32_t      Reserved0[3];
    __I   uint32_t      INT_ST;
    __O   uint32_t      INT_CLR;
    __IO  uint32_t      INT_EN;
          uint32_t      Reserved1;
    __I   uint32_t      CH_INT_ST0;
    __I   uint32_t      CH_INT_ST1;
    __I   uint32_t      CH_INT_ST2;
    __I   uint32_t      CH_INT_ST3;
    __I   uint32_t      Reserved2[6];
    __I   uint32_t      IIDR;
    __I   uint32_t      AIDR;
    __I   uint32_t      PID4;
    __I   uint32_t      Reserved3[3];
    __I   uint32_t      PID0;
    __I   uint32_t      PID1;
    __I   uint32_t      PID2;
    __I   uint32_t      PID3;
    __I   uint32_t      CID0;
    __I   uint32_t      CID1;
    __I   uint32_t      CID2;
    __I   uint32_t      CID3;
} MHUV2_REC_TypeDef;

typedef enum {LOW_PRI,HIGH_PRI} mhuv2_ns_intr_t;

/* peripheral and component ID values */
#define MHU_SND_IIDR  0x0760043B
#define MHU_SND_AIDR  0x00000011
#define MHU_SND_PID4  0x00000004
#define MHU_SND_PID0  0x00000076
#define MHU_SND_PID1  0x000000B0
#define MHU_SND_PID2  0x0000000B
#define MHU_SND_PID3  0x00000000
#define MHU_SND_CID0  0x0000000D
#define MHU_SND_CID1  0x000000F0
#define MHU_SND_CID2  0x00000005
#define MHU_SND_CID3  0x000000B1

#define MHU_REC_PID4  0x04
#define MHU_REC_PID0  0x76
#define MHU_REC_PID1  0xB0
#define MHU_REC_PID2  0x0B
#define MHU_REC_PID3  0x00
#define MHU_REC_CID0  0x0D
#define MHU_REC_CID1  0xF0
#define MHU_REC_CID2  0x05
#define MHU_REC_CID3  0xB1
#define MHU_REC_AIDR  0x11
#define MHU_REC_IIDR  0x0760043B

#define REG_DSP_IRQ_INT_EN 0xF98
#define REG_DSP_IRQ_EN_CH0 0x18
#define REG_DSP_IRQ_EN_CH1 0x38
#define REG_DSP_IRQ_STATUS_CH0 0x10
#define REG_DSP_IRQ_STATUS_CH1 0x30
#define REG_DSP_IRQ_CLR_CH0 0x14
#define REG_DSP_IRQ_CLR_CH1 0x34

#define WRITE_DSP_IRQ_INT_EN(base, val) dsp_reg_write(base, (REG_DSP_MAILBOX+REG_DSP_IRQ_INT_EN), val)
#define WRITE_DSP_IRQ_EN_CH0(base, val) dsp_reg_write(base, (REG_DSP_MAILBOX+REG_DSP_IRQ_EN_CH0), val)
#define WRITE_DSP_IRQ_EN_CH1(base, val) dsp_reg_write(base, (REG_DSP_MAILBOX+REG_DSP_IRQ_EN_CH1), val)
#define READ_DSP_IRQ_STATUS_CH0(base) dsp_reg_read(base, (REG_DSP_MAILBOX+REG_DSP_IRQ_STATUS_CH0))
#define READ_DSP_IRQ_STATUS_CH1(base) dsp_reg_read(base, (REG_DSP_MAILBOX+REG_DSP_IRQ_STATUS_CH1))
#define WRITE_DSP_IRQ_CLR_CH0(base, val) dsp_reg_write(base, (REG_DSP_MAILBOX+REG_DSP_IRQ_CLR_CH0), val)
#define WRITE_DSP_IRQ_CLR_CH1(base, val) dsp_reg_write(base, (REG_DSP_MAILBOX+REG_DSP_IRQ_CLR_CH1), val)

//~IRQ
////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // __DSP_REG_DX_V3_H
