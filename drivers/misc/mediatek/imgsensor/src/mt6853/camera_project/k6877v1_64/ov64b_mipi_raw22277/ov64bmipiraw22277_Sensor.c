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

/*****************************************************************************
 *
 * Filename:
 * ---------
 *     OV64B2Qmipi_Sensor.c
 *
 * Project:
 * --------
 *     ALPS
 *
 * Description:
 * ------------
 *     Source code of Sensor driver
 *
 * Setting version:
 * ------------
 *   update full pd setting for OV64B2QEB_03B
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/

#define PFX "OV64B2Q_camera_sensor"

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/types.h>
#include "imgsensor_eeprom.h"
#include "ov64bmipiraw22277_Sensor.h"
#include "ov64b_Sensor22277_setting.h"
#include "imgsensor_hwcfg_custom.h"
#include <soc/oplus/system/oplus_project.h>

#define _I2C_BUF_SIZE 4096
static kal_uint16 _i2c_data [_I2C_BUF_SIZE];
static unsigned int _size_to_write;
static bool _is_seamless;
static unsigned int _is_initFlag;

extern enum IMGSENSOR_RETURN Eeprom_DataInit(
            enum IMGSENSOR_SENSOR_IDX sensor_idx,
            kal_uint32 sensorID);
extern struct CAMERA_DEVICE_INFO gImgEepromInfo;
static kal_uint8 deviceInfo_register_value = 0x00;

#define LOG_INF(format, args...)    \
    pr_debug(PFX "[%s] " format, __func__, ##args)

#define I2C_BUFFER_LEN 765    /*trans# max is 255, each 3 bytes*/
#define LEFT_SIZE (0x4000)
#define READ_EEPROM_1K (1024)
#define EEPROM_I2C_ADDR 0xA0
static kal_uint8 otp_data[LEFT_SIZE] = {0};
static DEFINE_SPINLOCK(imgsensor_drv_lock);
static struct imgsensor_info_struct imgsensor_info = {
    .sensor_id = OV64B_SENSOR_ID22277,

    .checksum_value = 0x81c2c910,//test_Pattern_mode

    .pre = {
        .pclk = 115200000,
        .linelength = 696,
        .framelength = 3600,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 4624,
        .grabwindow_height = 3468,
        .mipi_data_lp2hs_settle_dc = 85,
        .mipi_pixel_rate = 952000000,
        .max_framerate = 460,
    },

    .cap = {
        .pclk = 115200000,
        .linelength = 696,
        .framelength = 3600,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 4624,
        .grabwindow_height = 3468,
        .mipi_data_lp2hs_settle_dc = 85,
        .mipi_pixel_rate = 952000000,
        .max_framerate = 460,
    },

    .normal_video = { /*4624*2600@30fps*/
        .pclk = 115200000,
        .linelength = 936,
        .framelength = 4102,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 4624,
        .grabwindow_height = 2604,
        .mipi_data_lp2hs_settle_dc = 85,
        .mipi_pixel_rate = 679680000,
        .max_framerate = 300,
    },

    .hs_video = { /*2304*1296@120fps (binning)*/
        .pclk = 115200000,
        .linelength = 504,
        .framelength = 1904,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 2304,
        .grabwindow_height = 1296,
        .mipi_data_lp2hs_settle_dc = 85,
        .mipi_pixel_rate = 679680000,
        .max_framerate = 1200,
    },

    .slim_video = { /* 4624*3468@30fps */
        .pclk = 115200000,
        .linelength = 936,
        .framelength = 4102,
        .startx = 8,
        .starty = 6,
        .grabwindow_width = 4608,
        .grabwindow_height = 3456,
        .mipi_data_lp2hs_settle_dc = 85,
        .mipi_pixel_rate = 679680000,
        .max_framerate = 300,
    },

    .custom1 = {
        .pclk = 115200000,
        .linelength = 1224,
        .framelength = 3920,
        .startx =0,
        .starty = 0,
        .grabwindow_width = 4624,
        .grabwindow_height = 3468,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 240,
        .mipi_pixel_rate = 518400000,
    },

    .custom2 = { /* reg_N 4K @ 60fps (binning)*/
        .pclk = 115200000,
        .linelength = 792,
        .framelength = 2424,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 4000,
        .grabwindow_height = 2256,
        .mipi_data_lp2hs_settle_dc = 85,
        .mipi_pixel_rate = 802560000,
        .max_framerate = 600,
    },

    .custom3 = {
        .pclk = 115200000,
        .linelength = 1008,
        .framelength = 7618,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 9248,
        .grabwindow_height = 6936,
        .mipi_data_lp2hs_settle_dc = 85,
        .mipi_pixel_rate = 1200000000,
        .max_framerate = 150,
    },

    .custom4 = { /* reg_G 2304x1296@240fps */
        .pclk = 115200000,
        .linelength = 336,
        .framelength = 1428,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 2304,
        .grabwindow_height = 1296,
        .mipi_data_lp2hs_settle_dc = 85,
        .mipi_pixel_rate = 998400000,
        .max_framerate = 2400,
    },

    .custom5 = { // reg_C-4 4624x3468@30fps
        .pclk = 115200000,
        .linelength = 696,
        .framelength = 3600,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 4624,
        .grabwindow_height = 3468,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 460,
        .mipi_pixel_rate = 952000000,
    },

    .custom6 = { // reg_C-4 4624x2600@60fps
        .pclk = 115200000,
        .linelength = 696,
        .framelength = 2758,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 4624,
        .grabwindow_height = 2600,
        .mipi_data_lp2hs_settle_dc = 85,
        .mipi_pixel_rate = 920000000,
        .max_framerate = 600,
    },

    .custom7 = { //4624x2600@30fps
        .pclk = 115200000,
        .linelength = 936,
        .framelength = 4102,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 4624,
        .grabwindow_height = 3468,
        .mipi_data_lp2hs_settle_dc = 85,
        .mipi_pixel_rate = 679680000,
        .max_framerate = 300,
    },

    .custom8 = { //4624x2600@15fps
        .pclk = 115200000,
        .linelength = 1320,
        .framelength = 2910,
        .startx =0,
        .starty = 0,
        .grabwindow_width = 4624,
        .grabwindow_height = 2600,
        .mipi_data_lp2hs_settle_dc = 85,
        .mipi_pixel_rate = 480000000,
        .max_framerate = 150,
    },

    .custom9 = { /*2304*1728@30fps (binning)*/
        .pclk = 115200000,
        .linelength = 1008,
        .framelength = 4760,
        .startx = 4,
        .starty = 4,
        .grabwindow_width = 2304,
        .grabwindow_height = 1728,
        .mipi_data_lp2hs_settle_dc = 85,
        .mipi_pixel_rate = 679680000,
        .max_framerate = 240,
    },

    .custom10 = { /*2304*1728@15fps (binning)*/
        .pclk = 115200000,
        .linelength = 1008,
        .framelength = 3808,
        .startx = 4,
        .starty = 4,
        .grabwindow_width = 2304,
        .grabwindow_height = 1728,
        .mipi_data_lp2hs_settle_dc = 85,
        .mipi_pixel_rate = 339840000,
        .max_framerate = 150,
    },
    .margin = 31,                    /* sensor framelength & shutter margin */
    .min_shutter = 8,                /* min shutter */
    .min_gain = 64, /*1x gain*/
    .max_gain = 992, /*64x gain*/
    .min_gain_iso = 100,
    .gain_step = 1,
    .gain_type = 1,/*to be modify,no gain table for sony*/
    .max_frame_length = 0xffffe9,     /* max framelength by sensor register's limitation */
    .ae_shut_delay_frame = 0,        //check
    .ae_sensor_gain_delay_frame = 0,//check
    .ae_ispGain_delay_frame = 2,
    .ihdr_support = 0,
    .ihdr_le_firstline = 0,
    .sensor_mode_num = 15,            //support sensor mode num

    .cap_delay_frame = 2,            //enter capture delay frame num
    .pre_delay_frame = 2,            //enter preview delay frame num
    .video_delay_frame = 2,            //enter video delay frame num
    .hs_video_delay_frame = 2,        //enter high speed video  delay frame num
    .slim_video_delay_frame = 2,    //enter slim video delay frame num
    .custom1_delay_frame = 2,        //enter custom1 delay frame num
    .custom2_delay_frame = 2,        //enter custom2 delay frame num
    .custom3_delay_frame = 2,        //enter custom3 delay frame num
    .custom4_delay_frame = 2,        //enter custom4 delay frame num
    .custom5_delay_frame = 2,        //enter custom5 delay frame num
    .custom6_delay_frame = 2,        //enter custom1 delay frame num
    .custom7_delay_frame = 2,        //enter custom2 delay frame num
    .custom8_delay_frame = 2,        //enter custom3 delay frame num
    .custom9_delay_frame = 2,        //enter custom4 delay frame num
    .custom10_delay_frame = 2,        //enter custom5 delay frame num
    .frame_time_delay_frame = 2,

    .isp_driving_current = ISP_DRIVING_8MA,
    .sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
    .mipi_sensor_type = MIPI_OPHY_NCSI2,
    .mipi_settle_delay_mode = 1,
    .sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_B,
    .mclk = 24,//mclk value, suggest 24 or 26 for 24Mhz or 26Mhz
    .mipi_lane_num = SENSOR_MIPI_4_LANE,//mipi lane num
    .i2c_addr_table = {0x6c, 0xff},
    .i2c_speed = 1000,
};

