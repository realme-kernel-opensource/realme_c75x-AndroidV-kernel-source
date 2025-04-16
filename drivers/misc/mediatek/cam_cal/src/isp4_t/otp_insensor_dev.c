/***********************************************************
 * ** Copyright (C), 2008-2016, OPPO Mobile Comm Corp., Ltd.
 * ** ODM_HQ_EDIT
 * ** File: - otp_insensor_dev.c
 * ** Description: Source file for CBufferList.
 * **		   To allocate and free memory block safely.
 * ** Version: 1.0
 * ** Date : 2018/12/07
 * ** Author: YanKun.Zhai@Mutimedia.camera.driver.otp
 * **
 * ** ------------------------------- Revision History: -------------------------------
 * **   <author>History<data>	  <version >version	   <desc>
 * **  YanKun.Zhai 2018/12/07	 1.0	 build this module
 * **
 * ****************************************************************/

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <linux/slab.h>

#include "cam_cal.h"
#include "cam_cal_define.h"
#include "cam_cal_list.h"

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#ifdef ODM_HQ_EDIT
/*Houbing.Peng@ODM_HQ Cam.Drv 20200915 add for otp*/
#include <soc/oplus/system/oplus_project.h>
#endif

#define LOG_TAG "OTP_InSensor"
#define LOG_ERR(format, ...) pr_err(LOG_TAG "Line:%d  FUNC: %s:  "format, __LINE__, __func__, ## __VA_ARGS__)
#define LOG_INFO(format, ...) pr_info(LOG_TAG "  %s:  "format, __func__, ## __VA_ARGS__)
#define LOG_DEBUG(format, ...) pr_debug(LOG_TAG "  %s:  "format, __func__, ## __VA_ARGS__)

#define READ_SUCCESS	0
#define ERROR_I2C	   1
#define ERROR_CHECKSUM  2
#define ERROR_READ_FLAG 3
#define ERROR_GROUP	 4
#define OTP_THREAHOLD   3

#define READ_FLAG	   1

static DEFINE_SPINLOCK(g_spinLock);

static struct i2c_client *g_pstI2CclientG;
static int g_channel;

/* add for linux-4.4 */
#ifndef I2C_WR_FLAG
#define I2C_WR_FLAG		(0x1000)
#define I2C_MASK_FLAG	(0x00ff)
#endif

struct i2c_client *g_pstI2Cclients[3]; /* I2C_DEV_IDX_MAX */


#define MAX_EEPROM_BYTE 0x1FFF
#define CHECKSUM_FLAG_ADDR MAX_EEPROM_BYTE-1
#define READ_FLAG_ADDR MAX_EEPROM_BYTE-2

#define OTP_DATA_GOOD_FLAG 0x88
char g_otp_buf[3][MAX_EEPROM_BYTE] = {{0}, {0}, {0}};
char g_otp_hi846_buf[MAX_EEPROM_BYTE] = {0};

#define MAX_NAME_LENGTH 20
#define MAX_GROUP_NUM 8
#define MAX_GROUP_ADDR_NUM 3
#define MAX_PAGE_NUMBER 5

typedef struct
{
	int group_start_addr;
	int group_end_addr;
	int group_flag_addr;
	int group_checksum_addr;
}GROUP_ADDR_INFO;

typedef struct
{
	char group_name[MAX_NAME_LENGTH];
	GROUP_ADDR_INFO group_addr_info[MAX_GROUP_ADDR_NUM];
}GROUP_INFO;

enum
{
	startpage1 = 2,
	startpage2 = 7,
} frontpage;

enum
{
	startaddr1 = 0x8200,
	startaddr2 = 0x8C00,
} pageheaderaddr;

typedef struct
{
	char module_name[MAX_NAME_LENGTH];
	int group_num;
	int group_addr_info_num;
	GROUP_INFO group_info[MAX_GROUP_NUM];
	int  (* readFunc) (u16 addr, u8 *data);
}OTP_MAP;


