/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/*
 * DW9827AF_23231_IMX882 voice coil motor driver
 *
 *
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>

#include "lens_info.h"

#define AF_DRVNAME "DW9827AF_23231_IMX882_DRV"
#define AF_I2C_SLAVE_ADDR 0x18

#define AF_DEBUG
#ifdef AF_DEBUG
#define LOG_INF(format, args...)                                               \
    pr_info(AF_DRVNAME " [%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif

static struct i2c_client *g_pstAF_I2Cclient;
static int *g_pAF_Opened;
static spinlock_t *g_pAF_SpinLock;

static unsigned long g_u4AF_INF = 0;
static unsigned long g_u4AF_MACRO = 16383;
static unsigned long g_u4CurrPosition;

static int s4AF_ReadReg(u8 a_uAddr, u16 *a_pu2Result)
{
    int i4RetValue = 0;
    char pBuff;
    char puSendCmd[1];

    puSendCmd[0] = a_uAddr;

    g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;

    g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;

    i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 1);

    if (i4RetValue < 0) {
        LOG_INF("I2C read - send failed!!\n");
        return -1;
    }

    i4RetValue = i2c_master_recv(g_pstAF_I2Cclient, &pBuff, 1);

    if (i4RetValue < 0) {
        LOG_INF("I2C read - recv failed!!\n");
        return -1;
    }
    *a_pu2Result = pBuff;

    return 0;
}

static int s4AF_WriteReg(u16 a_u2Addr, u16 a_u2Data)
{
    int i4RetValue = 0;

    char puSendCmd[2] = {(char)a_u2Addr, (char)a_u2Data};

    g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;

    g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;

    i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);

    if (i4RetValue < 0) {
        LOG_INF("I2C write failed!!\n");
        return -1;
    }

    return 0;
}

static inline int getAFInfo(__user struct stAF_MotorInfo *pstMotorInfo)
{
    struct stAF_MotorInfo stMotorInfo;

    stMotorInfo.u4MacroPosition = g_u4AF_MACRO / 16;
    stMotorInfo.u4InfPosition = g_u4AF_INF / 16;
    stMotorInfo.u4CurrentPosition = g_u4CurrPosition / 16;
    stMotorInfo.bIsSupportSR = 1;

    stMotorInfo.bIsMotorMoving = 1;

    if (*g_pAF_Opened >= 1)
        stMotorInfo.bIsMotorOpen = 1;
    else
        stMotorInfo.bIsMotorOpen = 0;

    if (copy_to_user(pstMotorInfo, &stMotorInfo,
             sizeof(struct stAF_MotorInfo)))
        LOG_INF("copy to user failed when getting motor information\n");

    return 0;
}

/* initAF include driver initialization and standby mode */
static int initAF(void)
{
    LOG_INF("+\n");

    if (*g_pAF_Opened == 1) {

        int ret = 0;

        /* 00:active mode , 10:Standby mode , x1:Sleep mode */
        ret = s4AF_WriteReg(0x02, 0x00);
        mdelay(10);
        spin_lock(g_pAF_SpinLock);
        *g_pAF_Opened = 2;
        spin_unlock(g_pAF_SpinLock);
    }

    LOG_INF("-\n");

    return 0;
}

static inline int setVCMPos(unsigned long a_u4Position)
{
    int i4RetValue = 0;
    LOG_INF("setVCMPos: %ld", a_u4Position);
    i4RetValue = s4AF_WriteReg(0x0, (u16)((a_u4Position >> 6) & 0xff));
    LOG_INF("setVCMPos: i4RetValue1 %d", i4RetValue);
    if (i4RetValue < 0)
        return -1;

    i4RetValue = s4AF_WriteReg(0x1, (u16)(a_u4Position & 0x3f));
    LOG_INF("setVCMPos: i4RetValue2 %d", i4RetValue);
    return i4RetValue;
}

/* moveAF only use to control moving the motor */
static inline int moveAF(unsigned long a_u4Position)
{
    int ret = 0;
    //change from bit 10 to bit 12
    unsigned long a_u4_NewPosition = a_u4Position * 16;
    LOG_INF("moveAF: %ld -> %ld", a_u4Position, a_u4_NewPosition);
    if (setVCMPos(a_u4_NewPosition) == 0) {
        g_u4CurrPosition = a_u4_NewPosition;
        ret = 0;
    } else {
        LOG_INF("set I2C failed when moving the motor\n");
        ret = -1;
    }

    return ret;
}