static struct imgsensor_struct imgsensor = {
    .mirror = IMAGE_NORMAL,
    .sensor_mode = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
    .shutter = 0x3D0,
    .gain = 0x100,
    .dummy_pixel = 0,
    .dummy_line = 0,
    .current_fps = 30,
    .autoflicker_en = KAL_FALSE,
    .test_pattern = KAL_FALSE,
    .current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
    .ihdr_en = 0,
    .i2c_write_id = 0x6c,
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[15] = {
    {9280, 6944, 16,   0, 9248, 6944, 4624, 3472, 0, 2, 4624, 3468, 0, 0, 4624, 3468}, /*Preview*/
    {9280, 6944, 16,   0, 9248, 6944, 4624, 3472, 0, 2, 4624, 3468, 0, 0, 4624, 3468}, /*capture*/
    {9280, 6944, 16, 864, 9248, 5216, 4624, 2608, 0, 2, 4624, 2604, 0, 0, 4624, 2604}, /*normal video*/
    {9280, 6944, 32, 864, 9216, 5216, 2304, 1304, 0, 4, 2304, 1296, 0, 0, 2304, 1296}, /*hs_video*/
    {9280, 6944, 16,   0, 9248, 6944, 4624, 3472, 0, 2, 4624, 3468, 8, 6, 4608, 3456}, /*slim video*/
    {9280, 6944, 16,   0, 9248, 6944, 4624, 3472, 0, 2, 4624, 3468, 0, 0, 4624, 3468}, /*custom1*/
    {9280, 6944,640,1216, 8000, 4512, 4000, 2256, 0, 0, 4000, 2256, 0, 0, 4000, 2256}, /*custom2*/
    {9280, 6944,  0,   0, 9280, 6944, 9280, 6944,16, 4, 9248, 6936, 0, 0, 9248, 6936}, /*custom3*/
    {9280, 6944, 32, 864, 9216, 5216, 2304, 1304, 0, 4, 2304, 1296, 0, 0, 2304, 1296}, /*custom4*/
    {9280, 6944, 2304, 1728, 4672, 3488, 4672, 3488, 24, 10, 4624, 3468, 0, 0, 4624, 3468},  /*custom5*/
    {9280, 6944, 16, 864, 9248, 5216, 4624, 2608, 0, 4, 4624, 2600, 0, 0, 4624, 2600},  /*custom6*/
    {9280, 6944, 16,   0, 9248, 6944, 4624, 3472, 0, 2, 4624, 3468, 0, 0, 4624, 3468},  /*custom7*/
    {9280, 6944, 16, 864, 9248, 5216, 4624, 2608, 0, 4, 4624, 2600, 0, 0, 4624, 2600}, /*custom8*/
    {9280, 6944, 16,   0, 9248, 6944, 2312, 1736, 0, 0, 2312, 1736, 4, 4, 2304, 1728}, /*custom9*/
    {9280, 6944, 16,   0, 9248, 6944, 2312, 1736, 0, 0, 2312, 1736, 4, 4, 2304, 1728} /*custom10*/
};

static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[4] = {
    /*4624*3468 1136(pxiel)*860 568pixel/line * (860*2)line*/
    {
        0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
        0x00, 0x2b, 0x0FA0, 0x0BB8, 0x00, 0x00, 0x0280, 0x0001,
        0x01, 0x2B, 0x0470, 0x035C, 0x03, 0x00, 0x0000, 0x0000
    },
    /*4624*2600 1136(pxiel)*648 568pixel/line*(648*2)line */
    {
        0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
        0x00, 0x2b, 0x0FA0, 0x08D0, 0x00, 0x00, 0x0280, 0x0001,
        0x01, 0x2B, 0x0470, 0x0288, 0x03, 0x00, 0x0000, 0x0000
    },
    /* 4624*3000 1136(pxiel)*748 568pixel/line*(748*2)line*/
    {
        0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
        0x00, 0x2b, 0x0FA0, 0x08D0, 0x00, 0x00, 0x0280, 0x0001,
        0x01, 0x2B, 0x03E0, 0x0234, 0x03, 0x00, 0x0000, 0x0000
    },
     /* custom2 mode setting 480(pxiel)*1080*/
    {
        0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
        0x00, 0x2b, 0x0FA0, 0x08D0, 0x00, 0x00, 0x0280, 0x0001,
        0x01, 0x2B, 0x0470, 0x0438, 0x03, 0x00, 0x0000, 0x0000
    },
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
     .i4OffsetX = 40,
     .i4OffsetY = 14,
     .i4PitchX = 16,
     .i4PitchY = 16,
     .i4PairNum = 8,
     .i4SubBlkW = 8,
     .i4SubBlkH = 4,
     .i4PosL = {{47, 16}, {55, 16}, {43, 20}, {51, 20},
                {47, 24}, {55, 24}, {43, 28}, {51, 28} },
     .i4PosR = {{46, 16}, {54, 16}, {42, 20}, {50, 20},
                {46, 24}, {54, 24}, {42, 28}, {50, 28} },
     .iMirrorFlip = 0,
     .i4BlockNumX = 284,
     .i4BlockNumY = 215,
     .i4Crop = { {0, 0}, {0, 0}, {0, 434}, {1000, 936}, {8, 6},
        {0, 0}, {312, 606}, {0, 0}, {1000, 936}, {0, 234} },
};

static kal_uint16 ov64b2q_table_write_cmos_sensor(
                    kal_uint16 *para, kal_uint32 len)
{
    char puSendCmd[I2C_BUFFER_LEN];
    kal_uint32 tosend, IDX;
    kal_uint16 addr = 0, addr_last = 0, data;

    tosend = 0;
    IDX = 0;
    while (len > IDX) {
        addr = para[IDX];
        {
            puSendCmd[tosend++] = (char)(addr >> 8);
            puSendCmd[tosend++] = (char)(addr & 0xFF);
            data = para[IDX + 1];
            puSendCmd[tosend++] = (char)(data & 0xFF);
            IDX += 2;
            addr_last = addr;
        }

        if ((I2C_BUFFER_LEN - tosend) < 3 ||
            len == IDX ||
            addr != addr_last) {
            iBurstWriteReg_multi(puSendCmd, tosend,
                imgsensor.i2c_write_id,
                3, imgsensor_info.i2c_speed);
            tosend = 0;
        }
    }
    return 0;
}

static kal_uint16 ov64b2q_burst_write_cmos_sensor(
                    kal_uint16 *para, kal_uint32 len)
{
    char puSendCmd[I2C_BUFFER_LEN];
    kal_uint32 tosend, IDX;
    kal_uint16 addr = 0, addr_last = 0, data;

    tosend = 0;
    IDX = 0;
    while (len > IDX) {
        addr = para[IDX];
        if(tosend==0)
        {
            puSendCmd[tosend++] = (char)(addr >> 8);
            puSendCmd[tosend++] = (char)(addr & 0xFF);
            data = para[IDX + 1];
            puSendCmd[tosend++] = (char)(data & 0xFF);
            IDX += 2;
            addr_last = addr;
        }
        else if(addr == addr_last+1)
        {
            data = para[IDX + 1];
            puSendCmd[tosend++] = (char)(data & 0xFF);
            addr_last = addr;
            IDX += 2;
        }

        if (( tosend>=I2C_BUFFER_LEN)||
            len == IDX ||
            addr != addr_last) {
            iBurstWriteReg_multi(puSendCmd, tosend,
                imgsensor.i2c_write_id,
                tosend, imgsensor_info.i2c_speed);
            tosend = 0;
        }
    }
    return 0;
}

/*OVPD-1:720Bytes & Crosstalk 288Bytes*/
static kal_uint16 ov64b_QSC_OVPD_setting[720*2];
static kal_uint16 ov64b_QSC_CT_setting[288*2];
static kal_uint8 ct_qsc_flag = 0;
static kal_uint8 crossTalk_flag = 0;

static void read_EepromQSC(void)
{
    kal_uint16 addr_ovpd = 0x190C, senaddr_ovpd = 0x5F80;
    kal_uint16 addr_crotalk = 0x1C00, senaddr_crotalk = 0x5A40;
    kal_uint16 i = 0;
    kal_uint32 Dac_master = 0, Dac_mac = 0, Dac_inf = 0;
    kal_uint32 vcmID = 0, lensID = 0;
    /*Read OVPD*/
    for (i = 0; i < 720; i ++) {
        ov64b_QSC_OVPD_setting[2*i] = senaddr_ovpd+i;
        ov64b_QSC_OVPD_setting[2*i+1] = Eeprom_1ByteDataRead((addr_ovpd+i), 0xA0);
    }
    /*Read crosstalk*/
    for (i = 0; i < 288; i ++) {
        ov64b_QSC_CT_setting[2*i] = senaddr_crotalk+i;
        ov64b_QSC_CT_setting[2*i+1] = Eeprom_1ByteDataRead((addr_crotalk+i), 0xA0);
    }
    /*Read normal eeprom data*/
    gImgEepromInfo.camNormdata[0][0] = Eeprom_1ByteDataRead(0x00, 0xA0);
    gImgEepromInfo.camNormdata[0][1] = Eeprom_1ByteDataRead(0x01, 0xA0);
    imgsensor_info.module_id = Eeprom_1ByteDataRead(0x00, 0xA0);
    Oplusimgsensor_Registdeviceinfo(gImgEepromInfo.pCamModuleInfo[0].name,
                                    gImgEepromInfo.pCamModuleInfo[0].version,
                                    imgsensor_info.module_id);
    for(i = 2; i < 8; i++) {
       gImgEepromInfo.camNormdata[0][i] = Eeprom_1ByteDataRead(0x04+i, 0xA0);
    }
    for (i = 0; i < OPLUS_CAMERASN_LENS_23; i ++) {
       gImgEepromInfo.camNormdata[0][8+i] = Eeprom_1ByteDataRead(0xB0+i, 0xA0);
    }
    Dac_mac = (Eeprom_1ByteDataRead(0x93, 0xA0) << 8) | Eeprom_1ByteDataRead(0x92, 0xA0);
    Dac_inf = (Eeprom_1ByteDataRead(0x95, 0xA0) << 8) | Eeprom_1ByteDataRead(0x94, 0xA0);
    vcmID = (Eeprom_1ByteDataRead(0xb, 0xA0) << 8) | Eeprom_1ByteDataRead(0xa, 0xA0);
    lensID = (Eeprom_1ByteDataRead(0x9, 0xA0) << 8) | Eeprom_1ByteDataRead(0x8, 0xA0);
    Dac_master = (5*Dac_mac+36*Dac_inf)/41;
    pr_info("Dac_inf:%d Dac_Mac:%d Dac_master:%d\n", Dac_inf, Dac_mac, Dac_master);
    memcpy(&gImgEepromInfo.camNormdata[2][28], &Dac_master, 4);
    memcpy(&gImgEepromInfo.camNormdata[0][40], &vcmID, 4);
    memcpy(&gImgEepromInfo.camNormdata[0][44], &lensID, 4);
    memcpy(&gImgEepromInfo.camNormdata[0][48], &Dac_mac, 4);
    memcpy(&gImgEepromInfo.camNormdata[0][52], &Dac_inf, 4);
    /*Read stereo eeprom data*/
    for (i = 0; i < CALI_DATA_MASTER_LENGTH; i ++) {
       gImgEepromInfo.stereoMWdata[i] = Eeprom_1ByteDataRead(OV64B_STEREO_START_ADDR+i, 0xA0);
    }
    gImgEepromInfo.i4CurSensorIdx = 0;
    gImgEepromInfo.i4CurSensorId = imgsensor_info.sensor_id;
}

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
    kal_uint16 get_byte = 0;
    char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };
    iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);
    return get_byte;
}
static void write_sensor_OVPD_QSC(void)
{
    LOG_INF("%s start\n", __func__);

    ov64b2q_burst_write_cmos_sensor(ov64b_QSC_OVPD_setting,
        sizeof(ov64b_QSC_OVPD_setting) / sizeof(kal_uint16));

    LOG_INF("%s end\n", __func__);
}
static void write_sensor_CT_QSC(void)
{
    LOG_INF("%s start\n", __func__);

    if(!ct_qsc_flag) { //crosstalk Flag valid
        ov64b2q_burst_write_cmos_sensor(ov64b_QSC_CT_setting,
            sizeof(ov64b_QSC_CT_setting) / sizeof(kal_uint16));
        ct_qsc_flag = 1;
    }

    LOG_INF("%s end\n", __func__);
}

static void write_sensor_crossTalk_data(void)
{
    LOG_INF("%s start\n", __func__);

    if( !crossTalk_flag ) {
        ov64b2q_burst_write_cmos_sensor(addr_data_pair_crossTalk_ov64b2q,
            sizeof(addr_data_pair_crossTalk_ov64b2q) / sizeof(kal_uint16));
        crossTalk_flag = 1;
    }

    LOG_INF("%s end\n", __func__);
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
    char pusendcmd[4] = {(char)(addr >> 8),
        (char)(addr & 0xFF), (char)(para & 0xFF)};
    //iWriteRegI2C(pusendcmd, 3, imgsensor.i2c_write_id);
    iWriteRegI2CTiming(pusendcmd, 3, imgsensor.i2c_write_id,
        imgsensor_info.i2c_speed);
}

static void set_dummy(void)
{
	//for protect 60hz banding filter let shutter bigger than framelength.
    if (imgsensor.shutter > imgsensor.min_frame_length - imgsensor_info.margin) {
        imgsensor.frame_length = imgsensor.shutter + imgsensor_info.margin;
    }

    if(!_is_seamless) {
        //imgsensor.frame_length = (imgsensor.frame_length  >> 1) << 1;
        //write_cmos_sensor(0x3208, 0x00);
        write_cmos_sensor(0x380c, imgsensor.line_length >> 8);
        write_cmos_sensor(0x380d, imgsensor.line_length & 0xFF);
        write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
        write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
        //write_cmos_sensor(0x3208, 0x10);
        //write_cmos_sensor(0x3208, 0xa0);
    } else {
        _i2c_data[_size_to_write++] = 0x3840;
        _i2c_data[_size_to_write++] = imgsensor.frame_length >> 16;
        _i2c_data[_size_to_write++] = 0x380e;
        _i2c_data[_size_to_write++] = imgsensor.frame_length >> 8;
        _i2c_data[_size_to_write++] = 0x380f;
        _i2c_data[_size_to_write++] = imgsensor.frame_length & 0xFF;
    }

}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
    kal_uint32 frame_length = imgsensor.frame_length;

    frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;

    spin_lock(&imgsensor_drv_lock);
    imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ?
            frame_length : imgsensor.min_frame_length;
    imgsensor.dummy_line = imgsensor.frame_length -
        imgsensor.min_frame_length;

    if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
    imgsensor.frame_length = imgsensor_info.max_frame_length;
    imgsensor.dummy_line = imgsensor.frame_length -
        imgsensor.min_frame_length;
    }
    if (min_framelength_en)
    imgsensor.min_frame_length = imgsensor.frame_length;
    spin_unlock(&imgsensor_drv_lock);

}

static void set_max_framerate_video(UINT16 framerate, kal_bool min_framelength_en)
{
    set_max_framerate(framerate, min_framelength_en);
    set_dummy();
}