/*This sensor only have two group otp data.in diffrent page in sensor*/
int sc820cs_read_data(u16 addr, u8 *data);
OTP_MAP sc820cs_otp_map_arka = {
		.module_name = "SC820CS_ARK",
		.group_num = 4,
		.group_addr_info_num = 2,
		.readFunc = sc820cs_read_data,
		.group_info = {
						{"info",
							{
								{0x827A, 0x8289, 0x828A, 0x828B},
								{0x8C7A, 0x8C89, 0x8C8A, 0x8C8B},
							},
						},
						{"awb",
							{
								{0x828C, 0x829B, 0x829C, 0x829D},
								{0x8C8C, 0x8C9B, 0x8C9C, 0x8C9D},
							 },
						},
						{"sn",
							{
								{0x82A4, 0x82BB, 0x82BC, 0x82BD},
								{0x8CA4, 0x8CBB, 0x8CBC, 0x8CBD},
							},
						},
						{"lsc",
							{
								{0x82BE, 0x8BF1, 0x8BF2, 0x8BF3},
								{0x8CBE, 0x95F1, 0x95F2, 0x95F3},
							},
						},
			},
};

/* sonic Hi846 map */
int hi846_read_data(u16 addr, u8 *data);

OTP_MAP hi846_otp_map_oris = {
		.module_name = "HI846_ORIS",
		.group_num = 4,
		.group_addr_info_num = 3,
		.readFunc = hi846_read_data,
		.group_info = {
						{"info",
							{ /* start   end     flag   checksum */
								{0x0202, 0X0210, 0x0211, 0x0212},
								{0x0993, 0x09A1, 0x09A2, 0x09A3},
								{0x1124, 0x1132, 0x1133, 0x1134},
							},
						},
						{"awb",
							{
								{0x0213, 0x0222, 0x0223, 0x0224},
								{0x09A4, 0x09B3, 0x09B4, 0x09B5},
								{0x1135, 0x1144, 0x1145, 0x1146},
							 },
						},
						{"lsc",
							{
								{0x0245, 0x0990, 0x0991, 0x0992},
								{0x09D6, 0x1121, 0x1122, 0x1123},
								{0x1167, 0x18B2, 0x18B3, 0x18B4},
							},
						},
						{"sn",
							{
								{0x022B, 0x0242, 0x0243, 0x0244},
								{0x09BC, 0x09D3, 0x09D4, 0x09D5},
								{0x114D, 0x1164, 0x1165, 0x1166},
							},
						},
			},
};


static int read_reg16_data8(u16 addr, u8 *data)
{
	int ret = 0;
	char puSendCmd[2] = {(char)(addr >> 8), (char)(addr & 0xff)};

	spin_lock(&g_spinLock);
		g_pstI2CclientG->addr =
			g_pstI2CclientG->addr & (I2C_MASK_FLAG | I2C_WR_FLAG);
	spin_unlock(&g_spinLock);

	ret = i2c_master_send(g_pstI2CclientG, puSendCmd, 2);
	if (ret != 2) {
		pr_err("I2C send failed!!, Addr = 0x%x\n", addr);
		return -1;
	}
	ret = i2c_master_recv(g_pstI2CclientG, (char *)data, 1);
	if (ret != 1) {
		pr_err("I2C read failed!!\n");
		return -1;
	}
	spin_lock(&g_spinLock);
	g_pstI2CclientG->addr = g_pstI2CclientG->addr & I2C_MASK_FLAG;
	spin_unlock(&g_spinLock);

	return 0;
}
static int write_reg16_data8(u16 addr, u8 data)
{
	int ret = 0;
	char puSendCmd[3] = {(char)(addr >> 8), (char)(addr & 0xff),
						(char)(data & 0xff)};
	spin_lock(&g_spinLock);
	g_pstI2CclientG->addr =
			g_pstI2CclientG->addr & (I2C_MASK_FLAG | I2C_WR_FLAG);
	spin_unlock(&g_spinLock);

	ret = i2c_master_send(g_pstI2CclientG, puSendCmd, 3);

	if (ret != 3) {
		pr_err("I2C send failed!!, Addr = 0x%x\n", addr);
		return -1;
	}
	spin_lock(&g_spinLock);
	g_pstI2CclientG->addr = g_pstI2CclientG->addr & I2C_MASK_FLAG;
	spin_unlock(&g_spinLock);

	return 0;
}

bool sc820cs_check_page_head(int *headeraddr, int curr_addr, int *pagenumber)
{
	if (*headeraddr < curr_addr) {
		*headeraddr = *headeraddr + 0x200;
	}

	if (*headeraddr == curr_addr) {
		*pagenumber = *pagenumber + 1;
		*headeraddr = *headeraddr + 0x200;
		return true;
	}
	return false;
}