static inline int setAFInf(unsigned long a_u4Position)
{
    spin_lock(g_pAF_SpinLock);
        //change from bit 10 to bit 14
    g_u4AF_INF = a_u4Position * 4;
    spin_unlock(g_pAF_SpinLock);
    return 0;
}

static inline int setAFMacro(unsigned long a_u4Position)
{
    spin_lock(g_pAF_SpinLock);
        //change from bit 10 to bit 14
    g_u4AF_MACRO = a_u4Position * 16;
    spin_unlock(g_pAF_SpinLock);
    return 0;
}

/* ////////////////////////////////////////////////////////////// */
long DW9827AF_23231_IMX882_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
            unsigned long a_u4Param)
{
    long i4RetValue = 0;

    switch (a_u4Command) {
    case AFIOC_G_MOTORINFO:
        i4RetValue =
            getAFInfo((__user struct stAF_MotorInfo *)(a_u4Param));
        break;

    case AFIOC_T_MOVETO:
        i4RetValue = moveAF(a_u4Param);
        break;

    case AFIOC_T_SETINFPOS:
        i4RetValue = setAFInf(a_u4Param);
        break;

    case AFIOC_T_SETMACROPOS:
        i4RetValue = setAFMacro(a_u4Param);
        break;

    default:
        LOG_INF("No CMD\n");
        i4RetValue = -EPERM;
        break;
    }

    return i4RetValue;
}

/* Main jobs: */
/* 1.Deallocate anything that "open" allocated in private_data. */
/* 2.Shut down the device on last close. */
/* 3.Only called once on last time. */
/* Q1 : Try release multiple times. */
int DW9827AF_23231_IMX882_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
    LOG_INF("Start\n");

    if (*g_pAF_Opened == 2) {
        LOG_INF("Wait\n");
        s4AF_WriteReg(0x02, 0x20);
        mdelay(3);
    }

    if (*g_pAF_Opened) {
        LOG_INF("Free\n");

        spin_lock(g_pAF_SpinLock);
        *g_pAF_Opened = 0;
        spin_unlock(g_pAF_SpinLock);
    }

    LOG_INF("End\n");

    return 0;
}

int DW9827AF_23231_IMX882_PowerDown(struct i2c_client *pstAF_I2Cclient,
            int *pAF_Opened)
{
    g_pstAF_I2Cclient = pstAF_I2Cclient;
    g_pAF_Opened = pAF_Opened;

    LOG_INF("+\n");
    if (*g_pAF_Opened == 0) {
        unsigned short data = 0;
        int cnt = 0;

        while (1) {
            data = 0;

            s4AF_WriteReg(0x02, 0x20);

            s4AF_ReadReg(0x02, &data);

            LOG_INF("Addr : 0x02 , Data : %x\n", data);

            if (data == 0x20 || cnt == 1)
                break;

            cnt++;
        }
    }
    LOG_INF("-\n");

    return 0;
}

int DW9827AF_23231_IMX882_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
              spinlock_t *pAF_SpinLock, int *pAF_Opened)
{
    g_pstAF_I2Cclient = pstAF_I2Cclient;
    g_pAF_SpinLock = pAF_SpinLock;
    g_pAF_Opened = pAF_Opened;

    initAF();

    return 1;
}

int DW9827AF_23231_IMX882_GetFileName(unsigned char *pFileName)
{
    #if SUPPORT_GETTING_LENS_FOLDER_NAME
    char FilePath[256];
    char *FileString;

    sprintf(FilePath, "%s", __FILE__);
    FileString = strrchr(FilePath, '/');
    *FileString = '\0';
    FileString = (strrchr(FilePath, '/') + 1);
    strncpy(pFileName, FileString, AF_MOTOR_NAME);
    LOG_INF("FileName : %s\n", pFileName);
    #else
    pFileName[0] = '\0';
    #endif
    return 1;
}

#if !defined(CONFIG_MACH_MT6765)
extern int fan53870_cam_ldo_set_voltage(int LDO_NUM, int set_mv);
extern int fan53870_cam_ldo_disable(int LDO_NUM);
#endif
int DW9827AF_23231_IMX882_DoExtLdo(int enable)
{
    LOG_INF("DW9827AF_23231_IMX882_DoExtLdo enable: %d", enable);
#if !defined(CONFIG_MACH_MT6765)
    if(enable) {
        fan53870_cam_ldo_set_voltage(6, 2800);
    }else {
        fan53870_cam_ldo_disable(6);
    }
#endif
    mdelay(10);
    return 1;
}