static void write_shutter(kal_uint32 shutter)
{
    kal_uint16 realtime_fps = 0;
    // OV Recommend Solution
    // if shutter bigger than frame_length, should extend frame length first
    if (imgsensor.sensor_mode == MSDK_SCENARIO_ID_SLIM_VIDEO) {
        LOG_INF("force-SFR B-shutter = %d\n ", shutter);
        shutter = 1230;
        LOG_INF("force-SFR A-shutter = %d\n ", shutter);
    }
    spin_lock(&imgsensor_drv_lock);
    if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
        imgsensor.frame_length = shutter + imgsensor_info.margin;
    else
        imgsensor.frame_length = imgsensor.min_frame_length;

    if (imgsensor.frame_length > imgsensor_info.max_frame_length)
    imgsensor.frame_length = imgsensor_info.max_frame_length;
    spin_unlock(&imgsensor_drv_lock);

    shutter = (shutter < imgsensor_info.min_shutter) ?
                    imgsensor_info.min_shutter : shutter;
    shutter = (shutter >
                    (imgsensor_info.max_frame_length - imgsensor_info.margin)) ?
                    (imgsensor_info.max_frame_length - imgsensor_info.margin) :
                    shutter;

    //frame_length and shutter should be an even number.
    shutter = (shutter >> 1) << 1;
    imgsensor.frame_length = (imgsensor.frame_length >> 1) << 1;

    if(imgsensor.autoflicker_en == KAL_TRUE){
        realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 /
            imgsensor.frame_length;
        if(realtime_fps >= 297 && realtime_fps <= 305) {
            realtime_fps = 296;
            set_max_framerate(realtime_fps, 0);
        }else if(realtime_fps >= 147 && realtime_fps <= 150) {
            realtime_fps = 146;
            set_max_framerate(realtime_fps, 0);
        }else{
        }
    }

    /*Warning : shutter must be even. Odd might happen Unexpected Results */
    if(!_is_seamless) {
        if (_is_initFlag) {
            write_cmos_sensor(0x3840, imgsensor.frame_length >> 16);
            write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
            write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFE);
            write_cmos_sensor(0x3500, (shutter >> 16) & 0xFF);
            write_cmos_sensor(0x3501, (shutter >> 8) & 0xFF);
            write_cmos_sensor(0x3502, (shutter)  & 0xFE);
            _is_initFlag = 0;
        } else {
            write_cmos_sensor(0x3208, 0x01);
            write_cmos_sensor(0x3840, imgsensor.frame_length >> 16);
            write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
            write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFE);
            write_cmos_sensor(0x3500, (shutter >> 16) & 0xFF);
            write_cmos_sensor(0x3501, (shutter >> 8) & 0xFF);
            write_cmos_sensor(0x3502, (shutter)  & 0xFE);
            write_cmos_sensor(0x3208, 0x11);
            write_cmos_sensor(0x3208, 0xa1);
       }
    } else {
        _i2c_data[_size_to_write++] = 0x3840;
        _i2c_data[_size_to_write++] = imgsensor.frame_length >> 16;
        _i2c_data[_size_to_write++] = 0x380e;
        _i2c_data[_size_to_write++] = imgsensor.frame_length >> 8;
        _i2c_data[_size_to_write++] = 0x380f;
        _i2c_data[_size_to_write++] = imgsensor.frame_length & 0xFE;
        _i2c_data[_size_to_write++] = 0x3500;
        _i2c_data[_size_to_write++] = (shutter >> 16) & 0xFF;
        _i2c_data[_size_to_write++] = 0x3501;
        _i2c_data[_size_to_write++] = (shutter >> 8) & 0xFF;
        _i2c_data[_size_to_write++] = 0x3502;
        _i2c_data[_size_to_write++] = (shutter)  & 0xFE;
    }
    LOG_INF("shutter =%d, framelength =%d, realtime_fps =%d _is_seamless %d\n",
            shutter, imgsensor.frame_length, realtime_fps, _is_seamless);
}

static void set_shutter(kal_uint32 shutter)  //should not be kal_uint16 -- can't reach long exp
{
    unsigned long flags;

    spin_lock_irqsave(&imgsensor_drv_lock, flags);
    imgsensor.shutter = shutter;
    spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
    write_shutter(shutter);
}

static kal_uint16 gain2reg(const kal_uint16 gain)
{
    kal_uint16 iReg = 0x0000;

    //platform 1xgain = 64, sensor driver 1*gain = 0x100
    iReg = gain*256/BASEGAIN;

    if(iReg < 0x100)    //sensor 1xGain
    {
        iReg = 0X100;
    }
    if(iReg > 0x3fff)    //sensor 64xGain
    {
        iReg = 0x3fff;
    }
    return iReg;        /* sensorGlobalGain */
}

static kal_uint16 set_gain(kal_uint16 gain)
{
    kal_uint16 reg_gain, max_gain = imgsensor_info.max_gain;
    unsigned long flags;

    if (gain < imgsensor_info.min_gain || gain > max_gain) {
        LOG_INF("Error gain setting");

        if (gain < imgsensor_info.min_gain)
            gain = imgsensor_info.min_gain;
        else if (gain > max_gain)
            gain = max_gain;
    }

    reg_gain = gain2reg(gain);
    spin_lock_irqsave(&imgsensor_drv_lock, flags);
    imgsensor.gain = reg_gain;
    spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

    LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

    if (imgsensor.sensor_mode == MSDK_SCENARIO_ID_SLIM_VIDEO) {
        reg_gain = 536;
        LOG_INF("force-SFR reg_gain = 0x%x(%d)\n ", reg_gain, reg_gain);
    }

    if (!_is_seamless) {
        if (reg_gain > 0xf00) {
            //15.5xA gain with digital gain
            write_cmos_sensor(0x03508, 0xf);
            write_cmos_sensor(0x03509, 0x00);
            reg_gain = reg_gain*40/150; // calculate 15xAgain+ £¿Dgain
            write_cmos_sensor(0x0350A, (reg_gain >> 10 )& 0xF);
            write_cmos_sensor(0x0350B, (reg_gain >> 2) & 0xFF);
            write_cmos_sensor(0x0350C, (reg_gain & 0x3) << 6);

         } else {
            //1xD gain with Again
            write_cmos_sensor(0x03508, (reg_gain >> 8));
            write_cmos_sensor(0x03509, (reg_gain&0xff));
            write_cmos_sensor(0x0350A, 0x01);
            write_cmos_sensor(0x0350B, 0x00);
            write_cmos_sensor(0x0350C, 0x00);
         }
    } else {
        _i2c_data[_size_to_write++] = 0x03508;
        _i2c_data[_size_to_write++] =  reg_gain >> 8;
        _i2c_data[_size_to_write++] = 0x03509;
        _i2c_data[_size_to_write++] =  reg_gain & 0xff;
    }

    return gain;
}

/* ITD: Modify Dualcam By Jesse 190924 Start */
static void set_shutter_frame_length(kal_uint16 shutter, kal_uint16 target_frame_length, kal_bool auto_extend_en)
{
    spin_lock(&imgsensor_drv_lock);
    if(target_frame_length > 1)
        imgsensor.dummy_line = target_frame_length - imgsensor.frame_length;
    imgsensor.frame_length = imgsensor.frame_length + imgsensor.dummy_line;
    imgsensor.min_frame_length = imgsensor.frame_length;
    spin_unlock(&imgsensor_drv_lock);
    set_shutter(shutter);
}

static void sensor_init(void)
{
    write_cmos_sensor(0x0103, 0x01);//SW Reset, need delay
    mdelay(5);
    LOG_INF("sensor_init start\n");
    ov64b2q_table_write_cmos_sensor(
        addr_data_pair_init_ov64b2q,
        sizeof(addr_data_pair_init_ov64b2q) / sizeof(kal_uint16));
    _is_initFlag = 1;

    write_sensor_crossTalk_data();
    LOG_INF("sensor_init end\n");
}

static void preview_setting(void)
{
    int _length = 0;

    LOG_INF("preview_setting RES_4624x3468_30.00fps\n");
    _length = sizeof(addr_data_pair_preview_ov64b2q) / sizeof(kal_uint16);
    if(!_is_seamless) {
    ov64b2q_table_write_cmos_sensor(
        addr_data_pair_preview_ov64b2q,
        _length);
    } else {
        LOG_INF("%s _is_seamless %d, _size_to_write %d\n",
            __func__, _is_seamless, _size_to_write);

        if (_size_to_write + _length > _I2C_BUF_SIZE) {
            pr_err("_too much i2c data for fast siwtch %d\n",
                _size_to_write + _length);
            return;
        }
        memcpy((void *) (_i2c_data + _size_to_write),
            addr_data_pair_preview_ov64b2q,
            sizeof(addr_data_pair_preview_ov64b2q));
        _size_to_write += _length;
    }

    LOG_INF("preview_setting_end\n");
}


static void capture_setting(kal_uint16 currefps)
{
    int _length = 0;

    LOG_INF("capture_setting currefps = %d\n", currefps);
    _length = sizeof(addr_data_pair_preview_ov64b2q) / sizeof(kal_uint16);
    if(!_is_seamless) {
    ov64b2q_table_write_cmos_sensor(
        addr_data_pair_capture_ov64b2q,
        _length);
    } else {
        LOG_INF("%s _is_seamless %d, _size_to_write %d\n",
            __func__, _is_seamless, _size_to_write);

        if (_size_to_write + _length > _I2C_BUF_SIZE) {
            pr_err("_too much i2c data for fast siwtch %d\n",
                _size_to_write + _length);
            return;
        }
        memcpy((void *) (_i2c_data + _size_to_write),
            addr_data_pair_capture_ov64b2q,
            sizeof(addr_data_pair_capture_ov64b2q));
        _size_to_write += _length;
    }
}

static void normal_video_setting(kal_uint16 currefps)
{
    int _length = 0;

    LOG_INF("normal_video_setting RES_4624x3468_zsl_30fps\n");
    _length = sizeof(addr_data_pair_video_ov64b2q) / sizeof(kal_uint16);
    if(!_is_seamless) {
    ov64b2q_table_write_cmos_sensor(
        addr_data_pair_video_ov64b2q,
        _length);
    } else {
        LOG_INF("%s _is_seamless %d, _size_to_write %d\n",
            __func__, _is_seamless, _size_to_write);

        if (_size_to_write + _length > _I2C_BUF_SIZE) {
            pr_err("_too much i2c data for fast siwtch %d\n",
                _size_to_write + _length);
            return;
        }
        memcpy((void *) (_i2c_data + _size_to_write),
            addr_data_pair_video_ov64b2q,
            sizeof(addr_data_pair_video_ov64b2q));
        _size_to_write += _length;
    }
}

static void hs_video_setting(void)
{
    int _length = 0;

    LOG_INF("hs_video_setting RES_1280x720_160fps\n");
    _length = sizeof(addr_data_pair_hs_video_ov64b2q) / sizeof(kal_uint16);
    if(!_is_seamless) {
        ov64b2q_table_write_cmos_sensor(
        addr_data_pair_hs_video_ov64b2q,
        _length);
    } else {
        LOG_INF("%s _is_seamless %d, _size_to_write %d\n",
            __func__, _is_seamless, _size_to_write);

        if (_size_to_write + _length > _I2C_BUF_SIZE) {
            pr_err("_too much i2c data for fast siwtch %d\n",
                _size_to_write + _length);
            return;
        }
        memcpy((void *) (_i2c_data + _size_to_write),
            addr_data_pair_hs_video_ov64b2q,
            sizeof(addr_data_pair_hs_video_ov64b2q));
        _size_to_write += _length;
    }
}

static void slim_video_setting(void)
{
    int _length = 0;

    LOG_INF("slim_video_setting start\n");
    _length = sizeof(addr_data_pair_slim_video_ov64b2q) / sizeof(kal_uint16);
    if(!_is_seamless) {
        ov64b2q_table_write_cmos_sensor(
        addr_data_pair_slim_video_ov64b2q,
        _length);
    } else {
        LOG_INF("%s _is_seamless %d, _size_to_write %d\n",
            __func__, _is_seamless, _size_to_write);

        if (_size_to_write + _length > _I2C_BUF_SIZE) {
            pr_err("_too much i2c data for fast siwtch %d\n",
                _size_to_write + _length);
            return;
        }
        memcpy((void *) (_i2c_data + _size_to_write),
            addr_data_pair_slim_video_ov64b2q,
            sizeof(addr_data_pair_slim_video_ov64b2q));
        _size_to_write += _length;
    }
    LOG_INF("slim_video_setting end\n");
}

static void custom1_setting(void)
{
    int _length = 0;

    LOG_INF("custom1_setting_start\n");
    _length = sizeof(addr_data_pair_custom1) / sizeof(kal_uint16);
    if(!_is_seamless) {
        ov64b2q_table_write_cmos_sensor(
        addr_data_pair_custom1,
        _length);
    } else {
        LOG_INF("%s _is_seamless %d, _size_to_write %d\n",
            __func__, _is_seamless, _size_to_write);

        if (_size_to_write + _length > _I2C_BUF_SIZE) {
            pr_err("_too much i2c data for fast siwtch %d\n",
                _size_to_write + _length);
            return;
        }
        memcpy((void *) (_i2c_data + _size_to_write),
            addr_data_pair_custom1,
            sizeof(addr_data_pair_custom1));
        _size_to_write += _length;
    }
    LOG_INF("custom1_setting_end\n");
}    /*    custom1_setting  */

static void custom2_setting(void)
{
    int _length = 0;

    LOG_INF("custom2_setting_start\n");
    _length = sizeof(addr_data_pair_custom2) / sizeof(kal_uint16);
    if(!_is_seamless) {
        ov64b2q_table_write_cmos_sensor(
        addr_data_pair_custom2,
        _length);
    } else {
        LOG_INF("%s _is_seamless %d, _size_to_write %d\n",
            __func__, _is_seamless, _size_to_write);

        if (_size_to_write + _length > _I2C_BUF_SIZE) {
            pr_err("_too much i2c data for fast siwtch %d\n",
                _size_to_write + _length);
            return;
        }
        memcpy((void *) (_i2c_data + _size_to_write),
            addr_data_pair_custom2,
            sizeof(addr_data_pair_custom2));
        _size_to_write += _length;
    }
    LOG_INF("custom2_setting_end\n");
}    /*    custom2_setting  */