bool sc820cs_sensor_otp_init(kal_uint16 threshold, int pagenumber)
{
	/*0x4412, 0xxx, read 0x8000-0x87FF data£¬0x4412,0x01
	 read 0x8800-0x8FFF£¬0x4412,0x03
	 0x4407, 0xxx, read 0x8000-0x87FF £¬0x4407,0x0c
	 read 0x8800-0x8FFF £¬0x4407,0x00
	*/
	int pageaddr = pagenumber * 2 - 1;
	static int startaddr = 0;
	static int endaddr = 0;
	if (pagenumber < 2 || pagenumber > 12)
		return false;
	startaddr = 0x8200 + (0x200 * (pagenumber - 2));
	endaddr = 0x83FF + (0x200 * (pagenumber - 2));

	write_reg16_data8(0x36b0, 0x48);
	write_reg16_data8(0x36b1, threshold);                     /* set threshold */
	write_reg16_data8(0x36b2, 0x41);
	write_reg16_data8(0x4408, (startaddr >> 8)&0xff);        /* High byte start address for OTP load Group1 */
	write_reg16_data8(0x4409, (startaddr)&0xff);             /* Low byte start address for OTP load  Group1 */
	write_reg16_data8(0x440a, (endaddr >> 8)&0xff);          /* High byte end address for OTP load Group1 */
	write_reg16_data8(0x440b, (endaddr)&0xff);              /* Low byte end address for OTP load Group1 */
	write_reg16_data8(0x4401, 0x13);
	write_reg16_data8(0x4412, pageaddr);
	write_reg16_data8(0x4407, 0x00);
	write_reg16_data8(0x4400, 0x11);                         /* 0x4400, 0x11 manual load */

	mdelay(20);

	return true;
}

/* hi846 */
int hi846_read_data(u16 addr, u8 *data)
{
	static u16 last_addr = 0;
	if (addr != last_addr+ 1) {
		write_reg16_data8(0x070a, (addr >> 8) & 0xff);
		write_reg16_data8(0x070b, addr & 0xff);
		write_reg16_data8(0x0702, 0x01);
	}

	last_addr = addr;
	return read_reg16_data8(0x708, data);
}

void hi846_otp_read_enable(void)
{
	write_reg16_data8(0x0A02, 0x01);
	write_reg16_data8(0x0A00, 0x00);
	mdelay(10);
	write_reg16_data8(0x0F02, 0x00);
	write_reg16_data8(0x071A, 0x01);
	write_reg16_data8(0x071B, 0x09);
	write_reg16_data8(0x0D04, 0x01);
	write_reg16_data8(0x0D00, 0x07);
	write_reg16_data8(0x003E, 0x10);
	write_reg16_data8(0x0A00, 0x01);
}

int sc820cs_read_data(u16 addr, u8 *data)
{
	static u16 last_addr = 0;
	last_addr = addr;
	return read_reg16_data8(addr, data);
}

static int parse_otp_map_data(OTP_MAP * map, char * data)
{
	int i = 0, j = 0;
	int addr = 0, size = 0, curr_addr = 0;
	int  ret = 0;
	char readbyte = 0;
	char group_flag = 0;
	int checksum = -1;

	LOG_INFO("module: %s ......", map->module_name);

	addr = 0x0201;
	ret = map->readFunc(addr, &group_flag);

	if (ret < 0) {
		LOG_ERR("read group flag error addr 0x%04x", addr);
		return -ERROR_I2C;
	}
	if (group_flag == 0x01) {
		j = 0;
	} else if (group_flag == 0x13) {
		j = 1;
	} else if (group_flag == 0x37) {
		j = 2;
	} else {
		LOG_INFO("invalid block module flag 0x%x", j);
		return -ERROR_READ_FLAG;
	}

	for (i = 0; i < map->group_num; i++) {
		checksum = 0;
		size = 0;

		LOG_INFO("groupinfo: %s, start_addr 0x%04x(%04d)", map->group_info[i].group_name, curr_addr, curr_addr);

		for (addr = map->group_info[i].group_addr_info[j].group_start_addr;
				addr <= map->group_info[i].group_addr_info[j].group_end_addr;
				addr++) {
			ret = map->readFunc(addr, data);
			if (ret < 0) {
				LOG_ERR(" read data error");
			}
			LOG_DEBUG(" group: %s, addr: 0x%04x, viraddr: 0x%04x(%04d), data: 0x%04x(%04d) ",
						map->group_info[i].group_name, addr, curr_addr, curr_addr, *data, *data);

			checksum += *data;
			curr_addr++;
			size++;
			data++;
		}

		checksum = checksum % 0xFF;
		ret = map->readFunc(map->group_info[i].group_addr_info[j].group_checksum_addr, &readbyte);
		if (checksum == readbyte) {
			LOG_INFO("groupinfo: %s, checksum OK c(%04d) r(%04d)", map->group_info[i].group_name, checksum, readbyte);
		} else {
			LOG_ERR("groupinfo: %s, checksum ERROR ret=%d, checksum=%04d readbyte=%04d", map->group_info[i].group_name, ret, checksum, readbyte);
			ret = -ERROR_CHECKSUM;
		}
		LOG_INFO("groupinfo: %s, end_addr 0x%04x(%04d) size 0x%04x(%04d)",
						map->group_info[i].group_name, curr_addr-1, curr_addr-1, size, size);
	}
	return ret;
}