static void custom3_setting(void)
{
    int _length = 0;

    LOG_INF("custom3_setting start\n");
    ov64b2q_burst_write_cmos_sensor(addr_data_pair_custom3,
        sizeof(addr_data_pair_custom3)/sizeof(kal_uint16));

    _length = sizeof(addr_data_pair_custom3) / sizeof(kal_uint16);
    if(!_is_seamless) {
        ov64b2q_table_write_cmos_sensor(
        addr_data_pair_custom3,
        _length);
    } else {
        LOG_INF("%s _is_seamless %d, _size_to_write %d\n",
            __func__, _is_seamless, _size_to_write);

        if (_size_to_write + _length > _I2C_BUF_SIZE) {
            pr_err("_too much i2c data for fast siwtch %d\n",
                _size_to_write + _length);
            return;
        }
        memcpy((void *) (_i2c_data + _size_to_write),
            addr_data_pair_custom3,
            sizeof(addr_data_pair_custom3));
        _size_to_write += _length;
    }
    LOG_INF("custom3_setting end\n");
}    /*    custom3_setting  */

static void custom4_setting(void)
{
    int _length = 0;

    LOG_INF("custom4_setting_start\n");
    _length = sizeof(addr_data_pair_custom4) / sizeof(kal_uint16);
    if(!_is_seamless) {
        ov64b2q_table_write_cmos_sensor(
        addr_data_pair_custom4,
        _length);
    } else {
        LOG_INF("%s _is_seamless %d, _size_to_write %d\n",
            __func__, _is_seamless, _size_to_write);

        if (_size_to_write + _length > _I2C_BUF_SIZE) {
            pr_err("_too much i2c data for fast siwtch %d\n",
                _size_to_write + _length);
            return;
}
        memcpy((void *) (_i2c_data + _size_to_write),
            addr_data_pair_custom4,
            sizeof(addr_data_pair_custom4));
        _size_to_write += _length;
    }

    LOG_INF("custom4_setting_end\n");

}    /*    custom4_setting  */

static void custom5_setting(void)
{
    int _length = 0;

    LOG_INF("custom5_setting start\n");
    _length = sizeof(addr_data_pair_custom5) / sizeof(kal_uint16);
    if(!_is_seamless) {
        ov64b2q_table_write_cmos_sensor(
        addr_data_pair_custom5,
        _length);
    } else {
        LOG_INF("%s _is_seamless %d, _size_to_write %d\n",
            __func__, _is_seamless, _size_to_write);

        if (_size_to_write + _length > _I2C_BUF_SIZE) {
            pr_err("_too much i2c data for fast siwtch %d\n",
                _size_to_write + _length);
            return;
    }
        memcpy((void *) (_i2c_data + _size_to_write),
            addr_data_pair_custom5,
            sizeof(addr_data_pair_custom5));
        _size_to_write += _length;
    }
    LOG_INF("custom5_setting end\n");
}/*custom5_setting  */

static void custom6_setting(void)
{
    int _length = 0;

    LOG_INF("E\n");
    _length = sizeof(addr_data_pair_custom6) / sizeof(kal_uint16);
    if(!_is_seamless) {
        ov64b2q_table_write_cmos_sensor(
        addr_data_pair_custom6,
        _length);
    } else {
        LOG_INF("%s _is_seamless %d, _size_to_write %d\n",
            __func__, _is_seamless, _size_to_write);

        if (_size_to_write + _length > _I2C_BUF_SIZE) {
            pr_err("_too much i2c data for fast siwtch %d\n",
                _size_to_write + _length);
            return;
    }
        memcpy((void *) (_i2c_data + _size_to_write),
            addr_data_pair_custom6,
            sizeof(addr_data_pair_custom6));
        _size_to_write += _length;
    }
}/*custom6_setting  */
static void custom7_setting(void)
{
    int _length = 0;

    LOG_INF("custom7_setting E\n");
    _length = sizeof(addr_data_pair_custom7) / sizeof(kal_uint16);
    if(!_is_seamless) {
        ov64b2q_table_write_cmos_sensor(
        addr_data_pair_custom7,
        _length);
    } else {
        LOG_INF("%s _is_seamless %d, _size_to_write %d\n",
            __func__, _is_seamless, _size_to_write);

        if (_size_to_write + _length > _I2C_BUF_SIZE) {
            pr_err("_too much i2c data for fast siwtch %d\n",
                _size_to_write + _length);
        return;
    }
    memcpy((void *) (_i2c_data + _size_to_write),
        addr_data_pair_custom7,
        sizeof(addr_data_pair_custom7));
        _size_to_write += _length;
    }
}/*custom7_setting  */

static void custom9_setting(void)
{
    int _length = 0;

    LOG_INF("custom9_setting E\n");
    _length = sizeof(addr_data_pair_custom9) / sizeof(kal_uint16);
    if(!_is_seamless) {
        ov64b2q_table_write_cmos_sensor(
        addr_data_pair_custom9,
        _length);
    } else {
        LOG_INF("%s _is_seamless %d, _size_to_write %d\n",
            __func__, _is_seamless, _size_to_write);

        if (_size_to_write + _length > _I2C_BUF_SIZE) {
            pr_err("_too much i2c data for fast siwtch %d\n",
                _size_to_write + _length);
        return;
    }
    memcpy((void *) (_i2c_data + _size_to_write),
        addr_data_pair_custom9,
        sizeof(addr_data_pair_custom9));
        _size_to_write += _length;
    }
}/*custom9_setting  */

static kal_uint32 return_sensor_id(void)
{
    return ((read_cmos_sensor(0x300a) << 8) | (read_cmos_sensor(0x300b)));
}

static void read_cmos_eeprom_table(kal_uint16 addr, kal_uint8 *table, kal_uint32 size)
{
    char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };
    iReadRegI2C(pusendcmd, 2, (u8 *)table, size, EEPROM_I2C_ADDR);
}

static kal_uint16 read_otp_info(kal_uint8 *data)
{
    kal_uint16 addr = 0x00;
    kal_uint16 left_size = 0;

    left_size = LEFT_SIZE;
    while (left_size > 0) {
        if (left_size >= READ_EEPROM_1K) {
            read_cmos_eeprom_table(addr,data, READ_EEPROM_1K);
            left_size = left_size - READ_EEPROM_1K;
        } else {
            read_cmos_eeprom_table(addr,data, left_size);
            break;
        }
        addr = addr + READ_EEPROM_1K;
        data = data + READ_EEPROM_1K;
    }
    return 0;
}

static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
    kal_uint8 i = 0;
    kal_uint8 retry = 2;

    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
    spin_lock(&imgsensor_drv_lock);
    imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
    gImgEepromInfo.i4CurSensorIdx = 0;
    gImgEepromInfo.i4CurSensorId = imgsensor_info.sensor_id;
    spin_unlock(&imgsensor_drv_lock);
    do {
        *sensor_id = return_sensor_id();
        if (*sensor_id == 0x5664) {
            *sensor_id = imgsensor_info.sensor_id;
            LOG_INF("Read sensor id Success, write id:0x%x, id: 0x%x\n", imgsensor.i2c_write_id, *sensor_id);
            read_EepromQSC();
            if (deviceInfo_register_value == 0x00) {
                Eeprom_DataInit(0, OV64B_SENSOR_ID22277);
                deviceInfo_register_value=0x01;
            }
            if (otp_data[0] == 0) {
                read_otp_info(otp_data);
            } else {
                pr_err("otp data has already read");
            }
            LOG_INF("RENM0_module_id=%d\n",imgsensor_info.module_id);
            return ERROR_NONE;
        }
        LOG_INF("Read sensor id fail, write id:0x%x, id: 0x%x\n", imgsensor.i2c_write_id, *sensor_id);
        retry--;
    } while (retry > 0);
    i++;
    retry = 2;
    }
    if (*sensor_id != imgsensor_info.sensor_id) {
        *sensor_id = 0xFFFFFFFF;
        return ERROR_SENSOR_CONNECT_FAIL;
    }

    return ERROR_NONE;
}

static kal_uint32 open(void)
{
    kal_uint8 i = 0;
    kal_uint8 retry = 1;
    kal_uint32 sensor_id = 0;

    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do {
            sensor_id = return_sensor_id();
            if (sensor_id == 0x5664) {
                sensor_id = imgsensor_info.sensor_id;
                LOG_INF("Read sensor id Success, write id:0x%x, id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
                break;
            }
            LOG_INF("Read sensor id fail, write id:0x%x, id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
            retry--;
        } while (retry > 0);
        i++;
        if (sensor_id == imgsensor_info.sensor_id)
            break;
        retry = 2;
    }
    if (imgsensor_info.sensor_id != sensor_id) {
        return ERROR_SENSORID_READ_FAIL;
    }
    sensor_init();
    write_sensor_OVPD_QSC();
    spin_lock(&imgsensor_drv_lock);
    imgsensor.autoflicker_en = KAL_FALSE;
    imgsensor.sensor_mode = MSDK_SCENARIO_ID_CAMERA_PREVIEW;
    imgsensor.shutter = 0x3D0;
    imgsensor.gain = 0x100;
    imgsensor.pclk = imgsensor_info.pre.pclk;
    imgsensor.frame_length = imgsensor_info.pre.framelength;
    imgsensor.line_length = imgsensor_info.pre.linelength;
    imgsensor.min_frame_length = imgsensor_info.pre.framelength;
    imgsensor.dummy_pixel = 0;
    imgsensor.dummy_line = 0;
    imgsensor.ihdr_en = 0;
    imgsensor.test_pattern = KAL_FALSE;
    imgsensor.current_fps = imgsensor_info.pre.max_framerate;
    spin_unlock(&imgsensor_drv_lock);

    return ERROR_NONE;
}

static kal_uint32 close(void)
{
    ct_qsc_flag = 0;
    crossTalk_flag = 0;

    return ERROR_NONE;
}   /*  close  */

static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
              MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = MSDK_SCENARIO_ID_CAMERA_PREVIEW;
    imgsensor.pclk = imgsensor_info.pre.pclk;
    //imgsensor.video_mode = KAL_FALSE;
    imgsensor.line_length = imgsensor_info.pre.linelength;
    imgsensor.frame_length = imgsensor_info.pre.framelength;
    imgsensor.min_frame_length = imgsensor_info.pre.framelength;
    imgsensor.current_fps = imgsensor.current_fps;
    //imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    preview_setting();
    return ERROR_NONE;
}

static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
          MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG;
    imgsensor.pclk = imgsensor_info.cap.pclk;
    //imgsensor.video_mode = KAL_FALSE;
    imgsensor.line_length = imgsensor_info.cap.linelength;
    imgsensor.frame_length = imgsensor_info.cap.framelength;
    imgsensor.min_frame_length = imgsensor_info.cap.framelength;
    //imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    capture_setting(imgsensor.current_fps);
    return ERROR_NONE;
} /* capture() */

static kal_uint32 normal_video(
            MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
            MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = MSDK_SCENARIO_ID_VIDEO_PREVIEW;
    imgsensor.pclk = imgsensor_info.normal_video.pclk;
    imgsensor.line_length = imgsensor_info.normal_video.linelength;
    imgsensor.frame_length = imgsensor_info.normal_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
    //imgsensor.current_fps = 300;
    //imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    normal_video_setting(imgsensor.current_fps);
    return ERROR_NONE;
}

static kal_uint32 hs_video(
            MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
            MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO;
    imgsensor.pclk = imgsensor_info.hs_video.pclk;
    //imgsensor.video_mode = KAL_TRUE;
    imgsensor.line_length = imgsensor_info.hs_video.linelength;
    imgsensor.frame_length = imgsensor_info.hs_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    //imgsensor.current_fps = 300;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    hs_video_setting();
    return ERROR_NONE;
}

static kal_uint32 slim_video(
            MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
            MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = MSDK_SCENARIO_ID_SLIM_VIDEO;
    imgsensor.pclk = imgsensor_info.slim_video.pclk;
    //imgsensor.video_mode = KAL_TRUE;
    imgsensor.line_length = imgsensor_info.slim_video.linelength;
    imgsensor.frame_length = imgsensor_info.slim_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    //imgsensor.current_fps = 300;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    slim_video_setting();
    return ERROR_NONE;
}

/* ITD: Modify Dualcam By Jesse 190924 Start */
static kal_uint32 Custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = MSDK_SCENARIO_ID_CUSTOM1;
    imgsensor.pclk = imgsensor_info.custom1.pclk;
    //imgsensor.video_mode = KAL_FALSE;
    imgsensor.line_length = imgsensor_info.custom1.linelength;
    imgsensor.frame_length = imgsensor_info.custom1.framelength;
    imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    custom1_setting();
    return ERROR_NONE;
}   /*  Custom1   */

static kal_uint32 Custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = MSDK_SCENARIO_ID_CUSTOM2;
    imgsensor.pclk = imgsensor_info.custom2.pclk;
    //imgsensor.video_mode = KAL_FALSE;
    imgsensor.line_length = imgsensor_info.custom2.linelength;
    imgsensor.frame_length = imgsensor_info.custom2.framelength;
    imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    custom2_setting();
    return ERROR_NONE;
}   /*  Custom2   */

static kal_uint32 Custom3(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                    MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = MSDK_SCENARIO_ID_CUSTOM3;
    imgsensor.pclk = imgsensor_info.custom3.pclk;
    //imgsensor.video_mode = KAL_FALSE;
    imgsensor.line_length = imgsensor_info.custom3.linelength;
    imgsensor.frame_length = imgsensor_info.custom3.framelength;
    imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    write_sensor_CT_QSC();
    custom3_setting();
    return ERROR_NONE;
}   /*  Custom3*/


static kal_uint32 Custom4(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                    MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = MSDK_SCENARIO_ID_CUSTOM4;
    imgsensor.pclk = imgsensor_info.custom4.pclk;
    //imgsensor.video_mode = KAL_FALSE;
    imgsensor.line_length = imgsensor_info.custom4.linelength;
    imgsensor.frame_length = imgsensor_info.custom4.framelength;
    imgsensor.min_frame_length = imgsensor_info.custom4.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    custom4_setting();
    return ERROR_NONE;
}   /*  Custom4    */

static kal_uint32 Custom5(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                    MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = MSDK_SCENARIO_ID_CUSTOM5;
    imgsensor.pclk = imgsensor_info.custom5.pclk;
    //imgsensor.video_mode = KAL_FALSE;
    imgsensor.line_length = imgsensor_info.custom5.linelength;
    imgsensor.frame_length = imgsensor_info.custom5.framelength;
    imgsensor.min_frame_length = imgsensor_info.custom5.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    write_sensor_CT_QSC();
    custom5_setting();
    return ERROR_NONE;
}/*    Custom5 */

static kal_uint32 Custom6(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                    MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("Custom6 E\n");
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = MSDK_SCENARIO_ID_CUSTOM6;
    imgsensor.pclk = imgsensor_info.custom6.pclk;
    imgsensor.line_length = imgsensor_info.custom6.linelength;
    imgsensor.frame_length = imgsensor_info.custom6.framelength;
    imgsensor.min_frame_length = imgsensor_info.custom6.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    custom6_setting();
    return ERROR_NONE;
}/*Custom6*/

static kal_uint32 Custom7(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                    MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("Custom7\n");
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = MSDK_SCENARIO_ID_CUSTOM7;
    imgsensor.pclk = imgsensor_info.custom7.pclk;
    imgsensor.line_length = imgsensor_info.custom7.linelength;
    imgsensor.frame_length = imgsensor_info.custom7.framelength;
    imgsensor.min_frame_length = imgsensor_info.custom7.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    custom7_setting();
    return ERROR_NONE;
}/*Custom7*/

static kal_uint32 Custom8(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                    MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("custom8\n");
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = MSDK_SCENARIO_ID_CUSTOM8;
    imgsensor.pclk = imgsensor_info.custom8.pclk;
    imgsensor.line_length = imgsensor_info.custom8.linelength;
    imgsensor.frame_length = imgsensor_info.custom8.framelength;
    imgsensor.min_frame_length = imgsensor_info.custom8.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    custom7_setting();
    return ERROR_NONE;
}/*Custom8*/

static kal_uint32 Custom9(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                    MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("custom9\n");
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = MSDK_SCENARIO_ID_CUSTOM9;
    imgsensor.pclk = imgsensor_info.custom9.pclk;
    imgsensor.line_length = imgsensor_info.custom9.linelength;
    imgsensor.frame_length = imgsensor_info.custom9.framelength;
    imgsensor.min_frame_length = imgsensor_info.custom9.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    custom9_setting();
    return ERROR_NONE;
}/*custom9*/

static kal_uint32 Custom10(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                    MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("custom10\n");
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = MSDK_SCENARIO_ID_CUSTOM10;
    imgsensor.pclk = imgsensor_info.custom10.pclk;
    imgsensor.line_length = imgsensor_info.custom10.linelength;
    imgsensor.frame_length = imgsensor_info.custom10.framelength;
    imgsensor.min_frame_length = imgsensor_info.custom10.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    custom9_setting();
    return ERROR_NONE;
}/*Custom10*/

static kal_uint32 get_resolution(
        MSDK_SENSOR_RESOLUTION_INFO_STRUCT * sensor_resolution)
{
    sensor_resolution->SensorFullWidth =
        imgsensor_info.cap.grabwindow_width;
    sensor_resolution->SensorFullHeight =
        imgsensor_info.cap.grabwindow_height;

    sensor_resolution->SensorPreviewWidth =
        imgsensor_info.pre.grabwindow_width;
    sensor_resolution->SensorPreviewHeight =
        imgsensor_info.pre.grabwindow_height;

    sensor_resolution->SensorVideoWidth =
        imgsensor_info.normal_video.grabwindow_width;
    sensor_resolution->SensorVideoHeight =
        imgsensor_info.normal_video.grabwindow_height;


    sensor_resolution->SensorHighSpeedVideoWidth =
        imgsensor_info.hs_video.grabwindow_width;
    sensor_resolution->SensorHighSpeedVideoHeight =
        imgsensor_info.hs_video.grabwindow_height;

    sensor_resolution->SensorSlimVideoWidth =
        imgsensor_info.slim_video.grabwindow_width;
    sensor_resolution->SensorSlimVideoHeight =
        imgsensor_info.slim_video.grabwindow_height;

/* ITD: Modify Dualcam By Jesse 190924 Start */
    sensor_resolution->SensorCustom1Width =
        imgsensor_info.custom1.grabwindow_width;
    sensor_resolution->SensorCustom1Height =
        imgsensor_info.custom1.grabwindow_height;

    sensor_resolution->SensorCustom2Width =
        imgsensor_info.custom2.grabwindow_width;
    sensor_resolution->SensorCustom2Height =
        imgsensor_info.custom2.grabwindow_height;

    sensor_resolution->SensorCustom3Width =
        imgsensor_info.custom3.grabwindow_width;
    sensor_resolution->SensorCustom3Height =
        imgsensor_info.custom3.grabwindow_height;

    sensor_resolution->SensorCustom4Width =
        imgsensor_info.custom4.grabwindow_width;
    sensor_resolution->SensorCustom4Height =
        imgsensor_info.custom4.grabwindow_height;

    sensor_resolution->SensorCustom5Width =
        imgsensor_info.custom5.grabwindow_width;
    sensor_resolution->SensorCustom5Height =
        imgsensor_info.custom5.grabwindow_height;

    sensor_resolution->SensorCustom6Width =
        imgsensor_info.custom6.grabwindow_width;
    sensor_resolution->SensorCustom6Height =
        imgsensor_info.custom6.grabwindow_height;

    sensor_resolution->SensorCustom7Width =
        imgsensor_info.custom7.grabwindow_width;
    sensor_resolution->SensorCustom7Height =
        imgsensor_info.custom7.grabwindow_height;

    sensor_resolution->SensorCustom8Width =
        imgsensor_info.custom8.grabwindow_width;
    sensor_resolution->SensorCustom8Height =
        imgsensor_info.custom8.grabwindow_height;

    sensor_resolution->SensorCustom9Width =
        imgsensor_info.custom9.grabwindow_width;
    sensor_resolution->SensorCustom9Height =
        imgsensor_info.custom9.grabwindow_height;
    sensor_resolution->SensorCustom10Width =
        imgsensor_info.custom10.grabwindow_width;
    sensor_resolution->SensorCustom10Height =
        imgsensor_info.custom10.grabwindow_height;

    return ERROR_NONE;
}   /*  get_resolution  */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
              MSDK_SENSOR_INFO_STRUCT *sensor_info,
              MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    if (scenario_id == 0)
    LOG_INF("scenario_id = %d\n", scenario_id);

    sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorInterruptDelayLines = 4; /* not use */
    sensor_info->SensorResetActiveHigh = FALSE; /* not use */
    sensor_info->SensorResetDelayCount = 5; /* not use */

    sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
    sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
    sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
    sensor_info->SensorOutputDataFormat =
        imgsensor_info.sensor_output_dataformat;

    sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
    sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
    sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
    sensor_info->HighSpeedVideoDelayFrame =
        imgsensor_info.hs_video_delay_frame;
    sensor_info->SlimVideoDelayFrame =
        imgsensor_info.slim_video_delay_frame;

    sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
    sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;
    sensor_info->Custom3DelayFrame = imgsensor_info.custom3_delay_frame;
    sensor_info->Custom4DelayFrame = imgsensor_info.custom4_delay_frame;
    sensor_info->Custom5DelayFrame = imgsensor_info.custom5_delay_frame;
    sensor_info->Custom6DelayFrame = imgsensor_info.custom6_delay_frame;
    sensor_info->Custom7DelayFrame = imgsensor_info.custom7_delay_frame;
    sensor_info->Custom8DelayFrame = imgsensor_info.custom8_delay_frame;
    sensor_info->Custom9DelayFrame = imgsensor_info.custom9_delay_frame;
    sensor_info->Custom10DelayFrame = imgsensor_info.custom10_delay_frame;

    sensor_info->SensorMasterClockSwitch = 0; /* not use */
    sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;