static int parse_otp_map_data_sc820cs(OTP_MAP * map, char * data, int temp)
{
	int group_type = 0, channel_type = 0;
	int addr = 0, size = 0, curr_addr = 0;
	int  ret = 0;
	char readbyte = 0;
	int checksum = -1;
	int pagenumber = 0;
	int pageflag = 0;
	int headeraddr;
	LOG_INFO("module: %s ......", map->module_name);

	if (g_channel == 1) {
		pagenumber = startpage1;
		headeraddr  = startaddr1;
		channel_type = 0;
	} else if (g_channel == 2) {
		pagenumber = startpage2;
		headeraddr  = startaddr2;
		channel_type = 1;
	}

	pageflag = pagenumber;

	ret = sc820cs_sensor_otp_init(temp, pagenumber);
	if (ret == false) {
		if (g_channel == 2) {
			ret = ERROR_GROUP;
		} else {
			ret = ERROR_READ_FLAG;
		}
		return ret;
	}
	for (group_type = 0; group_type < map->group_num; group_type++) {
		checksum = 0;
		size = 0;

		LOG_INFO("groupinfo: %s, start_addr 0x%0x(0x%0x)", map->group_info[group_type].group_name, curr_addr, curr_addr);
		if (!strcmp(map->group_info[group_type].group_name, "lsc")) {
			for (addr = map->group_info[group_type].group_addr_info[channel_type].group_start_addr;
				addr <=  map->group_info[group_type].group_addr_info[channel_type].group_end_addr; addr++) {
				if (sc820cs_check_page_head(&headeraddr, addr, &pageflag) == true) {
						addr = addr + 0x7A;
					ret = sc820cs_sensor_otp_init(temp, pageflag);
					if (ret == false) {
						if (g_channel > 2) {
							ret = ERROR_GROUP;
						} else {
							ret = ERROR_READ_FLAG;
						}
						return ret;
					}
				}
				ret = map->readFunc(addr, data);

				if (ret < 0) {
					LOG_ERR("read data error");
				}
				if (addr == map->group_info[group_type].group_addr_info[channel_type].group_flag_addr) {
					if (*data != READ_FLAG) {
						g_channel++;
						if (g_channel > 2) {
							ret = ERROR_GROUP;
							return ret;
						}
					}
				}
				checksum += *data;
				curr_addr++;
				size++;
				data++;
			}

		} else {	/*There is no cross-page stitching to read, go here*/
			for (addr = map->group_info[group_type].group_addr_info[channel_type].group_start_addr;
					addr <= map->group_info[group_type].group_addr_info[channel_type].group_end_addr;
					addr++) {
				ret = map->readFunc(addr, data);

				if (ret < 0) {
					LOG_ERR("read data error");
				}
				if (addr == map->group_info[group_type].group_addr_info[channel_type].group_flag_addr) {
					if (*data != READ_FLAG) {
						g_channel++;
						if (g_channel > 2) {
							ret = ERROR_GROUP;
							return ret;
						}
					}
				}

				checksum += *data;
				curr_addr++;
				size++;
				data++;
			}
		}

		checksum = checksum % 0xFF;
		ret = map->readFunc(map->group_info[group_type].group_addr_info[channel_type].group_checksum_addr, &readbyte);
		if (checksum == readbyte) {
			ret = READ_SUCCESS;
			LOG_INFO("groupinfo: %s, checksum OK c(0x%04x) r(0x%04x)  checkdum 0x%04x  ret%d",
			map->group_info[group_type].group_name, checksum, readbyte, map->group_info[group_type].group_addr_info[channel_type].group_checksum_addr, ret);
		} else {
			LOG_ERR("groupinfo: %s, checksum ERROR ret=%d, checksum=%04d readbyte=%04d 0x%04x",
			map->group_info[group_type].group_name, ret, checksum, readbyte, map->group_info[group_type].group_addr_info[channel_type].group_checksum_addr);
			ret = ERROR_CHECKSUM;
			return ret;
		}
		LOG_INFO("groupinfo: %s, end_addr 0x%04x(%04d) size 0x%04x(%04d)",
						map->group_info[group_type].group_name, curr_addr-1, curr_addr-1, size, size);
	}
	return ret;
}