/* The frame of setting shutter default 0 for TG int */
    sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
    /* The frame of setting sensor gain */
    sensor_info->AESensorGainDelayFrame =
        imgsensor_info.ae_sensor_gain_delay_frame;
    sensor_info->AEISPGainDelayFrame =
        imgsensor_info.ae_ispGain_delay_frame;
    sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
    sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
    sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
    sensor_info->PDAF_Support = 2;

    //sensor_info->HDR_Support = 0; /*0: NO HDR, 1: iHDR, 2:mvHDR, 3:zHDR*/
    sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
    sensor_info->SensorClockFreq = imgsensor_info.mclk;
    sensor_info->SensorClockDividCount = 3; /* not use */
    sensor_info->SensorClockRisingCount = 0;
    sensor_info->SensorClockFallingCount = 2; /* not use */
    sensor_info->SensorPixelClockCount = 3; /* not use */
    sensor_info->SensorDataLatchCount = 2; /* not use */

    sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
    sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
    sensor_info->SensorWidthSampling = 0;  // 0 is default 1x
    sensor_info->SensorHightSampling = 0;   // 0 is default 1x
    sensor_info->SensorPacketECCOrder = 1;

    switch (scenario_id) {
    case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.pre.mipi_data_lp2hs_settle_dc;

    break;
    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;

        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.cap.mipi_data_lp2hs_settle_dc;

    break;
    case MSDK_SCENARIO_ID_VIDEO_PREVIEW:

        sensor_info->SensorGrabStartX =
            imgsensor_info.normal_video.startx;
        sensor_info->SensorGrabStartY =
            imgsensor_info.normal_video.starty;

        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;

    break;
    case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;

        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;
    break;
    case MSDK_SCENARIO_ID_SLIM_VIDEO:
        sensor_info->SensorGrabStartX =
            imgsensor_info.slim_video.startx;
        sensor_info->SensorGrabStartY =
            imgsensor_info.slim_video.starty;

        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;
    break;
/* ITD: Modify Dualcam By Jesse 190924 Start */
    case MSDK_SCENARIO_ID_CUSTOM1:
        sensor_info->SensorGrabStartX =
            imgsensor_info.custom1.startx;
        sensor_info->SensorGrabStartY =
            imgsensor_info.custom1.starty;
        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
    break;
    case MSDK_SCENARIO_ID_CUSTOM2:
        sensor_info->SensorGrabStartX =
            imgsensor_info.custom2.startx;
        sensor_info->SensorGrabStartY =
            imgsensor_info.custom2.starty;
        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.custom2.mipi_data_lp2hs_settle_dc;
    break;
    case MSDK_SCENARIO_ID_CUSTOM3:
        sensor_info->SensorGrabStartX =
            imgsensor_info.custom3.startx;
        sensor_info->SensorGrabStartY =
            imgsensor_info.custom3.starty;
        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.custom3.mipi_data_lp2hs_settle_dc;
    break;

    case MSDK_SCENARIO_ID_CUSTOM4:
        sensor_info->SensorGrabStartX =
            imgsensor_info.custom4.startx;
        sensor_info->SensorGrabStartY =
            imgsensor_info.custom4.starty;
        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.custom4.mipi_data_lp2hs_settle_dc;
    break;
    case MSDK_SCENARIO_ID_CUSTOM5:
        sensor_info->SensorGrabStartX =
            imgsensor_info.custom5.startx;
        sensor_info->SensorGrabStartY =
            imgsensor_info.custom5.starty;
        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.custom5.mipi_data_lp2hs_settle_dc;
    break;
    case MSDK_SCENARIO_ID_CUSTOM6:
        sensor_info->SensorGrabStartX =
            imgsensor_info.custom6.startx;
        sensor_info->SensorGrabStartY =
            imgsensor_info.custom6.starty;
        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.custom6.mipi_data_lp2hs_settle_dc;
    break;
    case MSDK_SCENARIO_ID_CUSTOM7:
        sensor_info->SensorGrabStartX =
            imgsensor_info.custom7.startx;
        sensor_info->SensorGrabStartY =
            imgsensor_info.custom7.starty;
        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.custom7.mipi_data_lp2hs_settle_dc;
    break;
    case MSDK_SCENARIO_ID_CUSTOM8:
        sensor_info->SensorGrabStartX =
            imgsensor_info.custom8.startx;
        sensor_info->SensorGrabStartY =
            imgsensor_info.custom8.starty;
        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.custom8.mipi_data_lp2hs_settle_dc;
    break;
    case MSDK_SCENARIO_ID_CUSTOM9:
        sensor_info->SensorGrabStartX =
            imgsensor_info.custom9.startx;
        sensor_info->SensorGrabStartY =
            imgsensor_info.custom9.starty;
        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.custom9.mipi_data_lp2hs_settle_dc;
    break;
    case MSDK_SCENARIO_ID_CUSTOM10:
        sensor_info->SensorGrabStartX =
            imgsensor_info.custom10.startx;
        sensor_info->SensorGrabStartY =
            imgsensor_info.custom10.starty;
        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.custom10.mipi_data_lp2hs_settle_dc;
    break;
    default:
        sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
    break;
    }

    return ERROR_NONE;
}   /*  get_info  */


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
            MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
            MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    spin_lock(&imgsensor_drv_lock);
    imgsensor.current_scenario_id = scenario_id;
    spin_unlock(&imgsensor_drv_lock);

    switch (scenario_id) {
    case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        preview(image_window, sensor_config_data);
    break;
    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        capture(image_window, sensor_config_data);
    break;
    case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        normal_video(image_window, sensor_config_data);
    break;
    case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        hs_video(image_window, sensor_config_data);
    break;
    case MSDK_SCENARIO_ID_SLIM_VIDEO:
        slim_video(image_window, sensor_config_data);
    break;
/* ITD: Modify Dualcam By Jesse 190924 Start */
    case MSDK_SCENARIO_ID_CUSTOM1:
        Custom1(image_window, sensor_config_data);
    break;
    case MSDK_SCENARIO_ID_CUSTOM2:
        Custom2(image_window, sensor_config_data);
    break;
    case MSDK_SCENARIO_ID_CUSTOM3:
        Custom3(image_window, sensor_config_data);
    break;
    case MSDK_SCENARIO_ID_CUSTOM4:
        Custom4(image_window, sensor_config_data);
    break;
    case MSDK_SCENARIO_ID_CUSTOM5:
        Custom5(image_window, sensor_config_data);
    break;
    case MSDK_SCENARIO_ID_CUSTOM6:
        Custom6(image_window, sensor_config_data);
    break;
    case MSDK_SCENARIO_ID_CUSTOM7:
        Custom7(image_window, sensor_config_data);
    break;
    case MSDK_SCENARIO_ID_CUSTOM8:
        Custom8(image_window, sensor_config_data);
    break;
    case MSDK_SCENARIO_ID_CUSTOM9:
        Custom9(image_window, sensor_config_data);
    break;
    case MSDK_SCENARIO_ID_CUSTOM10:
        Custom10(image_window, sensor_config_data);
    break;
/* ITD: Modify Dualcam By Jesse 190924 End */
    default:
        LOG_INF("Error ScenarioId setting");
        preview(image_window, sensor_config_data);
    return ERROR_INVALID_SCENARIO_ID;
    }

    return ERROR_NONE;
}   /* control() */

static kal_uint32 set_video_mode(UINT16 framerate)
{
    // SetVideoMode Function should fix framerate
    if (framerate == 0)
    // Dynamic frame rate
    return ERROR_NONE;

    spin_lock(&imgsensor_drv_lock);
    if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
    imgsensor.current_fps = 296;
    else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
    imgsensor.current_fps = 146;
    else
    imgsensor.current_fps = framerate;
    spin_unlock(&imgsensor_drv_lock);

    set_max_framerate_video(imgsensor.current_fps, 1);

    return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable,
            UINT16 framerate)
{
    LOG_INF("enable = %d, framerate = %d\n",
        enable, framerate);

    spin_lock(&imgsensor_drv_lock);
    if (enable) //enable auto flicker
    imgsensor.autoflicker_en = KAL_TRUE;
    else //Cancel Auto flick
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);

    return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(
            enum MSDK_SCENARIO_ID_ENUM scenario_id,
            MUINT32 framerate)
{
    kal_uint32 frameHeight;

    LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

    if (framerate == 0)
        return ERROR_NONE;

    switch (scenario_id) {
    case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        frameHeight = imgsensor_info.pre.pclk / framerate * 10 /
            imgsensor_info.pre.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line =
            (frameHeight > imgsensor_info.pre.framelength) ?
            (frameHeight - imgsensor_info.pre.framelength):0;
        imgsensor.frame_length = imgsensor_info.pre.framelength +
            imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
    break;
    case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        if (framerate == 0)
            return ERROR_NONE;
        frameHeight = imgsensor_info.normal_video.pclk / framerate * 10 /
                imgsensor_info.normal_video.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line = (frameHeight >
            imgsensor_info.normal_video.framelength) ?
        (frameHeight - imgsensor_info.normal_video.framelength):0;
        imgsensor.frame_length = imgsensor_info.normal_video.framelength +
            imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
    break;
    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        frameHeight = imgsensor_info.cap.pclk / framerate * 10 /
            imgsensor_info.cap.linelength;
        spin_lock(&imgsensor_drv_lock);

        imgsensor.dummy_line =
            (frameHeight > imgsensor_info.cap.framelength) ?
            (frameHeight - imgsensor_info.cap.framelength):0;
        imgsensor.frame_length = imgsensor_info.cap.framelength +
            imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
    break;
    case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        frameHeight = imgsensor_info.hs_video.pclk / framerate * 10 /
            imgsensor_info.hs_video.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line =
            (frameHeight > imgsensor_info.hs_video.framelength) ?
            (frameHeight - imgsensor_info.hs_video.framelength):0;
        imgsensor.frame_length = imgsensor_info.hs_video.framelength +
            imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
    break;
    case MSDK_SCENARIO_ID_SLIM_VIDEO:
        frameHeight = imgsensor_info.slim_video.pclk / framerate * 10 /
            imgsensor_info.slim_video.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line = (frameHeight >
            imgsensor_info.slim_video.framelength) ?
            (frameHeight - imgsensor_info.slim_video.framelength):0;
        imgsensor.frame_length = imgsensor_info.slim_video.framelength +
            imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
    break;
/* ITD: Modify Dualcam By Jesse 190924 Start */
    case MSDK_SCENARIO_ID_CUSTOM1:
        frameHeight = imgsensor_info.custom1.pclk / framerate * 10 /
            imgsensor_info.custom1.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line = (frameHeight >
            imgsensor_info.custom1.framelength) ?
            (frameHeight - imgsensor_info.custom1.framelength):0;
        imgsensor.frame_length = imgsensor_info.custom1.framelength +
            imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
    break;
    case MSDK_SCENARIO_ID_CUSTOM2:
        frameHeight = imgsensor_info.custom2.pclk / framerate * 10 /
            imgsensor_info.custom2.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line = (frameHeight >
            imgsensor_info.custom2.framelength) ?
            (frameHeight - imgsensor_info.custom2.framelength):0;
        imgsensor.frame_length = imgsensor_info.custom2.framelength +
            imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
    break;
    case MSDK_SCENARIO_ID_CUSTOM3:
        frameHeight = imgsensor_info.custom3.pclk / framerate * 10 /
            imgsensor_info.custom3.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line = (frameHeight >
            imgsensor_info.custom3.framelength) ?
            (frameHeight - imgsensor_info.custom3.framelength):0;
        imgsensor.frame_length = imgsensor_info.custom3.framelength +
            imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
    break;
    case MSDK_SCENARIO_ID_CUSTOM4:
        frameHeight = imgsensor_info.custom4.pclk / framerate * 10 /
            imgsensor_info.custom4.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line = (frameHeight >
            imgsensor_info.custom4.framelength) ?
            (frameHeight - imgsensor_info.custom4.framelength):0;
        imgsensor.frame_length = imgsensor_info.custom4.framelength +
            imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
    break;
    case MSDK_SCENARIO_ID_CUSTOM5:
        frameHeight = imgsensor_info.custom5.pclk / framerate * 10 /
            imgsensor_info.custom5.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line = (frameHeight >
            imgsensor_info.custom5.framelength) ?
            (frameHeight - imgsensor_info.custom5.framelength):0;
        imgsensor.frame_length = imgsensor_info.custom5.framelength +
            imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
    break;
    case MSDK_SCENARIO_ID_CUSTOM6:
        frameHeight = imgsensor_info.custom6.pclk / framerate * 10 /
            imgsensor_info.custom6.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line = (frameHeight >
            imgsensor_info.custom6.framelength) ?
            (frameHeight - imgsensor_info.custom6.framelength):0;
        imgsensor.frame_length = imgsensor_info.custom6.framelength +
            imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
    break;
    case MSDK_SCENARIO_ID_CUSTOM7:
        frameHeight = imgsensor_info.custom7.pclk / framerate * 10 /
            imgsensor_info.custom7.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line = (frameHeight >
            imgsensor_info.custom7.framelength) ?
            (frameHeight - imgsensor_info.custom7.framelength):0;
        imgsensor.frame_length = imgsensor_info.custom7.framelength +
            imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
    break;
    case MSDK_SCENARIO_ID_CUSTOM8:
        frameHeight = imgsensor_info.custom8.pclk / framerate * 10 /
            imgsensor_info.custom8.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line = (frameHeight >
            imgsensor_info.custom8.framelength) ?
            (frameHeight - imgsensor_info.custom8.framelength):0;
        imgsensor.frame_length = imgsensor_info.custom8.framelength +
            imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
    break;
    case MSDK_SCENARIO_ID_CUSTOM9:
        frameHeight = imgsensor_info.custom9.pclk / framerate * 10 /
            imgsensor_info.custom9.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line = (frameHeight >
            imgsensor_info.custom9.framelength) ?
            (frameHeight - imgsensor_info.custom9.framelength):0;
        imgsensor.frame_length = imgsensor_info.custom9.framelength +
            imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
    break;
    case MSDK_SCENARIO_ID_CUSTOM10:
        frameHeight = imgsensor_info.custom10.pclk / framerate * 10 /
            imgsensor_info.custom10.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line = (frameHeight >
            imgsensor_info.custom10.framelength) ?
            (frameHeight - imgsensor_info.custom10.framelength):0;
        imgsensor.frame_length = imgsensor_info.custom10.framelength +
            imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
    break;
    default:  //coding with  preview scenario by default
        frameHeight = imgsensor_info.pre.pclk / framerate * 10 /
            imgsensor_info.pre.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line = (frameHeight >
            imgsensor_info.pre.framelength) ?
            (frameHeight - imgsensor_info.pre.framelength):0;
        imgsensor.frame_length = imgsensor_info.pre.framelength +
            imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
    break;
    }
    return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(
            enum MSDK_SCENARIO_ID_ENUM scenario_id,
            MUINT32 *framerate)
{
    if (scenario_id == 0)
    LOG_INF("[3058]scenario_id = %d\n", scenario_id);

    switch (scenario_id) {
    case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        *framerate = imgsensor_info.pre.max_framerate;
    break;
    case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        *framerate = imgsensor_info.normal_video.max_framerate;
    break;
    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        *framerate = imgsensor_info.cap.max_framerate;
    break;
    case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        *framerate = imgsensor_info.hs_video.max_framerate;
    break;
    case MSDK_SCENARIO_ID_SLIM_VIDEO:
        *framerate = imgsensor_info.slim_video.max_framerate;
    break;
/* ITD: Modify Dualcam By Jesse 190924 Start */
    case MSDK_SCENARIO_ID_CUSTOM1:
        *framerate = imgsensor_info.custom1.max_framerate;
    break;
    case MSDK_SCENARIO_ID_CUSTOM2:
        *framerate = imgsensor_info.custom2.max_framerate;
    break;
    case MSDK_SCENARIO_ID_CUSTOM3:
        *framerate = imgsensor_info.custom3.max_framerate;
    break;
    case MSDK_SCENARIO_ID_CUSTOM4:
        *framerate = imgsensor_info.custom4.max_framerate;
    break;
    case MSDK_SCENARIO_ID_CUSTOM5:
        *framerate = imgsensor_info.custom5.max_framerate;
    break;
    case MSDK_SCENARIO_ID_CUSTOM6:
        *framerate = imgsensor_info.custom6.max_framerate;
    break;
    case MSDK_SCENARIO_ID_CUSTOM7:
        *framerate = imgsensor_info.custom7.max_framerate;
    break;
    case MSDK_SCENARIO_ID_CUSTOM8:
        *framerate = imgsensor_info.custom8.max_framerate;
    break;
    case MSDK_SCENARIO_ID_CUSTOM9:
        *framerate = imgsensor_info.custom9.max_framerate;
    break;
    case MSDK_SCENARIO_ID_CUSTOM10:
        *framerate = imgsensor_info.custom10.max_framerate;
    break;
    default:
    break;
    }

    return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
    LOG_INF("Jesse+ enable: %d\n", enable);
    if (enable) { // for solid color

        write_cmos_sensor(0x3208, 0x00);
        write_cmos_sensor(0x430b, 0x00);
        write_cmos_sensor(0x430c, 0x00);
        write_cmos_sensor(0x4310, 0x00);
        write_cmos_sensor(0x4311, 0x00);
        write_cmos_sensor(0x3208, 0x10);
        write_cmos_sensor(0x3208, 0xa0);
    } else {

        write_cmos_sensor(0x3208, 0x00);
        write_cmos_sensor(0x430b, 0xff);
        write_cmos_sensor(0x430c, 0xff);
        write_cmos_sensor(0x4310, 0xff);
        write_cmos_sensor(0x4311, 0xff);
        write_cmos_sensor(0x3208, 0x10);
        write_cmos_sensor(0x3208, 0xa0);
    }

    spin_lock(&imgsensor_drv_lock);
    imgsensor.test_pattern = enable;
    spin_unlock(&imgsensor_drv_lock);
    return ERROR_NONE;
}

static kal_uint32 get_sensor_temperature(void)
{
    UINT32 temperature = 0;
    INT32 temperature_convert = 0;

    /*TEMP_SEN_CTL */
    write_cmos_sensor(0x4d12, 0x01);
    temperature = (read_cmos_sensor(0x4d13) << 8) |
        read_cmos_sensor(0x4d13);
    if (temperature < 0xc000)
        temperature_convert = temperature / 256;
    else
        temperature_convert = 192 - temperature / 256;

    if (temperature_convert < -64) {
        //LOG_INF("Temperature too low: %d\n",
                //temperature_convert);
        temperature_convert = -64;
    }

    return 20;
    //return temperature_convert;
}

static kal_uint32 seamless_switch(enum MSDK_SCENARIO_ID_ENUM scenario_id,
    kal_uint32 shutter, kal_uint32 gain,
    kal_uint32 shutter_2ndframe, kal_uint32 gain_2ndframe)
{
    int _length = 0;
//    int k = 0;
    _is_seamless = true;
    memset(_i2c_data, 0x0, sizeof(_i2c_data));
    _size_to_write = 0;

    /*pr_err("seamless_switch %d, %d, %d, %d, %d sizeof(_i2c_data) %d\n",
        scenario_id, shutter, gain, shutter_2ndframe, gain_2ndframe, sizeof(_i2c_data)); */

    _length = sizeof(addr_data_pair_seamless_switch_step1_ov64b2q) / sizeof(kal_uint16);

    if (_length> _I2C_BUF_SIZE) {
        pr_err("_too much i2c data for fast siwtch\n");
        return ERROR_NONE;
}

    memcpy((void *)(_i2c_data + _size_to_write),
        addr_data_pair_seamless_switch_step1_ov64b2q,
        sizeof(addr_data_pair_seamless_switch_step1_ov64b2q));
    _size_to_write += _length;



    control(scenario_id, NULL, NULL);
    if(shutter != 0)
        set_shutter(shutter);
    if(gain != 0)
        set_gain(gain);

    _length = sizeof(addr_data_pair_seamless_switch_step2_ov64b2q) / sizeof(kal_uint16);

    if (_size_to_write + _length > _I2C_BUF_SIZE) {
        pr_err("_too much i2c data for fast siwtch\n");
        return ERROR_NONE;
    }

    memcpy((void *)(_i2c_data + _size_to_write),
        addr_data_pair_seamless_switch_step2_ov64b2q,
        sizeof(addr_data_pair_seamless_switch_step2_ov64b2q));
    _size_to_write += _length;

    if(shutter_2ndframe != 0)
        set_shutter(shutter_2ndframe);
    if(gain_2ndframe != 0)
        set_gain(gain_2ndframe);

    _length = sizeof(addr_data_pair_seamless_switch_step3_ov64b2q) / sizeof(kal_uint16);
    if (_size_to_write + _length > _I2C_BUF_SIZE) {
        pr_err("_too much i2c data for fast siwtch\n");
        return ERROR_NONE;
}
    memcpy((void *)(_i2c_data + _size_to_write),
        addr_data_pair_seamless_switch_step3_ov64b2q,
        sizeof(addr_data_pair_seamless_switch_step3_ov64b2q));
    _size_to_write += _length;

    LOG_INF("%s _is_seamless %d, _size_to_write %d\n",
            __func__, _is_seamless, _size_to_write);
#if 0
    for (k = 0; k <_size_to_write; k+=2) {
        LOG_INF( "k = %d, 0x%x , 0x%x \n", k,  _i2c_data[k], _i2c_data[k+1]);
    }
#endif

    ov64b2q_table_write_cmos_sensor(
        _i2c_data,
        _size_to_write);

#if 0
    LOG_INF("===========================================\n");

    for (k = 0; k <_size_to_write; k+=2) {
        LOG_INF( "k = %d, 0x%x , 0x%x \n", k,  _i2c_data[k], read_cmos_sensor(_i2c_data[k]));
    }
#endif
    _is_seamless = false;
    pr_err("exit\n");
    return ERROR_NONE;
}

static kal_uint32 streaming_control(kal_bool enable)
{
    LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
    if (enable)
        write_cmos_sensor(0x0100, 0x01);
    else
        write_cmos_sensor(0x0100, 0x00);
    return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
            UINT8 *feature_para, UINT32 *feature_para_len)
{
    UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
    UINT16 *feature_data_16 = (UINT16 *) feature_para;
    UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
    UINT32 *feature_data_32 = (UINT32 *) feature_para;
    INT32 *feature_return_para_i32 = (INT32 *) feature_para;
    unsigned long long *feature_data = (unsigned long long *) feature_para;

    struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
    #if 1
    UINT32 *pAeCtrls = NULL;
    UINT32 *pScenarios = NULL;
    #endif
    struct SENSOR_VC_INFO_STRUCT *pvcinfo;
    MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
        (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

    struct SET_PD_BLOCK_INFO_T *PDAFinfo;

    if (!((feature_id == 3040) || (feature_id == 3058)))
        LOG_INF("feature_id = %d\n", feature_id);
    switch (feature_id) {
        case SENSOR_FEATURE_SET_SENSOR_OTP:
        {
            enum IMGSENSOR_RETURN ret = IMGSENSOR_RETURN_SUCCESS;
            LOG_INF("SENSOR_FEATURE_SET_SENSOR_OTP\n");
            ret = Eeprom_CallWriteService((ACDK_SENSOR_ENGMODE_STEREO_STRUCT *)(feature_para));
            if (ret == IMGSENSOR_RETURN_SUCCESS)
                return ERROR_NONE;
            else
                return ERROR_MSDK_IS_ACTIVATED;
        }
        break;
    case SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS:
        pScenarios = (MUINT32 *)((uintptr_t)(*(feature_data+1)));
        switch (*feature_data) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            *pScenarios = MSDK_SCENARIO_ID_CUSTOM5;
            break;
        case MSDK_SCENARIO_ID_CUSTOM5:
            *pScenarios = MSDK_SCENARIO_ID_CAMERA_PREVIEW;
            break;
        default:
            *pScenarios = 0xff;
            break;
        }
        LOG_INF("SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS %llu %d\n", *feature_data, *pScenarios);
        break;
    case SENSOR_FEATURE_SEAMLESS_SWITCH:
        pAeCtrls = (MUINT32 *)((uintptr_t)(*(feature_data+1)));
        if (pAeCtrls)
            seamless_switch((*feature_data),*pAeCtrls,*(pAeCtrls+1),*(pAeCtrls+4),*(pAeCtrls+5));
        else
            seamless_switch((*feature_data), 0, 0, 0, 0);
        break;
    case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
        *(feature_data + 1) = imgsensor_info.min_gain;
        *(feature_data + 2) = imgsensor_info.max_gain;
        break;
    case SENSOR_FEATURE_GET_BASE_GAIN_ISO_AND_STEP:
        *(feature_data + 0) = imgsensor_info.min_gain_iso;
        *(feature_data + 1) = imgsensor_info.gain_step;
        *(feature_data + 2) = imgsensor_info.gain_type;
        break;
    case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:
    *(feature_data + 1) = imgsensor_info.min_shutter;
        switch (*feature_data) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        case MSDK_SCENARIO_ID_CUSTOM1:
        case MSDK_SCENARIO_ID_CUSTOM2:
        case MSDK_SCENARIO_ID_CUSTOM4:
        case MSDK_SCENARIO_ID_CUSTOM5:
        case MSDK_SCENARIO_ID_CUSTOM6:
        case MSDK_SCENARIO_ID_CUSTOM7:
        case MSDK_SCENARIO_ID_CUSTOM8:
        case MSDK_SCENARIO_ID_CUSTOM9:
        case MSDK_SCENARIO_ID_CUSTOM10:
            *(feature_data + 2) = 2;
            break;
        case MSDK_SCENARIO_ID_CUSTOM3:
        default:
            *(feature_data + 2) = 1;
            break;
        }
        break;
         /*its sensor_fusion test Need to use the test box to manually test to obtain the best offset value
          *offset calculation process：
          *Best correlation of 0.000011 at shift of 3.08ms
          *According to the above log, the shift is 3.08,Test 5-10 times, take the average, use the average * 1000000 and then add to the previous offset
          *then you can get the latest offset*/
    case SENSOR_FEATURE_GET_OFFSET_TO_START_OF_EXPOSURE:
        *(MINT32 *)(signed long)(*(feature_data + 1)) = -1697585;
        break;
    case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
        switch (*feature_data) {
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.cap.pclk;
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.normal_video.pclk;
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.hs_video.pclk;
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.slim_video.pclk;
            break;
        case MSDK_SCENARIO_ID_CUSTOM1:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.custom1.pclk;
            break;
        case MSDK_SCENARIO_ID_CUSTOM2:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.custom2.pclk;
            break;
        case MSDK_SCENARIO_ID_CUSTOM3:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.custom3.pclk;
            break;
        case MSDK_SCENARIO_ID_CUSTOM4:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.custom4.pclk;
                break;
        case MSDK_SCENARIO_ID_CUSTOM5:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.custom5.pclk;
                break;
        case MSDK_SCENARIO_ID_CUSTOM6:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.custom6.pclk;
                break;
        case MSDK_SCENARIO_ID_CUSTOM7:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.custom7.pclk;
                break;
        case MSDK_SCENARIO_ID_CUSTOM8:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.custom8.pclk;
                break;
        case MSDK_SCENARIO_ID_CUSTOM9:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.custom9.pclk;
                break;
        case MSDK_SCENARIO_ID_CUSTOM10:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.custom10.pclk;
                break;
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        default:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.pre.pclk;
            break;
        }
        break;
    case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
        switch (*feature_data) {
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.cap.framelength << 16)
                + imgsensor_info.cap.linelength;
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.normal_video.framelength << 16)
                + imgsensor_info.normal_video.linelength;
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.hs_video.framelength << 16)
                + imgsensor_info.hs_video.linelength;
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.slim_video.framelength << 16)
                + imgsensor_info.slim_video.linelength;
            break;
        case MSDK_SCENARIO_ID_CUSTOM1:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.custom1.framelength << 16)
                + imgsensor_info.custom1.linelength;
            break;
        case MSDK_SCENARIO_ID_CUSTOM2:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.custom2.framelength << 16)
                + imgsensor_info.custom2.linelength;
            break;
        case MSDK_SCENARIO_ID_CUSTOM3:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.custom3.framelength << 16)
                + imgsensor_info.custom3.linelength;
            break;
        case MSDK_SCENARIO_ID_CUSTOM4:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.custom4.framelength << 16)
                + imgsensor_info.custom4.linelength;
            break;
        case MSDK_SCENARIO_ID_CUSTOM5:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.custom5.framelength << 16)
                + imgsensor_info.custom5.linelength;
            break;
        case MSDK_SCENARIO_ID_CUSTOM6:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.custom6.framelength << 16)
                + imgsensor_info.custom6.linelength;
            break;
        case MSDK_SCENARIO_ID_CUSTOM7:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.custom7.framelength << 16)
                + imgsensor_info.custom7.linelength;
            break;
        case MSDK_SCENARIO_ID_CUSTOM8:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.custom8.framelength << 16)
                + imgsensor_info.custom8.linelength;
            break;
        case MSDK_SCENARIO_ID_CUSTOM9:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.custom9.framelength << 16)
                + imgsensor_info.custom9.linelength;
            break;
        case MSDK_SCENARIO_ID_CUSTOM10:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.custom10.framelength << 16)
                + imgsensor_info.custom10.linelength;
            break;
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        default:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.pre.framelength << 16)
                + imgsensor_info.pre.linelength;
            break;
        }
        break;
    case SENSOR_FEATURE_GET_PERIOD:
        *feature_return_para_16++ = imgsensor.line_length;
        *feature_return_para_16 = imgsensor.frame_length;
        *feature_para_len = 4;
    break;
    case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
        *feature_return_para_32 = imgsensor.pclk;
        *feature_para_len = 4;
    break;
    case SENSOR_FEATURE_SET_ESHUTTER:
        set_shutter(*feature_data);
    break;
    case SENSOR_FEATURE_SET_NIGHTMODE:
    break;
    case SENSOR_FEATURE_SET_GAIN:
        set_gain((UINT16) *feature_data);
    break;
    case SENSOR_FEATURE_SET_FLASHLIGHT:
    break;
    case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
    break;
    case SENSOR_FEATURE_SET_REGISTER:
        if(sensor_reg_data->RegAddr == 0xff )
            seamless_switch(sensor_reg_data->RegData, 1920, 369, 960, 369);
        else
            write_cmos_sensor(sensor_reg_data->RegAddr, sensor_reg_data->RegData);

    break;
    case SENSOR_FEATURE_GET_REGISTER:
        sensor_reg_data->RegData =
            read_cmos_sensor(sensor_reg_data->RegAddr);
    break;
    case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
        *feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
        *feature_para_len = 4;
    break;
    case SENSOR_FEATURE_SET_VIDEO_MODE:
        set_video_mode(*feature_data);
    break;
    case SENSOR_FEATURE_CHECK_SENSOR_ID:
        get_imgsensor_id(feature_return_para_32);
    break;