unsigned int sc820cs_read_region(struct i2c_client *client, unsigned int addr, unsigned char *data, unsigned int size)
{
	int ret = 0;
	int readtimes;
	int otpThreshold[3] = {0x38, 0x18, 0x58}; /* the Threshold for sc820cs*/
	int tempotpthreshold =  0;
	g_channel = 1;
	if (g_otp_buf[IMGSENSOR_SENSOR_IDX_SUB][READ_FLAG_ADDR]) {
		LOG_INFO("read otp data from g_otp_buf: addr 0x%x, size %d", addr, size);
		memcpy((void *)data, &g_otp_buf[IMGSENSOR_SENSOR_IDX_SUB][addr], size);
		return size;
	}
	if (client != NULL) {
		g_pstI2CclientG = client;
	} else if (g_pstI2Cclients[IMGSENSOR_SENSOR_IDX_SUB] != NULL) {
		g_pstI2CclientG = g_pstI2Cclients[IMGSENSOR_SENSOR_IDX_SUB];
		g_pstI2CclientG->addr = 0x20 >> 1;
	}
	for (readtimes = 0; readtimes < OTP_THREAHOLD; readtimes++) {
		tempotpthreshold = otpThreshold[readtimes];
		ret = parse_otp_map_data_sc820cs(&sc820cs_otp_map_arka, &g_otp_buf[IMGSENSOR_SENSOR_IDX_SUB][0], tempotpthreshold);
		if (ret == ERROR_READ_FLAG || ret == ERROR_CHECKSUM) {
			if(readtimes >= 2 && ret == ERROR_CHECKSUM) {
				break;
			} else {
				continue;
			}
		} else if (ret == ERROR_GROUP) {
			LOG_ERR("bad group or bad check sum, read sc820cs err!!!\n");
			break;
		}

		if(ret == READ_SUCCESS) {
			g_otp_buf[IMGSENSOR_SENSOR_IDX_SUB][CHECKSUM_FLAG_ADDR] = OTP_DATA_GOOD_FLAG;
			g_otp_buf[IMGSENSOR_SENSOR_IDX_SUB][READ_FLAG_ADDR] = 1;
			break;
		}
	}

	if (NULL != data) {
		memcpy((void *)data, &g_otp_buf[IMGSENSOR_SENSOR_IDX_SUB][addr], size);
	}
	return ret;
}

void hi846_otp_read_disable(void)
{
	write_reg16_data8(0x0a00, 0x00);
	mdelay(10);
	write_reg16_data8(0x003e, 0x00);
	write_reg16_data8(0x004a, 0x01);
}
unsigned int Hi846_oris_read_region(struct i2c_client *client, unsigned int addr, unsigned char *data, unsigned int size)
{
	int ret = 0;

	if (g_otp_hi846_buf[READ_FLAG_ADDR]) {
		LOG_INFO("read otp data from g_otp_hi846_buf: addr 0x%x, size %d", addr, size);
		memcpy((void *)data, &g_otp_hi846_buf[addr], size);
		return size;
	}
	if (client != NULL) {
		g_pstI2CclientG = client;
	} else if (g_pstI2Cclients[0] != NULL) {
		g_pstI2CclientG = g_pstI2Cclients[0];
		g_pstI2CclientG->addr =  0x40 >> 1;
	}
	hi846_otp_read_enable();
	ret = parse_otp_map_data(&hi846_otp_map_oris, &g_otp_hi846_buf[0]);
	if (!ret) {
		g_otp_hi846_buf[CHECKSUM_FLAG_ADDR] = OTP_DATA_GOOD_FLAG;
		g_otp_hi846_buf[READ_FLAG_ADDR] = 1;
	}
	hi846_otp_read_disable();
	if (NULL != data) {
		memcpy((void *)data, &g_otp_hi846_buf[addr], size);
	}
	return ret;
}