/*    case SENSOR_FEATURE_CHECK_MODULE_ID:
        *feature_return_para_32 = imgsensor_info.module_id;
        break;*/
    case SENSOR_FEATURE_GET_SENSOR_OTP_ALL:
    {
        memcpy(feature_return_para_32, (UINT32 *)otp_data, sizeof(otp_data));
        break;
    }
    case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
        set_auto_flicker_mode((BOOL)*feature_data_16,
            *(feature_data_16+1));
    break;
    case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
        set_max_framerate_by_scenario(
            (enum MSDK_SCENARIO_ID_ENUM)*feature_data,
            *(feature_data+1));
    break;
    case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
        get_default_framerate_by_scenario(
            (enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
            (MUINT32 *)(uintptr_t)(*(feature_data+1)));
    break;
    case SENSOR_FEATURE_SET_TEST_PATTERN:
        set_test_pattern_mode((BOOL)*feature_data);
    break;
    case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
        *feature_return_para_32 = imgsensor_info.checksum_value;
        *feature_para_len = 4;
    break;
    case SENSOR_FEATURE_SET_FRAMERATE:
        spin_lock(&imgsensor_drv_lock);
        imgsensor.current_fps = *feature_data_32;
        spin_unlock(&imgsensor_drv_lock);
        LOG_INF("current fps :%d\n", imgsensor.current_fps);
    break;
    case SENSOR_FEATURE_GET_CROP_INFO:
        LOG_INF("GET_CROP_INFO scenarioId:%d\n",
            *feature_data_32);

        wininfo = (struct  SENSOR_WINSIZE_INFO_STRUCT *)
            (uintptr_t)(*(feature_data+1));
        switch (*feature_data_32) {
            case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                memcpy((void *)wininfo,
                    (void *)&imgsensor_winsize_info[1],
                    sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
            case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                memcpy((void *)wininfo,
                    (void *)&imgsensor_winsize_info[2],
                    sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
            case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
                memcpy((void *)wininfo,
                    (void *)&imgsensor_winsize_info[3],
                    sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
            case MSDK_SCENARIO_ID_SLIM_VIDEO:
                memcpy((void *)wininfo,
                    (void *)&imgsensor_winsize_info[4],
                    sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
            case MSDK_SCENARIO_ID_CUSTOM1:
                memcpy((void *)wininfo,
                    (void *)&imgsensor_winsize_info[5],
                    sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
            case MSDK_SCENARIO_ID_CUSTOM2:
                memcpy((void *)wininfo,
                    (void *)&imgsensor_winsize_info[6],
                    sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
            case MSDK_SCENARIO_ID_CUSTOM3:
                memcpy((void *)wininfo,
                    (void *)&imgsensor_winsize_info[7],
                    sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
            case MSDK_SCENARIO_ID_CUSTOM4:
                memcpy((void *)wininfo,
                    (void *)&imgsensor_winsize_info[8],
                    sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
            case MSDK_SCENARIO_ID_CUSTOM5:
                memcpy((void *)wininfo,
                    (void *)&imgsensor_winsize_info[9],
                    sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
            case MSDK_SCENARIO_ID_CUSTOM6:
                memcpy((void *)wininfo,
                    (void *)&imgsensor_winsize_info[10],
                    sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
            case MSDK_SCENARIO_ID_CUSTOM7:
                memcpy((void *)wininfo,
                    (void *)&imgsensor_winsize_info[11],
                    sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
            case MSDK_SCENARIO_ID_CUSTOM8:
                memcpy((void *)wininfo,
                    (void *)&imgsensor_winsize_info[12],
                    sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
            case MSDK_SCENARIO_ID_CUSTOM9:
                memcpy((void *)wininfo,
                    (void *)&imgsensor_winsize_info[13],
                    sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
            case MSDK_SCENARIO_ID_CUSTOM10:
                memcpy((void *)wininfo,
                    (void *)&imgsensor_winsize_info[14],
                    sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
            case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            default:
                memcpy((void *)wininfo,
                    (void *)&imgsensor_winsize_info[0],
                    sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        }
    break;
    case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
        LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
            (UINT16)*feature_data, (UINT16)*(feature_data+1),
            (UINT16)*(feature_data+2));
    break;
    case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
            switch (*feature_data) {
                case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
                    imgsensor_info.cap.mipi_pixel_rate;
                break;
            case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
                    imgsensor_info.normal_video.mipi_pixel_rate;
                break;
            case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
                    imgsensor_info.hs_video.mipi_pixel_rate;
                break;
            case MSDK_SCENARIO_ID_SLIM_VIDEO:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
                    imgsensor_info.slim_video.mipi_pixel_rate;
                break;
            case MSDK_SCENARIO_ID_CUSTOM1:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
                    imgsensor_info.custom1.mipi_pixel_rate;
                break;
            case MSDK_SCENARIO_ID_CUSTOM2:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
                    imgsensor_info.custom2.mipi_pixel_rate;
                break;
            case MSDK_SCENARIO_ID_CUSTOM3:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
                    imgsensor_info.custom3.mipi_pixel_rate;
                break;
            case MSDK_SCENARIO_ID_CUSTOM4:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
                    imgsensor_info.custom4.mipi_pixel_rate;
                break;
            case MSDK_SCENARIO_ID_CUSTOM5:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
                    imgsensor_info.custom5.mipi_pixel_rate;
                break;
            case MSDK_SCENARIO_ID_CUSTOM6:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
                    imgsensor_info.custom6.mipi_pixel_rate;
                break;
            case MSDK_SCENARIO_ID_CUSTOM7:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
                    imgsensor_info.custom7.mipi_pixel_rate;
                break;
            case MSDK_SCENARIO_ID_CUSTOM8:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
                    imgsensor_info.custom8.mipi_pixel_rate;
                break;
            case MSDK_SCENARIO_ID_CUSTOM9:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
                    imgsensor_info.custom9.mipi_pixel_rate;
                break;
            case MSDK_SCENARIO_ID_CUSTOM10:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
                    imgsensor_info.custom10.mipi_pixel_rate;
                break;
            case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            default:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
                    imgsensor_info.pre.mipi_pixel_rate;
                break;
            }
    break;
    case SENSOR_FEATURE_GET_BINNING_TYPE:
        switch (*(feature_data + 1)) {
            case MSDK_SCENARIO_ID_CUSTOM3:
            case MSDK_SCENARIO_ID_CUSTOM5:
                *feature_return_para_32 = 1;
                break;
            case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            case MSDK_SCENARIO_ID_CUSTOM4:
                *feature_return_para_32 = 2940;
                break;
            default:
                *feature_return_para_32 = 1470; /*BINNING_AVERAGED*/
                break;
            }
        LOG_INF("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
            *feature_return_para_32);
        *feature_para_len = 4;

        break;
    case SENSOR_FEATURE_GET_VC_INFO:
        LOG_INF("SENSOR_FEATURE_GET_VC_INFO %d\n",
            (UINT16) *feature_data);

        pvcinfo =
        (struct SENSOR_VC_INFO_STRUCT *) (uintptr_t) (*(feature_data + 1));

        switch (*feature_data_32) {
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:/*4624*2600*/
        case MSDK_SCENARIO_ID_CUSTOM6:
        case MSDK_SCENARIO_ID_CUSTOM8:
            memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[1],
                   sizeof(struct SENSOR_VC_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_CUSTOM2:  /*4000*2256*/
            memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[2],
                   sizeof(struct SENSOR_VC_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
        case MSDK_SCENARIO_ID_CUSTOM1:
        case MSDK_SCENARIO_ID_CUSTOM7:
        default: /*4624*3468*/
            memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[0],
                   sizeof(struct SENSOR_VC_INFO_STRUCT));
            break;
        }
        break;
    case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
        switch (*feature_data) {
            case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            case MSDK_SCENARIO_ID_SLIM_VIDEO:
            case MSDK_SCENARIO_ID_CUSTOM1:
            case MSDK_SCENARIO_ID_CUSTOM2:
            case MSDK_SCENARIO_ID_CUSTOM6:
            case MSDK_SCENARIO_ID_CUSTOM7:
            case MSDK_SCENARIO_ID_CUSTOM8:
                *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
                break;
            default:
                *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
                break;
        }
        break;
    case SENSOR_FEATURE_GET_PDAF_INFO:
        PDAFinfo = (struct SET_PD_BLOCK_INFO_T *)
            (uintptr_t)(*(feature_data+1));

        switch (*feature_data) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
        case MSDK_SCENARIO_ID_CUSTOM1:
        case MSDK_SCENARIO_ID_CUSTOM7:
            imgsensor_pd_info.i4BlockNumX = 284; //4624*3468
            imgsensor_pd_info.i4BlockNumY = 215;
            memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info,
                sizeof(struct SET_PD_BLOCK_INFO_T));
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        case MSDK_SCENARIO_ID_CUSTOM6:
        case MSDK_SCENARIO_ID_CUSTOM8:
            imgsensor_pd_info.i4BlockNumX = 284; //4624*2600
            imgsensor_pd_info.i4BlockNumY = 162;
            memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info,
                sizeof(struct SET_PD_BLOCK_INFO_T));
            break;
        case MSDK_SCENARIO_ID_CUSTOM2:
            imgsensor_pd_info.i4BlockNumX = 248; //4000*2256
            imgsensor_pd_info.i4BlockNumY = 141;
            memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info,
                sizeof(struct SET_PD_BLOCK_INFO_T));
            break;
        default:
            memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info,
                            sizeof(struct SET_PD_BLOCK_INFO_T));
            break;
        }
        break;
    case SENSOR_FEATURE_GET_PDAF_DATA:
        break;
    case SENSOR_FEATURE_SET_PDAF:
            imgsensor.pdaf_mode = *feature_data_16;
        break;
    case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
        *feature_return_para_i32 = get_sensor_temperature();
        *feature_para_len = 4;
    break;
    case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
        LOG_INF("SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME\n");
        set_shutter_frame_length((UINT16)*feature_data, (UINT16)*(feature_data+1), (UINT16) *(feature_data + 2));
        break;
    case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
        streaming_control(KAL_FALSE);
        break;

    case SENSOR_FEATURE_SET_STREAMING_RESUME:
        if (*feature_data != 0)
            set_shutter(*feature_data);
        streaming_control(KAL_TRUE);
        break;
    case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
        /*
         * 1, if driver support new sw frame sync
         * set_shutter_frame_length() support third para auto_extend_en
         */
        *(feature_data + 1) = 1; /* margin info by scenario */
        *(feature_data + 2) = imgsensor_info.margin;
        break;
    default:
    break;
    }

    return ERROR_NONE;
}   /*  feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
    open,
    get_info,
    get_resolution,
    feature_control,
    control,
    close
};

UINT32 OV64B_MIPI_RAW22277_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
    /* To Do : Check Sensor status here */
    if (pfFunc != NULL)
    *pfFunc =  &sensor_func;
    return ERROR_NONE;
}
