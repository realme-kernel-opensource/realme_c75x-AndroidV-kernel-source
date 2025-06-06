// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


/* #define DEBUG 1 */
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <mt-plat/aee.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <mt-plat/sync_write.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include <linux/seq_file.h>
#include <linux/slab.h>
#include "mach/mtk_thermal.h"
#include <linux/bug.h>

#if IS_ENABLED(CONFIG_MTK_CLKMGR)
#include <mach/mtk_clkmgr.h>
#else
#include <linux/clk.h>
#endif

// #include <mt-plat/mtk_wd_api.h>
#include <linux/nvmem-consumer.h>
#include <mtk_gpu_utility.h>
#include <linux/time.h>

#define __MT_MTK_LVTS_TC_C__
#include <tscpu_settings.h>

#if IS_ENABLED(CONFIG_OF)
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif
#define __MT_MTK_LVTS_TC_C__

// #include <mt-plat/mtk_devinfo.h>
#include "mtk_thermal_ipi.h"


/**
 * curr_temp >= tscpu_polling_trip_temp1:
 *	polling interval = interval
 * tscpu_polling_trip_temp1 > cur_temp >= tscpu_polling_trip_temp2:
 *	polling interval = interval * tscpu_polling_factor1
 * tscpu_polling_trip_temp2 > cur_temp:
 *	polling interval = interval * tscpu_polling_factor2
 */
/* chip dependent */
int tscpu_polling_trip_temp1 = 40000;
int tscpu_polling_trip_temp2 = 20000;
int tscpu_polling_factor1 = 3;
int tscpu_polling_factor2 = 4;

#if MTKTSCPU_FAST_POLLING
/* Combined fast_polling_trip_temp and fast_polling_factor,
 * it means polling_delay will be 1/5 of original interval
 * after mtktscpu reports > 65C w/o exit point
 */
int fast_polling_trip_temp = 60000;
int fast_polling_trip_temp_high = 60000; /* deprecaed */
int fast_polling_factor = 1;
int tscpu_cur_fp_factor = 1;
int tscpu_next_fp_factor = 1;
#endif

int tscpu_debug_log;
int tscpu_sspm_thermal_throttle;
#if IS_ENABLED(CONFIG_OF)
const struct of_device_id mt_thermal_of_match[2] = {
	{.compatible = "mediatek,mt6781-lvts",},
	{},
};
#endif
/*=============================================================
 * Local variable definition
 *=============================================================
 */
/* chip dependent */
/*
 * TO-DO: I assume AHB bus frequecy is 78MHz.
 * Please confirm it.
 */
/*
 * module			LVTS Plan
 *=====================================================
 * MCU_LITTLE	LVTS1-0, LVTS1-1, LVTS1-2
 * CAM MM		LVTS1-3
 * MCU_BIG		LVTS2-0, LVTS2-1
 * MD			LVTS3-0
 * MFG			LVTS3-1, LVTS3-2
 */

struct lvts_thermal_controller lvts_tscpu_g_tc[LVTS_CONTROLLER_NUM] = {
	[0] = {
		.ts = {L_TS_LVTS1_0, L_TS_LVTS1_1, L_TS_LVTS1_2, L_TS_LVTS1_3},
		.ts_number = 4,
		.dominator_ts_idx = 1, //TODO: need confirm dominator sensor
		.tc_offset = 0,
		.tc_speed = {
			.group_interval_delay = 0x001,
			.period_unit = 0x00C,
			.filter_interval_delay = 0x001,
			.sensor_interval_delay = 0x001
		}
	},
	[1] = {/*AP BIG*/
		.ts = {L_TS_LVTS2_0, L_TS_LVTS2_1},
		.ts_number = 2,
		.dominator_ts_idx = 1, //TODO: need confirm dominator sensor
		.tc_offset = 0x100,
		.tc_speed = {
			.group_interval_delay = 0x001,
			.period_unit = 0x00C,
			.filter_interval_delay = 0x001,
			.sensor_interval_delay = 0x001
		}
	},
	[2] = {
		.ts = {L_TS_LVTS3_0, L_TS_LVTS3_1, L_TS_LVTS3_2},
		.ts_number = 3,
		.dominator_ts_idx = 1, //TODO: need confirm dominator sensor
		.tc_offset = 0x200,
		.tc_speed = {
			.group_interval_delay = 0x001,
			.period_unit = 0x00C,
			.filter_interval_delay = 0x001,
			.sensor_interval_delay = 0x001
		}
	}
};


static unsigned int g_golden_temp;
static unsigned int g_count_r[L_TS_LVTS_NUM];
static unsigned int g_count_rc[L_TS_LVTS_NUM];
static unsigned int g_count_rc_now[L_TS_LVTS_NUM];
static int g_use_fake_efuse;
int lvts_debug_log;
int lvts_rawdata_debug_log;

#ifdef LVTS_DYNAMIC_ENABLE_REBOOT
static int hw_protect_setting_done;
int lvts_hw_protect_enabled;
#endif

#if DUMP_LVTS_REGISTER_FOR_ZERO_RAW_ISSUE
#define NUM_LVTS_DEVICE_REG (34)
static const unsigned int g_lvts_device_addrs[NUM_LVTS_DEVICE_REG] = {
	0x00,
	0x01,
	0x02,
	0x03,
	0x04,
	0x05,
	0x06,
	0x07,
	0x08,
	0x09,
	0x0A,
	0x0B,
	0x0C,
	0x0D,
	0x0E,
	0x10,
	0x11,
	0x12,
	0x13,
	0x14,
	0x15,
	0x16,
	0x17,
	0x18,
	0x19,
	0x1A,
	0x1B,
	0xF0,
	0xF1,
	0xF2,
	0xF3,
	0xFC,
	0xFD,
	0xFF};

static unsigned int g_lvts_device_value_b[LVTS_CONTROLLER_NUM]
	[NUM_LVTS_DEVICE_REG];
static unsigned int g_lvts_device_value_e[LVTS_CONTROLLER_NUM]
	[NUM_LVTS_DEVICE_REG];

#define NUM_LVTS_CONTROLLER_REG (28)
static const unsigned int g_lvts_controller_addrs[NUM_LVTS_CONTROLLER_REG] = {
	0x00,//LVTSMONCTL0_0
	0x04,//LVTSMONCTL1_0
	0x08,//LVTSMONCTL2_0
	0x0C,//LVTSMONINT_0
	0x10,//LVTSMONINTSTS_0
	0x20,//LVTSMONIDET3_0
	0x38,//LVTSMSRCTL0_0
	0x3C,//LVTSMSRCTL1_0
	0x40,//LVTSTSSEL_0
	0x4C,//LVTS_ID_0
	0x50,//LVTS_CONFIG_0
	0x54,//LVTSSEDATA00_0
	0x58,//LVTSSEDATA01_0
	0x5C,//LVTSSEDATA02_0
	0x60,//LVTSSEDATA03_0
	0x90,//LVTSMSR0_0
	0x94,//LVTSMSR1_0
	0x98,//LVTSMSR2_0
	0x9C,//LVTSMSR3_0
	0xB0,//LVTSRDATA0_0
	0xB4,//LVTSRDATA1_0
	0xB8,//LVTSRDATA2_0
	0xBC,//LVTSRDATA3_0
	0xC0,//LVTSPROTCTCTL_0
	0xCC,//LVTSPROTTC_0
	0xFC,//LVTSPRRE3_0
	0xE8,//LVTSDBGSEL_0
	0xE4};//LVTSCLKEN_0
static unsigned int g_lvts_controller_value_b[LVTS_CONTROLLER_NUM]
	[NUM_LVTS_CONTROLLER_REG];
static unsigned int g_lvts_controller_value_e[LVTS_CONTROLLER_NUM]
	[NUM_LVTS_CONTROLLER_REG];
#endif

#if LVTS_VALID_DATA_TIME_PROFILING
unsigned long long SODI3_count, noValid_count;
/* If isTempValid is 0, it means no valid temperature data
 * between two SODI3 entry points.
 */
int isTempValid;
/* latency_array
 * {a, b}
 * a: a time threshold in milliseconds. if it is -1, it means others.
 * b: the number of valid temperature latencies from a phone enters SODI3 to
 *    a phone gets a valid temperature of any sensor.
 *    It is possible a phone enters SODI3 several times without a valid
 *    temperature data.
 */
#define NUM_TIME_TH (16) /* TODO: know what it is */
static unsigned int latency_array[NUM_TIME_TH][2] = {
	{100, 0},
	{200, 0},
	{300, 0},
	{400, 0},
	{500, 0},
	{600, 0},
	{700, 0},
	{800, 0},
	{900, 0},
	{1000, 0},
	{2000, 0},
	{3000, 0},
	{4000, 0},
	{5000, 0},
	{10000, 0},
	{-1, 0}
};
long long start_timestamp;
static long long end_timestamp, time_diff;
/* count if start_timestamp is bigger than end_timestamp */
int diff_error_count;
#endif

#if CFG_THERM_LVTS
#define DEFAULT_EFUSE_GOLDEN_TEMP		(50)
#define DEFAULT_EFUSE_COUNT			(19000)
#define DEFAULT_EFUSE_COUNT_RC			(5350)
#define FAKE_EFUSE_VALUE			0x2B048500
#define LVTS_COEFF_A_X_1000			(-204650)
#define LVTS_COEFF_B_X_1000			 (204650)
#endif

/*=============================================================
 * Local function declartation
 *=============================================================
 */
static unsigned int  lvts_temp_to_raw(int ret, enum lvts_sensor_enum ts_name);

static void lvts_set_tc_trigger_hw_protect(
		int temperature, int temperature2, int tc_num);

void mt_reg_sync_writel_print(unsigned int val, void *addr)
{
	if (lvts_debug_log)
		lvts_dbg_printk("### LVTS_REG: addr 0x%p, val 0x%x\n",
								addr, val);

	mt_reg_sync_writel(val, addr);
}

/*=============================================================*/

static int lvts_write_device(unsigned int config, unsigned int dev_reg_idx,
unsigned int data, int tc_num)
{
	int offset;

	dev_reg_idx &= 0xFF;
	data &= 0xFF;

		config = config | (dev_reg_idx << 8) | data;

		offset = lvts_tscpu_g_tc[tc_num].tc_offset;

		mt_reg_sync_writel_print(config, LVTS_CONFIG_0 + offset);

	/*
	 * LVTS Device Register Setting take 1us(by 26MHz clock source)
	 * interface latency to access.
	 * So we set 2~3us delay could guarantee access complete.
	 */
	udelay(3);

	return 1;
}

static unsigned int lvts_read_device(unsigned int config,
unsigned int dev_reg_idx, int tc_num)
{
	int offset, cnt;
	unsigned int data;

	dev_reg_idx &= 0xFF;

		config = config | (dev_reg_idx << 8) | 0x00;

		offset = lvts_tscpu_g_tc[tc_num].tc_offset;

		mt_reg_sync_writel_print(config, LVTS_CONFIG_0 + offset);

		/* wait 2us + 3us buffer*/
		udelay(5);
		/* Check ASIF bus status for transaction finished
		 * Wait until DEVICE_ACCESS_START = 0
		 */
		cnt = 0;
		while ((readl(LVTS_CONFIG_0 + offset) & _BIT_(24))) {
			cnt++;

			if (cnt == 100) {
				lvts_printk("Error: DEVICE_ACCESS_START didn't ready\n");
				break;
			}
			udelay(2);
		}

	data = (readl(LVTSRDATA0_0 + offset));

	return data;
}
int lvts_raw_to_temp(unsigned int msr_raw, enum lvts_sensor_enum ts_name)
{
	/* This function returns degree mC
	 * temp[i] = a * MSR_RAW/16384 + GOLDEN_TEMP/2 + b
	 * a = -204.65
	 * b =  204.65
	 */
	int temp_mC = 0;
	int temp1 = 0;

	temp1 = (LVTS_COEFF_A_X_1000 * ((unsigned long long)msr_raw)) >> 14;

	temp_mC = temp1 + g_golden_temp * 500 + LVTS_COEFF_B_X_1000;

	return temp_mC;
}
#if DUMP_LVTS_REGISTER_FOR_ZERO_RAW_ISSUE
static void read_controller_reg_before_active(void)
{
	int i, j, offset, temp;

	for (i = 0; i < ARRAY_SIZE(lvts_tscpu_g_tc); i++) {
		offset = lvts_tscpu_g_tc[i].tc_offset;

		for (j = 0; j < NUM_LVTS_CONTROLLER_REG; j++) {
			temp = readl(LVTSMONCTL0_0 + g_lvts_controller_addrs[j]
				+ offset);
			g_lvts_controller_value_b[i][j] = temp;
		}
	}

}

static void read_controller_reg_when_error(void)
{
	int i, j, offset, temp;

	for (i = 0; i < ARRAY_SIZE(lvts_tscpu_g_tc); i++) {
		offset = lvts_tscpu_g_tc[i].tc_offset; //tc offset

		for (j = 0; j < NUM_LVTS_CONTROLLER_REG; j++) {
			temp = readl(LVTSMONCTL0_0 + g_lvts_controller_addrs[j]
				+ offset);
			g_lvts_controller_value_e[i][j] = temp;
		}
	}
}

static void read_controller_reg_when_error_by_ctrl_num(int ctrl_num)
{
	int j, offset, temp;

	//for (i = 0; i < ARRAY_SIZE(lvts_tscpu_g_tc); i++) {
		offset = lvts_tscpu_g_tc[ctrl_num].tc_offset; //tc offset

		for (j = 0; j < NUM_LVTS_CONTROLLER_REG; j++) {
			temp = readl(LVTSMONCTL0_0 + g_lvts_controller_addrs[j]
				+ offset);
			g_lvts_controller_value_e[ctrl_num][j] = temp;
		}
	//}
}

static void read_device_reg_before_active(void)
{
	int i, j;
	unsigned int addr, data;

	for (i = 0; i < ARRAY_SIZE(lvts_tscpu_g_tc); i++) {
		for (j = 0; j < NUM_LVTS_DEVICE_REG; j++) {
			addr = g_lvts_device_addrs[j];
			data =  lvts_read_device(0xC1020000, addr, i);
			g_lvts_device_value_b[i][j] = data;
		}
	}
}

static void read_device_reg_when_error(void)
{
	int i, j, offset, cnt;
	unsigned int addr;

	for (i = 0; i < ARRAY_SIZE(lvts_tscpu_g_tc); i++) {

		offset = lvts_tscpu_g_tc[i].tc_offset; //tc offset

		for (j = 0; j < NUM_LVTS_DEVICE_REG; j++) {
			addr = g_lvts_device_addrs[j];
			lvts_write_device(0xC1020000, addr, 0x00, i);
			/* wait 2us + 3us buffer*/
			udelay(5);
			/* Check ASIF bus status for transaction finished
			 * Wait until DEVICE_ACCESS_START = 0
			 */
			cnt = 0;
			while ((readl(LVTS_CONFIG_0 + offset) & _BIT_(24))) {
				cnt++;

				if (cnt == 100) {
					lvts_printk("Error: DEVICE_ACCESS_START didn't ready\n");
					break;
				}
				udelay(2);
			}

			g_lvts_device_value_e[i][j] = (readl(LVTSRDATA0_0
				+ offset));
		}
	}
}

static void read_device_reg_when_error_by_ctrl_num(int ctrl_num)
{
	int j, offset, cnt;
	unsigned int addr;

	//for (i = 0; i < ARRAY_SIZE(lvts_tscpu_g_tc); i++) {

		offset = lvts_tscpu_g_tc[ctrl_num].tc_offset; //tc offset

		for (j = 0; j < NUM_LVTS_DEVICE_REG; j++) {
			addr = g_lvts_device_addrs[j];
			lvts_write_device(0x81020000, addr, 0x00, ctrl_num);
			/* wait 2us + 3us buffer*/
			udelay(5);
			/* Check ASIF bus status for transaction finished
			 * Wait until DEVICE_ACCESS_START = 0
			 */
			cnt = 0;
			while ((readl(LVTS_CONFIG_0 + offset) & _BIT_(24))) {
				cnt++;

				if (cnt == 100) {
					lvts_printk("Error: DEVICE_ACCESS_START didn't ready\n");
					break;
				}
				udelay(2);
			}

			g_lvts_device_value_e[ctrl_num][j] = (readl(LVTSRDATA0_0
				+ offset));
		}
	//}
}


void clear_lvts_register_value_array(void)
{
	int i, j;

	for (i = 0; i < ARRAY_SIZE(lvts_tscpu_g_tc); i++) {
		for (j = 0; j < NUM_LVTS_CONTROLLER_REG; j++) {
			g_lvts_controller_value_b[i][j] = 0;
			g_lvts_controller_value_e[i][j] = 0;
		}

		for (j = 0; j < NUM_LVTS_DEVICE_REG; j++) {
			g_lvts_device_value_b[i][j] = 0;
			g_lvts_device_value_e[i][j] = 0;
		}
	}
}

static void dump_lvts_register_value_by_ctrl_num(int ctrl_num)
{
	int j, offset, tc_offset;
	char buffer[512];

	//for (i = 0; i < ARRAY_SIZE(lvts_tscpu_g_tc); i++) {
		lvts_printk("[LVTS_ERROR][BEFROE][CONTROLLER_%d][DUMP]\n", ctrl_num);
		tc_offset = lvts_tscpu_g_tc[ctrl_num].tc_offset; //tc offset

		offset = sprintf(buffer, "[LVTS_ERROR][BEFORE][TC][DUMP] ");
		for (j = 0; j < NUM_LVTS_CONTROLLER_REG; j++)
			offset += sprintf(buffer + offset, "0x%x:%x ",
					tc_offset + g_lvts_controller_addrs[j],
					g_lvts_controller_value_b[ctrl_num][j]);

		buffer[offset] = '\0';
		lvts_printk("%s\n", buffer);

		offset = sprintf(buffer, "[LVTS_ERROR][BEFORE][DEVICE][DUMP] ");
		for (j = 0; j < NUM_LVTS_DEVICE_REG; j++)
			offset += sprintf(buffer + offset, "0x%x:%x ",
					g_lvts_device_addrs[j],
					g_lvts_device_value_b[ctrl_num][j]);

		buffer[offset] = '\0';
		lvts_printk("%s\n", buffer);
	//}

	//for (i = 0; i < ARRAY_SIZE(lvts_tscpu_g_tc); i++) {
		lvts_printk("[LVTS_ERROR][AFTER][CONTROLLER_%d][DUMP]\n", ctrl_num);
		tc_offset = lvts_tscpu_g_tc[ctrl_num].tc_offset; //tc offset

		offset = sprintf(buffer, "[LVTS_ERROR][AFTER][TC][DUMP] ");
		for (j = 0; j < NUM_LVTS_CONTROLLER_REG; j++)
			offset += sprintf(buffer + offset, "0x%x:%x ",
					tc_offset + g_lvts_controller_addrs[j],
					g_lvts_controller_value_e[ctrl_num][j]);

		buffer[offset] = '\0';
		lvts_printk("%s\n", buffer);

		offset = sprintf(buffer, "[LVTS_ERROR][AFTER][DEVICE][DUMP] ");
		for (j = 0; j < NUM_LVTS_DEVICE_REG; j++)
			offset += sprintf(buffer + offset, "0x%x:%x ",
					g_lvts_device_addrs[j],
					g_lvts_device_value_e[ctrl_num][j]);

		buffer[offset] = '\0';
		lvts_printk("%s\n", buffer);
	//}
}

static void dump_lvts_register_value(void)
{
	int i, j, offset, tc_offset;
	char buffer[512];

	for (i = 0; i < ARRAY_SIZE(lvts_tscpu_g_tc); i++) {
		lvts_printk("[LVTS_ERROR][BEFROE][CONTROLLER_%d][DUMP]\n", i);
		tc_offset = lvts_tscpu_g_tc[i].tc_offset; //tc offset

		offset = sprintf(buffer, "[LVTS_ERROR][BEFORE][TC][DUMP] ");
		for (j = 0; j < NUM_LVTS_CONTROLLER_REG; j++)
			offset += sprintf(buffer + offset, "0x%x:%x ",
					tc_offset + g_lvts_controller_addrs[j],
					g_lvts_controller_value_b[i][j]);

		buffer[offset] = '\0';
		lvts_printk("%s\n", buffer);

		offset = sprintf(buffer, "[LVTS_ERROR][BEFORE][DEVICE][DUMP] ");
		for (j = 0; j < NUM_LVTS_DEVICE_REG; j++)
			offset += sprintf(buffer + offset, "0x%x:%x ",
					g_lvts_device_addrs[j],
					g_lvts_device_value_b[i][j]);

		buffer[offset] = '\0';
		lvts_printk("%s\n", buffer);
	}

	for (i = 0; i < ARRAY_SIZE(lvts_tscpu_g_tc); i++) {
		lvts_printk("[LVTS_ERROR][AFTER][CONTROLLER_%d][DUMP]\n", i);
		tc_offset = lvts_tscpu_g_tc[i].tc_offset; //tc offset

		offset = sprintf(buffer, "[LVTS_ERROR][AFTER][TC][DUMP] ");
		for (j = 0; j < NUM_LVTS_CONTROLLER_REG; j++)
			offset += sprintf(buffer + offset, "0x%x:%x ",
					tc_offset + g_lvts_controller_addrs[j],
					g_lvts_controller_value_e[i][j]);

		buffer[offset] = '\0';
		lvts_printk("%s\n", buffer);

		offset = sprintf(buffer, "[LVTS_ERROR][AFTER][DEVICE][DUMP] ");
		for (j = 0; j < NUM_LVTS_DEVICE_REG; j++)
			offset += sprintf(buffer + offset, "0x%x:%x ",
					g_lvts_device_addrs[j],
					g_lvts_device_value_e[i][j]);

		buffer[offset] = '\0';
		lvts_printk("%s\n", buffer);
	}
}

void dump_lvts_error_info(void)
{
	read_controller_reg_when_error();

	lvts_disable_all_sensing_points();
	lvts_wait_for_all_sensing_point_idle();

	read_device_reg_when_error();
	dump_lvts_register_value();
}

void dump_lvts_error_info_by_ctrl_num(int crtl_num)
{
	read_controller_reg_when_error_by_ctrl_num(crtl_num);

	lvts_disable_all_sensing_points();
	lvts_wait_for_all_sensing_point_idle();

	read_device_reg_when_error_by_ctrl_num(crtl_num);
	dump_lvts_register_value_by_ctrl_num(crtl_num);
}

#endif

static void lvts_device_check_counting_status(int tc_num)
{
	/* Check this when LVTS device is counting for
	 * a temperature or a RC now
	 */

	int offset, cnt;

	offset = lvts_tscpu_g_tc[tc_num].tc_offset; //tc offset

		cnt = 0;
		while ((readl(LVTS_CONFIG_0 + offset) & _BIT_(25))) {
			cnt++;

		if (cnt == 100) {
			lvts_printk("Error: DEVICE_SENSING_STATUS didn't ready\n");
			break;
		}
		udelay(2);
	}
}

#if defined(CFG_THERM_USE_BOOTUP_COUNT_RC)
void lvts_device_read_count_RC_N_resume(void)
{
	/* Resistor-Capacitor Calibration */
	/* count_RC_N: count RC now */
	int i, j, offset, num_ts, s_index __maybe_unused;
	unsigned int data __maybe_unused;
	char buffer[512];

	for (i = 0; i < ARRAY_SIZE(lvts_tscpu_g_tc); i++) {

		offset = lvts_tscpu_g_tc[i].tc_offset;
		num_ts = lvts_tscpu_g_tc[i].ts_number;

		/* Set LVTS Manual-RCK operation */
		lvts_write_device(0xC1030000, 0x0E, 0x00, i);

		for (j = 0; j < num_ts; j++) {
			s_index = lvts_tscpu_g_tc[i].ts[j];

			/* Select sensor-N with RCK */
			lvts_write_device(0xC1030000, 0x0D, j, i);
			/* Set Device Low-Power Single mode */
			lvts_write_device(0xC1030000, 0x06, 0xB8, i);
			/* Set Device Counting windows */
			lvts_write_device(0xC1030000, 0x05, 0x00, i);
			lvts_write_device(0xC1030000, 0x04, 0x20, i);
			/* Kick-off RCK counting */
			lvts_write_device(0xC1030000, 0x03, 0x02, i);
			/* wait 40us + 10us buffer */
			udelay(50);
			lvts_device_check_counting_status(i);

			/* Get RCK count data (sensor-N) */
			data = lvts_read_device(0xC1020000, 0x00, i);

			/* Get RCK value from LSB[23:0] */
			// not assign new rc value when resume LVTS
			//g_count_rc_now[s_index] = (data & _BITMASK_(23:0));
			/* Recover Setting for Normal Access on
			 * temperature fetch
			 */
			/* Select Sensor-N without RCK */
			lvts_write_device(0xC1030000, 0x0D, (0x10 | j), i);
		}

	}

	offset = snprintf(buffer, sizeof(buffer), "[COUNT_RC_NOW] ");
	for (i = 0; i < L_TS_LVTS_NUM; i++) {

		if (((sizeof(buffer) - offset) <= 0) || (offset < 0)) {
			lvts_printk("%s %d error\n", __func__, __LINE__);
			break;
		}

		offset += snprintf(buffer + offset,
			(sizeof(buffer) - offset), "%d:%d ",
			i, g_count_rc_now[i]);
	}

	lvts_printk("%s\n", buffer);

#if DUMP_LVTS_REGISTER_FOR_ZERO_RAW_ISSUE
	read_device_reg_before_active();
#endif
}
#endif

void lvts_device_read_count_RC_N(void)
{
	/* Resistor-Capacitor Calibration */
	/* count_RC_N: count RC now */
	int i, j, offset, num_ts, s_index;
	unsigned int data;
	char buffer[512];

	for (i = 0; i < ARRAY_SIZE(lvts_tscpu_g_tc); i++) {

		offset = lvts_tscpu_g_tc[i].tc_offset;
		num_ts = lvts_tscpu_g_tc[i].ts_number;

		/* Set LVTS Manual-RCK operation */
		lvts_write_device(0xC1030000, 0x0E, 0x00, i);

		for (j = 0; j < num_ts; j++) {
			s_index = lvts_tscpu_g_tc[i].ts[j];

			/* Select sensor-N with RCK */
			lvts_write_device(0xC1030000, 0x0D, j, i);
			/* Set Device Low-Power Single mode */
			lvts_write_device(0xC1030000, 0x06, 0xB8, i);
			/* Set Device Counting windows */
			lvts_write_device(0xC1030000, 0x05, 0x00, i);
			lvts_write_device(0xC1030000, 0x04, 0x20, i);
			/* Kick-off RCK counting */
			lvts_write_device(0xC1030000, 0x03, 0x02, i);
			/* wait 40us + 10us buffer */
			udelay(50);
			lvts_device_check_counting_status(i);

			/* Get RCK count data (sensor-N) */
			data = lvts_read_device(0xC1020000, 0x00, i);

			/* Get RCK value from LSB[23:0] */
			g_count_rc_now[s_index] = (data & _BITMASK_(23:0));
			/* Recover Setting for Normal Access on
			 * temperature fetch
			 */
			/* Select Sensor-N without RCK */
			lvts_write_device(0xC1030000, 0x0D, (0x10 | j), i);
		}

	}


	offset = snprintf(buffer, sizeof(buffer), "[COUNT_RC_NOW] ");
	for (i = 0; i < L_TS_LVTS_NUM; i++) {

		if (((sizeof(buffer) - offset) <= 0) || (offset < 0)) {
			lvts_printk("%s %d error\n", __func__, __LINE__);
			break;
		}

		offset += snprintf(buffer + offset,
				(sizeof(buffer) - offset), "%d:%d ",
				i, g_count_rc_now[i]);
	}

	lvts_printk("%s\n", buffer);

#if DUMP_LVTS_REGISTER_FOR_ZERO_RAW_ISSUE
	read_device_reg_before_active();
#endif
}

void lvts_device_enable_auto_rck(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(lvts_tscpu_g_tc); i++) {
		/* Set LVTS AUTO-RCK operation */
		lvts_write_device(0xC1030000, 0x0E, 0x01, i);
	}
}

void lvts_efuse_setting(void)
{
	__u32 offset;
	int i, j, s_index;
	int efuse_data;

	for (i = 0; i < ARRAY_SIZE(lvts_tscpu_g_tc); i++) {
		for (j = 0; j < lvts_tscpu_g_tc[i].ts_number; j++) {

			offset = lvts_tscpu_g_tc[i].tc_offset;
			s_index = lvts_tscpu_g_tc[i].ts[j];

#if LVTS_DEVICE_AUTO_RCK == 0
			efuse_data =
			(((unsigned long long)g_count_rc_now[s_index]) *
				g_count_r[s_index]) >> 14;
#else
			efuse_data = g_count_r[s_index];
#endif

			switch (j) {
			case 0:
				mt_reg_sync_writel_print(efuse_data,
					LVTSEDATA00_0 + offset);
				lvts_dbg_printk("efuse LVTSEDATA00_%d 0x%x\n",
					i, readl(LVTSEDATA00_0 + offset));
				break;

			case 1:
				mt_reg_sync_writel_print(efuse_data,
					LVTSEDATA01_0 + offset);
				lvts_dbg_printk("efuse LVTSEDATA01_%d 0x%x\n",
					i, readl(LVTSEDATA01_0 + offset));
				break;
			case 2:
				mt_reg_sync_writel_print(efuse_data,
					LVTSEDATA02_0 + offset);
				lvts_dbg_printk("efuse LVTSEDATA02_%d 0x%x\n",
					i, readl(LVTSEDATA02_0 + offset));
				break;
			case 3:
				mt_reg_sync_writel_print(efuse_data,
					LVTSEDATA03_0 + offset);
				lvts_dbg_printk("efuse LVTSEDATA03_%d 0x%x\n",
					i, readl(LVTSEDATA03_0 + offset));
				break;
			default:
				lvts_dbg_printk("%s, illegal ts order : %d!!\n",
								__func__, j);
			}
		}
	}

}

#if CONFIG_LVTS_ERROR_AEE_WARNING
void dump_efuse_data(void)
{
	int i, efuse, offset;
	char buffer[512];

	lvts_printk("[LVTS_ERROR][GOLDEN_TEMP][DUMP] %d\n", g_golden_temp);

	offset = sprintf(buffer, "[LVTS_ERROR][COUNT_R][DUMP] ");
	for (i = 0; i < L_TS_LVTS_NUM; i++)
		offset += sprintf(buffer + offset, "%d:%d ", i, g_count_r[i]);

	buffer[offset] = '\0';
	lvts_printk("%s\n", buffer);

	offset = sprintf(buffer, "[LVTS_ERROR][COUNT_RC][DUMP] ");
	for (i = 0; i < L_TS_LVTS_NUM; i++)
		offset += sprintf(buffer + offset, "%d:%d ", i, g_count_rc[i]);

	buffer[offset] = '\0';
	lvts_printk("%s\n", buffer);

	offset = sprintf(buffer, "[LVTS_ERROR][COUNT_RC_NOW][DUMP] ");
	for (i = 0; i < L_TS_LVTS_NUM; i++)
		offset += sprintf(buffer + offset, "%d:%d ",
				i, g_count_rc_now[i]);

	buffer[offset] = '\0';
	lvts_printk("%s\n", buffer);

	offset = sprintf(buffer, "[LVTS_ERROR][LVTSEDATA][DUMP] ");
	for (i = 0; i < L_TS_LVTS_NUM; i++) {
#if LVTS_DEVICE_AUTO_RCK == 0
		efuse = g_count_rc_now[i] * g_count_r[i];
#else
		efuse = g_count_r[i];
#endif
		offset += sprintf(buffer + offset, "%d:%d ", i, efuse);
	}

	buffer[offset] = '\0';
	lvts_printk("%s\n", buffer);
}

int check_lvts_mcu_efuse(void)
{
	return (g_use_fake_efuse)?(0):(1);
}
#endif

void lvts_device_identification(void)
{
	int tc_num, data, offset;

	for (tc_num = 0; tc_num < ARRAY_SIZE(lvts_tscpu_g_tc); tc_num++) {

		offset = lvts_tscpu_g_tc[tc_num].tc_offset;

		/*  Enable LVTS_CTRL Clock */
		mt_reg_sync_writel_print(0x00000001, LVTSCLKEN_0 + offset);

		/*  Reset All Devices */
		lvts_write_device(0xC1030000, 0xFF, 0xFF, tc_num);
		/* udelay(100); */

		/*  Read back Dev_ID with Update */
		lvts_write_device(0xC5020000, 0xFC, 0x55, tc_num);

		/*  Check LVTS device ID */
		data = (readl(LVTS_ID_0 + offset) & _BITMASK_(7:0));
		if (data != (0x81 + tc_num))
			lvts_printk("LVTS_TC_%d, Device ID should be 0x%x, but 0x%x\n",
				tc_num, (0x81 + tc_num), data);
	}
}

void lvts_reset_device_and_stop_clk(void)
{
	__u32 offset;
	int tc_num;

	for (tc_num = 0; tc_num < ARRAY_SIZE(lvts_tscpu_g_tc); tc_num++) {

		offset = lvts_tscpu_g_tc[tc_num].tc_offset;

		/*  Reset All Devices */
		lvts_write_device(0xC1030000, 0xFF, 0xFF, tc_num);

		/*  Disable LVTS_CTRL Clock */
		mt_reg_sync_writel_print(0x00000000, LVTSCLKEN_0 + offset);
	}
}

void lvts_Device_Enable_Init_all_Devices(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(lvts_tscpu_g_tc); i++) {
		/* Stop Counting (RG_TSFM_ST=0) */
		lvts_write_device(0xC1030000, 0x03, 0x00, i);
		/* Auto RCK */
		lvts_write_device(0xC1030000, 0x0E, 0x01, i);
		/* Low power single mode */
		lvts_write_device(0xC1030000, 0x06, 0xB8, i);
		/* RG_TSFM_LPDLY[1:0]=2' 10 */
		lvts_write_device(0xC1030000, 0x07, 0xA6, i);
		/* Set LVTS device counting window 20us */
		lvts_write_device(0xC1030000, 0x05, 0x00, i);
		lvts_write_device(0xC1030000, 0x04, 0x20, i);
		/* TSV2F_CHOP_CKSEL & TSV2F_EN */
		lvts_write_device(0xC1030000, 0x0A, 0xAC, i);
		/* TSBG_DEM_CKSEL * TSBG_CHOP_EN */
		lvts_write_device(0xC1030000, 0x0C, 0xFC, i);
		/* Set TS_RSV */
		lvts_write_device(0xC1030000, 0x09, 0x8D, i);
		/* Set TS_CHOP control */
		lvts_write_device(0xC1030000, 0x08, 0xF7, i);
		/* Set TSV2F_RSV */
		lvts_write_device(0xC1030000, 0x0B, 0x04, i);
	}
}

static void *read_mtk_efuse_cell(char *nvmem_cell_name)
{
	struct nvmem_cell *efuse_cell;
	u32 *efuse_buf = NULL;
	size_t len;
	struct device_node *dev_node;

	dev_node = of_find_node_by_name(NULL, "lvts");
	if (!dev_node) {
		pr_info("get lvts node fail\n");
		return NULL;
	}
	efuse_cell = of_nvmem_cell_get(dev_node, nvmem_cell_name);
	if (IS_ERR(efuse_cell)) {
		pr_info("@%s: cannot get %s\n", __func__, nvmem_cell_name);
		return NULL;
	}
	efuse_buf = (unsigned int *)nvmem_cell_read(efuse_cell, &len);
	nvmem_cell_put(efuse_cell);

	if (IS_ERR(efuse_buf)) {
		pr_info("@%s: cannot get efuse_buf : %s\n", __func__, nvmem_cell_name);
		return NULL;
	}

	return efuse_buf;

}

void lvts_thermal_cal_prepare(struct platform_device *dev)
{	unsigned int temp[16];
	int i, offset;
	char buffer[512];
	__u32  *buf = NULL;

	buf = read_mtk_efuse_cell("e_data1");
	if (!buf)
		return;
	temp[0] = buf[0];
	kfree(buf);// free effuse buffer
	buf = NULL;

	buf = read_mtk_efuse_cell("e_data2");
	if (!buf)
		return;
	temp[1] = buf[0];
	temp[2] = buf[1];
	kfree(buf);// free effuse buffer
	buf = NULL;

	buf = read_mtk_efuse_cell("e_data3");
	if (!buf)
		return;
	temp[3] = buf[0];
	temp[4] = buf[1];
	temp[5] = buf[2];
	temp[6] = buf[3];
	temp[7] = buf[4];
	temp[8] = buf[5];
	temp[9] = buf[6];
	temp[10] = buf[7];
	temp[11] = buf[8];
	temp[12] = buf[9];
	temp[13] = buf[10];
	temp[14] = buf[11];
	temp[15] = buf[12];
	kfree(buf);// free effuse buffer

	for (i = 0; (i + 4) < 16; i = i + 4)
		lvts_printk("[lvts_call] %d: 0x%x, %d: 0x%x, %d: 0x%x, %d: 0x%x\n",
		i, temp[i], i + 1, temp[i + 1], i + 2, temp[i + 2],
		i + 3, temp[i + 3]);
	lvts_printk("[lvts_call] 12: 0x%x, 13: 0x%x, 14: 0x%x, 15: 0x%x\n",
		temp[12], temp[13], temp[14], temp[15]);

	g_golden_temp = (temp[0] & _BITMASK_(7:0));
	g_count_r[0] = (temp[1] & _BITMASK_(23:0));
	g_count_r[1] = (temp[2] & _BITMASK_(23:0));
	g_count_r[2] = (temp[3] & _BITMASK_(23:0));
	g_count_r[3] = (temp[4] & _BITMASK_(23:0));
	g_count_r[4] = (temp[5] & _BITMASK_(23:0));
	g_count_r[5] = (temp[6] & _BITMASK_(23:0));
	g_count_r[6] = (temp[7] & _BITMASK_(23:0));
	g_count_r[7] = (temp[8] & _BITMASK_(23:0));
	g_count_r[8] = (temp[9] & _BITMASK_(23:0));


	g_count_rc[0] = (temp[11] & _BITMASK_(23:0));

	g_count_rc[1] = (temp[12] & _BITMASK_(23:0));

	g_count_rc[2] = (temp[13] & _BITMASK_(23:0));

	g_count_rc[3] = (temp[14] & _BITMASK_(23:0));

	g_count_rc[4] = (temp[15] & _BITMASK_(23:0));

	g_count_rc[5] = ((temp[1] & _BITMASK_(31:24)) >> 8) +
		((temp[2] & _BITMASK_(31:24)) >> 16) +
		((temp[3] & _BITMASK_(31:24)) >> 24);

	g_count_rc[6] = ((temp[4] & _BITMASK_(31:24)) >> 8) +
		((temp[5] & _BITMASK_(31:24)) >> 16) +
		((temp[6] & _BITMASK_(31:24)) >> 24);

	g_count_rc[7] = ((temp[7] & _BITMASK_(31:24)) >> 8) +
		((temp[8] & _BITMASK_(31:24)) >> 16) +
		((temp[9] & _BITMASK_(31:24)) >> 24);

	g_count_rc[8] = ((temp[10] & _BITMASK_(31:24)) >> 8) +
		((temp[11] & _BITMASK_(31:24)) >> 16) +
		((temp[12] & _BITMASK_(31:24)) >> 24);


	for (i = 0; i < L_TS_LVTS_NUM; i++) {
		if (i == 0) {
			if ((temp[0] & _BITMASK_(7:0)) != 0)
				break;
		} else {
			if (temp[i] != 0)
				break;
		}
	}

	if (i == L_TS_LVTS_NUM) {
		/* It means all efuse data are equal to 0 */
		lvts_printk(
			"[lvts_cal] This sample is not calibrated, fake !!\n");

		g_golden_temp = DEFAULT_EFUSE_GOLDEN_TEMP;
		for (i = 0; i < L_TS_LVTS_NUM; i++) {
			g_count_r[i] = DEFAULT_EFUSE_COUNT;
			g_count_rc[i] = DEFAULT_EFUSE_COUNT_RC;
		}

		g_use_fake_efuse = 1;
	}

	lvts_printk("[lvts_cal] g_golden_temp = %d\n", g_golden_temp);

	offset = snprintf(buffer, sizeof(buffer),
		"[lvts_cal] num:g_count_r:g_count_rc ");
	for (i = 0; i < L_TS_LVTS_NUM; i++) {

		if (((sizeof(buffer) - offset) <= 0) || (offset < 0)) {
			lvts_printk("%s %d error\n", __func__, __LINE__);
			break;
		}

		offset += snprintf(buffer + offset,
				(sizeof(buffer) - offset), "%d:%d:%d ",
				i, g_count_r[i], g_count_rc[i]);
	}

	lvts_printk("%s\n", buffer);
}

#if THERMAL_ENABLE_TINYSYS_SSPM || THERMAL_ENABLE_ONLY_TZ_SSPM
void lvts_ipi_send_efuse_data(void)
{
	struct thermal_ipi_data thermal_data;


	thermal_data.u.data.arg[0] = g_golden_temp;
	thermal_data.u.data.arg[1] = 0;
	thermal_data.u.data.arg[2] = 0;
	while (thermal_to_sspm(THERMAL_IPI_LVTS_INIT_GRP1, &thermal_data) != 0)
		udelay(100);
}
#endif


#if THERMAL_ENABLE_TINYSYS_SSPM || THERMAL_ENABLE_ONLY_TZ_SSPM
#if defined(THERMAL_SSPM_THERMAL_THROTTLE_SWITCH)
void lvts_ipi_send_sspm_thermal_thtottle(void)
{
	struct thermal_ipi_data thermal_data;


	thermal_data.u.data.arg[0] = tscpu_sspm_thermal_throttle;
	thermal_data.u.data.arg[1] = 0;
	thermal_data.u.data.arg[2] = 0;
	while (thermal_to_sspm(THERMAL_IPI_SET_DIS_THERMAL_THROTTLE,
		&thermal_data) != 0)
		udelay(100);
}
#endif

#if defined(THERMAL_KERNEL_SUSPEND_RESUME_NOTIFY)
void lvts_ipi_send_sspm_thermal_suspend_resume(int is_suspend)
{
	struct thermal_ipi_data thermal_data;

	//lvts_printk("%s, is_suspend %d\n", __func__, is_suspend);

	thermal_data.u.data.arg[0] = is_suspend;
	thermal_data.u.data.arg[1] = 0;
	thermal_data.u.data.arg[2] = 0;
	while (thermal_to_sspm(THERMAL_IPI_SUSPEND_RESUME_NOTIFY,
		&thermal_data) != 0)
		udelay(100);
}
#endif
#endif

static unsigned int lvts_temp_to_raw(int temp, enum lvts_sensor_enum ts_name)
{
	/* MSR_RAW = ((temp[i] - GOLDEN_TEMP/2 - b) * 16384) / a
	 * a = -204.65
	 * b =  204.65
	 */
	unsigned int msr_raw = 0;

	msr_raw = ((long long)(((long long)g_golden_temp * 500 +
		LVTS_COEFF_B_X_1000 - temp)) << 14)/(-1 * LVTS_COEFF_A_X_1000);

	lvts_dbg_printk("%s msr_raw = 0x%x,temp=%d\n", __func__, msr_raw, temp);

	return msr_raw;
}

static void lvts_interrupt_handler(int tc_num)
{
	unsigned int  ret = 0;
	int offset;

	offset = lvts_tscpu_g_tc[tc_num].tc_offset;

		ret = readl(offset + LVTSMONINTSTS_0);
		/* Write back to clear interrupt status */
		mt_reg_sync_writel(ret, offset + LVTSMONINTSTS_0);

	lvts_printk("[Thermal IRQ] LVTS thermal controller %d, LVTSMONINTSTS=0x%08x\n",
		tc_num, ret);

		if (ret & THERMAL_COLD_INTERRUPT_0)
			lvts_dbg_printk("[Thermal IRQ]: Cold interrupt triggered, sensor point 0\n");

		if (ret & THERMAL_HOT_INTERRUPT_0)
			lvts_dbg_printk("[Thermal IRQ]: Hot interrupt triggered, sensor point 0\n");

		if (ret & THERMAL_LOW_OFFSET_INTERRUPT_0)
			lvts_dbg_printk("[Thermal IRQ]: Low offset interrupt triggered, sensor point 0\n");

		if (ret & THERMAL_HIGH_OFFSET_INTERRUPT_0)
			lvts_dbg_printk("[Thermal IRQ]: High offset interrupt triggered, sensor point 0\n");

		if (ret & THERMAL_HOT2NORMAL_INTERRUPT_0)
			lvts_dbg_printk("[Thermal IRQ]: Hot to normal interrupt triggered, sensor point 0\n");

		if (ret & THERMAL_COLD_INTERRUPT_1)
			lvts_dbg_printk("[Thermal IRQ]: Cold interrupt triggered, sensor point 1\n");

		if (ret & THERMAL_HOT_INTERRUPT_1)
			lvts_dbg_printk("[Thermal IRQ]: Hot interrupt triggered, sensor point 1\n");

		if (ret & THERMAL_LOW_OFFSET_INTERRUPT_1)
			lvts_dbg_printk("[Thermal IRQ]: Low offset interrupt triggered, sensor point 1\n");

		if (ret & THERMAL_HIGH_OFFSET_INTERRUPT_1)
			lvts_dbg_printk("[Thermal IRQ]: High offset interrupt triggered, sensor point 1\n");

		if (ret & THERMAL_HOT2NORMAL_INTERRUPT_1)
			lvts_dbg_printk("[Thermal IRQ]: Hot to normal interrupt triggered, sensor point 1\n");

		if (ret & THERMAL_COLD_INTERRUPT_2)
			lvts_dbg_printk("[Thermal IRQ]: Cold interrupt triggered, sensor point 2\n");

		if (ret & THERMAL_HOT_INTERRUPT_2)
			lvts_dbg_printk("[Thermal IRQ]: Hot interrupt triggered, sensor point 2\n");

		if (ret & THERMAL_LOW_OFFSET_INTERRUPT_2)
			lvts_dbg_printk("[Thermal IRQ]: Low offset interrupt triggered, sensor point 2\n");

		if (ret & THERMAL_HIGH_OFFSET_INTERRUPT_2)
			lvts_dbg_printk("[Thermal IRQ]: High offset interrupt triggered, sensor point 2\n");

		if (ret & THERMAL_HOT2NORMAL_INTERRUPT_2)
			lvts_dbg_printk("[Thermal IRQ]: Hot to normal interrupt triggered, sensor point 2\n");

		if (ret & THERMAL_DEVICE_TIMEOUT_INTERRUPT)
			lvts_dbg_printk("[Thermal IRQ]: Device access timeout triggered\n");

		if (ret & THERMAL_IMMEDIATE_INTERRUPT_0)
			lvts_dbg_printk("[Thermal IRQ]: Immediate sense interrupt triggered, sensor point 0\n");

		if (ret & THERMAL_IMMEDIATE_INTERRUPT_1)
			lvts_dbg_printk("[Thermal IRQ]: Immediate sense interrupt triggered, sensor point 1\n");

		if (ret & THERMAL_IMMEDIATE_INTERRUPT_2)
			lvts_dbg_printk("[Thermal IRQ]: Immediate sense interrupt triggered, sensor point 2\n");

		if (ret & THERMAL_FILTER_INTERRUPT_0)
			lvts_dbg_printk("[Thermal IRQ]: Filter sense interrupt triggered, sensor point 0\n");

		if (ret & THERMAL_FILTER_INTERRUPT_1)
			lvts_dbg_printk("[Thermal IRQ]: Filter sense interrupt triggered, sensor point 1\n");

		if (ret & THERMAL_FILTER_INTERRUPT_2)
			lvts_dbg_printk("[Thermal IRQ]: Filter sense interrupt triggered, sensor point 2\n");

		if (ret & THERMAL_COLD_INTERRUPT_3)
			lvts_dbg_printk("[Thermal IRQ]: Cold interrupt triggered, sensor point 3\n");

		if (ret & THERMAL_HOT_INTERRUPT_3)
			lvts_dbg_printk("[Thermal IRQ]: Hot interrupt triggered, sensor point 3\n");

		if (ret & THERMAL_LOW_OFFSET_INTERRUPT_3)
			lvts_dbg_printk("[Thermal IRQ]: Low offset interrupt triggered, sensor point 3\n");

		if (ret & THERMAL_HIGH_OFFSET_INTERRUPT_3)
			lvts_dbg_printk("[Thermal IRQ]: High offset triggered, sensor point 3\n");

		if (ret & THERMAL_HOT2NORMAL_INTERRUPT_3)
			lvts_dbg_printk("[Thermal IRQ]: Hot to normal interrupt triggered, sensor point 3\n");

	if (ret & THERMAL_IMMEDIATE_INTERRUPT_3)
		lvts_dbg_printk("[Thermal IRQ]: Immediate sense interrupt triggered, sensor point 3\n");

	if (ret & THERMAL_FILTER_INTERRUPT_3)
		lvts_dbg_printk("[Thermal IRQ]: Filter sense interrupt triggered, sensor point 3\n");

		if (ret & THERMAL_PROTECTION_STAGE_1)
			lvts_dbg_printk("[Thermal IRQ]: Thermal protection stage 1 interrupt triggered\n");

		if (ret & THERMAL_PROTECTION_STAGE_2) {
			lvts_dbg_printk("[Thermal IRQ]: Thermal protection stage 2 interrupt triggered\n");
#if MTK_TS_CPU_RT
			wake_up_process(ktp_thread_handle);
#endif
	}

	if (ret & THERMAL_PROTECTION_STAGE_3)
		lvts_printk("[Thermal IRQ]: Thermal protection stage 3 interrupt triggered, Thermal HW reboot\n");
}

irqreturn_t lvts_tscpu_thermal_all_tc_interrupt_handler(int irq, void *dev_id)
{
	unsigned int ret = 0, i, mask = 1;

	ret = readl(THERMINTST);
	ret = ret & 0x1F;
	/* MSB LSB NAME
	 * 4   4   LVTSINT3
	 * 3   3   LVTSINT2
	 * 2   2   LVTSINT1
	 * 1   1   LVTSINT0
	 * 0   0   THERMINT0
	 */

	lvts_printk("%s : THERMINTST = 0x%x\n", __func__, ret);

	ret = ((ret >> 1) << LVTS_AP_CONTROLLER0);

	for (i = 0; i < ARRAY_SIZE(lvts_tscpu_g_tc); i++) {
		mask = 1 << i;

		if ((ret & mask) == 0)
			lvts_interrupt_handler(i);
	}

	return IRQ_HANDLED;
}

static void lvts_configure_polling_speed_and_filter(int tc_num)
{
	__u32 offset, lvtsMonCtl1, lvtsMonCtl2;

	offset = lvts_tscpu_g_tc[tc_num].tc_offset;

		lvtsMonCtl1 = (((lvts_tscpu_g_tc[tc_num].tc_speed.group_interval_delay
				<< 20) & _BITMASK_(29:20)) |
				(lvts_tscpu_g_tc[tc_num].tc_speed.period_unit &
				_BITMASK_(9:0)));
		lvtsMonCtl2 = (((lvts_tscpu_g_tc[tc_num].tc_speed.filter_interval_delay
				<< 16) & _BITMASK_(25:16)) |
				(lvts_tscpu_g_tc[tc_num].tc_speed.sensor_interval_delay
				& _BITMASK_(9:0)));
		/*
		 * Calculating period unit in Module clock x 256, and the Module clock
		 * will be changed to 26M when Infrasys enters Sleep mode.
		 */

		/*
		 * bus clock 66M counting unit is
		 *			12 * 1/66M * 256 = 12 * 3.879us = 46.545 us
		 */
		mt_reg_sync_writel_print(lvtsMonCtl1, offset + LVTSMONCTL1_0);
		/*
		 *filt interval is 1 * 46.545us = 46.545us,
		 *sen interval is 429 * 46.545us = 19.968ms
		 */
		mt_reg_sync_writel_print(lvtsMonCtl2, offset + LVTSMONCTL2_0);

		/* temperature sampling control, 2 out of 4 samples */
		mt_reg_sync_writel_print(0x00000492, offset + LVTSMSRCTL0_0);

	udelay(1);
	lvts_dbg_printk(
		"%s %d, LVTSMONCTL1_0= 0x%x,LVTSMONCTL2_0= 0x%x,LVTSMSRCTL0_0= 0x%x\n",
		__func__, tc_num,
		readl(LVTSMONCTL1_0 + offset),
		readl(LVTSMONCTL2_0 + offset),
		readl(LVTSMSRCTL0_0 + offset));
}

/*
 * temperature2 to set the middle threshold for interrupting CPU.
 * -275000 to disable it.
 */
static void lvts_set_tc_trigger_hw_protect(
int temperature, int temperature2, int tc_num)
{
	int temp = 0, raw_high, config, offset;
#if LVTS_USE_DOMINATOR_SENSING_POINT
	int d_index;
	enum lvts_sensor_enum ts_name;
#endif

	offset = lvts_tscpu_g_tc[tc_num].tc_offset;

		lvts_dbg_printk("%s t1=%d t2=%d\n",
					__func__, temperature, temperature2);

#if LVTS_USE_DOMINATOR_SENSING_POINT
		if (lvts_tscpu_g_tc[tc_num].dominator_ts_idx <
			lvts_tscpu_g_tc[tc_num].ts_number){
			d_index = lvts_tscpu_g_tc[tc_num].dominator_ts_idx;
		} else {
			lvts_printk("Error: LVTS TC%d, dominator_ts_idx = %d should smaller than ts_number = %d\n",
				tc_num,
				lvts_tscpu_g_tc[tc_num].dominator_ts_idx,
				lvts_tscpu_g_tc[tc_num].ts_number);

			lvts_printk("Use the sensor point 0 as the dominated sensor\n");
			d_index = 0;
		}

		ts_name = lvts_tscpu_g_tc[tc_num].ts[d_index];

		lvts_dbg_printk("%s # in tc%d , the dominator ts_name is %d\n",
							__func__, tc_num, ts_name);

		/* temperature to trigger SPM state2 */
		raw_high = lvts_temp_to_raw(temperature, ts_name);
#else
		raw_high = lvts_temp_to_raw(temperature, 0);
#endif

#ifndef LVTS_DYNAMIC_ENABLE_REBOOT
	temp = readl(offset + LVTSMONINT_0);
	/* disable trigger SPM interrupt */
	mt_reg_sync_writel_print(temp & 0x00000000, offset + LVTSMONINT_0);
#endif

		temp = readl(offset + LVTSPROTCTL_0) & ~(0xF << 16);
#if LVTS_USE_DOMINATOR_SENSING_POINT
		/* Select protection sensor */
		config = ((d_index << 2) + 0x2) << 16;
		mt_reg_sync_writel_print(temp | config, offset + LVTSPROTCTL_0);
#else
		/* Maximum of 4 sensing points */
		config = (0x1 << 16);
		mt_reg_sync_writel_print(temp | config, offset + LVTSPROTCTL_0);
#endif

		/* set hot to HOT wakeup event */
		mt_reg_sync_writel_print(raw_high, offset + LVTSPROTTC_0);

#ifndef CONFIG_LVTS_DYNAMIC_ENABLE_REBOOT
		/* enable trigger Hot SPM interrupt */
		mt_reg_sync_writel_print(temp | 0x80000000, offset + LVTSMONINT_0);
#endif
}

static void dump_lvts_device(int tc_num, __u32 offset)
{
	lvts_printk("%s, LVTS_CONFIG_%d= 0x%x\n", __func__,
				tc_num, readl(LVTS_CONFIG_0 + offset));
	udelay(2);

	//read raw data
	lvts_printk("%s, LVTSRDATA0_%d= 0x%x\n", __func__,
				tc_num, readl(LVTSRDATA0_0 + offset));

	udelay(2);
	lvts_printk("%s, LVTSRDATA1_%d= 0x%x\n", __func__,
				tc_num, readl(LVTSRDATA1_0 + offset));

	udelay(2);
	lvts_printk("%s, LVTSRDATA2_%d= 0x%x\n", __func__,
				tc_num, readl(LVTSRDATA2_0 + offset));

	udelay(2);
	lvts_printk("%s, LVTSRDATA3_%d= 0x%x\n", __func__,
				tc_num, readl(LVTSRDATA3_0 + offset));
}

#if LVTS_VALID_DATA_TIME_PROFILING
void lvts_dump_time_profiling_result(struct seq_file *m)
{
	int i, sum;

	seq_printf(m, "SODI3_count= %llu\n", SODI3_count);
	seq_printf(m, "noValid_count %llu, %d%%\n",
		noValid_count, ((noValid_count * 100) / SODI3_count));
	seq_printf(m, "valid_count %llu, %d%%\n",
		(SODI3_count - noValid_count),
		(((SODI3_count - noValid_count) * 100) / SODI3_count));

	sum = 0;
	for (i = 0; i < NUM_TIME_TH; i++)
		sum += latency_array[i][1];

	seq_printf(m, "Valid count in latency_array = %d\n", sum);

	for (i = 0; i < NUM_TIME_TH; i++) {
		if (i == 0) {
			seq_printf(m, "Count valid latency between 0ms ~ %dms: %d, %d%%\n",
				latency_array[i][0], latency_array[i][1],
				((latency_array[i][1] * 100) / sum));
		} else if (i == (NUM_TIME_TH - 1)) {
			seq_printf(m, "Count valid others: %d, %d%%\n",
				latency_array[i][1],
				((latency_array[i][1] * 100) / sum));
		} else {
			seq_printf(m, "Count valid latency between %dms ~ %dms: %d, %d%%\n",
				latency_array[i - 1][0],
				latency_array[i][0], latency_array[i][1],
				((latency_array[i][1] * 100) / sum));
		}
	}

	/* count if start_timestamp is bigger than end_timestamp */
	seq_printf(m, "diff_error_count= %d\n", diff_error_count);
	seq_printf(m, "Current start_timestamp= %lldus\n", start_timestamp);
	seq_printf(m, "Current end_timestamp= %lldus\n", end_timestamp);
	seq_printf(m, "Current time_diff= %lldus\n", time_diff);
}

static void lvts_count_valid_temp_latency(long long time_diff)
{
	/* time_diff is in microseconds */
	int i;

	for (i = 0; i < (NUM_TIME_TH - 1); i++) {
		if (time_diff < (((long long)latency_array[i][0])
			* 1000)) {
			latency_array[i][1]++;
			break;
		}
	}

	if (i == (NUM_TIME_TH - 1))
		latency_array[i][1]++;
}
#endif


static int lvts_read_tc_raw_and_temp(
		u32 *tempmsr_name, enum lvts_sensor_enum ts_name)
{
	int temp = 0, raw = 0, raw1 = 0, raw2 = 0;

	if (thermal_base == 0)
		return 0;

	if (tempmsr_name == 0)
		return 0;

	raw = readl((tempmsr_name));
	raw1 = (raw & 0x10000) >> 16; //bit 16 : valid bit
	raw2 = raw & 0xFFFF;
	temp = lvts_raw_to_temp(raw2, ts_name);

	if (raw2 == 0) {
		/* 26111 is magic num
		 * this is to keep system alive for a while
		 * to wait HW init done,
		 * because 0 msr raw will translates to 28x'C
		 * and then 28x'C will trigger a SW reset.
		 *
		 * if HW init finish, this msr raw will not be 0,
		 * system can report normal temperature.
		 * if wait over 60 times zero, this means something
		 * wrong with HW, must trigger BUG on and dump useful
		 * register for debug.
		 */

		temp = 26111;
	}


	if (lvts_rawdata_debug_log) {
		lvts_printk(
		"[LVTS_MSR] ts%d msr_all=%x, valid=%d, msr_temp=%d, temp=%d\n",
			ts_name, raw, raw1, raw2, temp);
	}

	tscpu_ts_lvts_temp_r[ts_name] = raw2;
#if CONFIG_LVTS_ERROR_AEE_WARNING
	tscpu_ts_lvts_temp_v[ts_name] = raw1;
#endif
#if LVTS_VALID_DATA_TIME_PROFILING
	if (isTempValid == 0 && raw1 != 0 && SODI3_count != 0) {
		isTempValid = 1;
		end_timestamp = thermal_get_current_time_us();
		time_diff = end_timestamp - start_timestamp;
		if (time_diff < 0) {
			lvts_printk("[LVTS_ERROR] time_diff = %lldus,start_time= %lldus, end_time= %lldus\n",
				time_diff, start_timestamp, end_timestamp);
			diff_error_count++;
		} else {
			lvts_count_valid_temp_latency(time_diff);
		}
	}
#endif

	return temp;
}

static void lvts_tscpu_thermal_read_tc_temp(
		int tc_num, enum lvts_sensor_enum type, int order)
{
	__u32 offset;

	offset = lvts_tscpu_g_tc[tc_num].tc_offset;

		if (lvts_rawdata_debug_log)
			dump_lvts_device(tc_num, offset);

		switch (order) {
		case 0:
			tscpu_ts_lvts_temp[type] =
				lvts_read_tc_raw_and_temp((offset + LVTSMSR0_0), type);
			lvts_dbg_printk("%s order %d tc_num %d type %d temp %d\n",
					__func__, order, tc_num, type,
					tscpu_ts_lvts_temp[type]);
			break;
		case 1:
			tscpu_ts_lvts_temp[type] =
				lvts_read_tc_raw_and_temp((offset + LVTSMSR1_0), type);
			lvts_dbg_printk("%s order %d tc_num %d type %d temp %d\n",
					__func__, order, tc_num, type,
					tscpu_ts_lvts_temp[type]);
			break;
		case 2:
			tscpu_ts_lvts_temp[type] =
				lvts_read_tc_raw_and_temp((offset + LVTSMSR2_0), type);
			lvts_dbg_printk("%s order %d tc_num %d type %d temp %d\n",
					__func__, order, tc_num, type,
					tscpu_ts_lvts_temp[type]);
			break;
		case 3:
			tscpu_ts_lvts_temp[type] =
				lvts_read_tc_raw_and_temp((offset + LVTSMSR3_0), type);
			lvts_dbg_printk("%s order %d tc_num %d type %d temp %d\n",
					__func__, order, tc_num, type,
					tscpu_ts_lvts_temp[type]);
			break;
		default:
			tscpu_ts_lvts_temp[type] =
				lvts_read_tc_raw_and_temp((offset + LVTSMSR0_0), type);
			lvts_dbg_printk("%s order %d tc_num %d type %d temp %d\n",
					__func__, order, tc_num, type,
					tscpu_ts_lvts_temp[type]);
			break;
		}


}

void read_all_tc_lvts_temperature(void)
{
	int i = 0, j = 0;

	for (i = 0; i < ARRAY_SIZE(lvts_tscpu_g_tc); i++)
		for (j = 0; j < lvts_tscpu_g_tc[i].ts_number; j++)
			lvts_tscpu_thermal_read_tc_temp(i,
					lvts_tscpu_g_tc[i].ts[j], j);
}

/* pause ALL periodoc temperature sensing point */
void lvts_pause_all_sensing_points(void)
{
	int i, temp, offset;

	for (i = 0; i < ARRAY_SIZE(lvts_tscpu_g_tc); i++) {
		offset = lvts_tscpu_g_tc[i].tc_offset;
		temp = readl(offset + LVTSMSRCTL1_0);
		/* set bit8=bit1=bit2=bit3=1 to pause sensing point 0,1,2,3 */
		mt_reg_sync_writel_print((temp | 0x10E),
					offset + LVTSMSRCTL1_0);
	}
}

/*
 * lvts_thermal_check_all_sensing_point_idle -
 * Check if all sensing points are idle
 * Return: 0 if all sensing points are idle
 *         an error code if one of them is busy
 * error code[31:16]: an index of LVTS thermal controller
 * error code[2]: bit 10 of LVTSMSRCTL1
 * error code[1]: bit 7 of LVTSMSRCTL1
 * error code[0]: bit 0 of LVTSMSRCTL1
 */
static int lvts_thermal_check_all_sensing_point_idle(void)
{
	int i, temp, offset, error_code;

	for (i = 0; i < ARRAY_SIZE(lvts_tscpu_g_tc); i++) {
		offset = lvts_tscpu_g_tc[i].tc_offset;
		temp = readl(offset + LVTSMSRCTL1_0);
		/* Check if bit10=bit7=bit0=0 */
		if ((temp & 0x481) != 0) {
			error_code = (i << 16) + ((temp & _BIT_(10)) >> 8) +
				((temp & _BIT_(7)) >> 6) +
				(temp & _BIT_(0));

			return error_code;
		}
	}

	return 0;
}

void lvts_wait_for_all_sensing_point_idle(void)
{
	int cnt, temp;

	cnt = 0;
	/*
	 * Wait until all sensoring points idled.
	 * No need to check LVTS status when suspend/resume,
	 * this will spend extra 100us of suspend flow.
	 * LVTS status will be reset after resume.
	 */
	while (cnt < 50 && (tscpu_kernel_status() == 0)) {
		temp = lvts_thermal_check_all_sensing_point_idle();
		if (temp == 0)
			break;

		if ((cnt + 1) % 10 == 0) {
			pr_notice("Cnt= %d LVTS TC %d, LVTSMSRCTL1[10,7,0] = %d,%d,%d, LVTSMSRCTL1[10:0] = 0x%x\n",
					cnt + 1, (temp >> 16),
					((temp & _BIT_(2)) >> 2),
					((temp & _BIT_(1)) >> 1),
					(temp & _BIT_(0)),
					(temp & _BITMASK_(10:0)));
		}

		udelay(2);
		cnt++;
	}
}
/* release ALL periodoc temperature sensing point */
void lvts_release_all_sensing_points(void)
{
	int i = 0, temp;
	__u32 offset;

	for (i = 0; i < ARRAY_SIZE(lvts_tscpu_g_tc); i++) {
		offset = lvts_tscpu_g_tc[i].tc_offset;

		temp = readl(offset + LVTSMSRCTL1_0);
		/* set bit1=bit2=bit3=bit8=0 to release sensing point 0,1,2,3*/
		mt_reg_sync_writel_print(((temp & (~0x10E))),
					offset + LVTSMSRCTL1_0);
	}
}

void lvts_sodi3_release_thermal_controller(void)
{
	/* SPM will close 26M to saving power during SODI3
	 * Because both auxadc thermal controllers and lvts thermal controllers
	 * need 26M to work properly, it would cause thermal controllers to
	 * report an abnormal high or low temperature after leaving SODI3
	 *
	 * The SW workaround solution is that
	 * SPM will pause LVTS thermal controllers before closing 26M, and
	 * try to release LVTS thermal controllers after leaving SODI3
	 * thermal driver check and release LVTS thermal controllers if
	 * necessary after leaving SODI3
	 */
	int i = 0, temp, lvts_paused = 0;
	__u32 offset;


	/*don't need to do release LVTS when suspend/resume*/
	if (tscpu_kernel_status() == 0) {

		/* Check if SPM paused thermal controller */
		for (i = 0; i < ARRAY_SIZE(lvts_tscpu_g_tc); i++) {
			offset = lvts_tscpu_g_tc[i].tc_offset;
			temp = readl(offset + LVTSMSRCTL1_0);
			/* set bit8=bit1=bit2=bit3=1 to pause
			 *sensing point 0,1,2,3
			 */
			if ((temp & 0x10E) != 0) {
				lvts_paused = 1;
				pr_notice_ratelimited(
					"lvts_paused = %d\n", lvts_paused);
				break;
			}
		}

		/* Return if SPM didn't pause thermal controller or
		 * released thermal controllers already
		 */
		if (lvts_paused == 0)
			return;
		/* Wait until all of LVTS thermal controllers are idle
		 * Pause operation has to take time to finish.
		 * if it didn't finish before SPM closed 26M, we have to wait
		 * until it is finished to make sure all LVTS thermal
		 * controllers in a correct finite state machine
		 */

		lvts_wait_for_all_sensing_point_idle();

		lvts_release_all_sensing_points();
	}
}

/*
 * disable ALL periodoc temperature sensing point
 */
void lvts_disable_all_sensing_points(void)
{
	int i = 0, offset;

	for (i = 0; i < ARRAY_SIZE(lvts_tscpu_g_tc); i++) {
		offset = lvts_tscpu_g_tc[i].tc_offset;
		mt_reg_sync_writel_print(0x00000000, offset + LVTSMONCTL0_0);
	}
}

void lvts_enable_all_sensing_points(void)
{
	int i, offset;

	for (i = 0; i < ARRAY_SIZE(lvts_tscpu_g_tc); i++) {

		offset = lvts_tscpu_g_tc[i].tc_offset;

		lvts_dbg_printk("%s %d:%d\n", __func__, i,
			lvts_tscpu_g_tc[i].ts_number);

		switch (lvts_tscpu_g_tc[i].ts_number) {
		case 1:
			/* enable sensing point 0 */
			mt_reg_sync_writel_print(0x00000201,
					offset + LVTSMONCTL0_0);
			break;
		case 2:
			/* enable sensing point 0,1 */
			mt_reg_sync_writel_print(0x00000203,
					offset + LVTSMONCTL0_0);
			break;
		case 3:
			/* enable sensing point 0,1,2 */
			mt_reg_sync_writel_print(0x00000207,
					offset + LVTSMONCTL0_0);
			break;
		case 4:
			/* enable sensing point 0,1,2,3*/
			mt_reg_sync_writel_print(0x0000020F,
					offset + LVTSMONCTL0_0);
			break;
		default:
			lvts_printk("Error at %s\n", __func__);
			break;
		}
	}
}

void lvts_tscpu_thermal_initial_all_tc(void)
{
	int i = 0, offset;

	for (i = 0; i < ARRAY_SIZE(lvts_tscpu_g_tc); i++) {
		offset = lvts_tscpu_g_tc[i].tc_offset;

		/*  set sensor index of LVTS */
		mt_reg_sync_writel_print(0x13121110, LVTSTSSEL_0 + offset);
		/*  set calculation scale rules */
		mt_reg_sync_writel_print(0x00000300, LVTSCALSCALE_0 + offset);
		/* Set Device Single mode */
		lvts_write_device(0xC1030000, 0x06, 0xB8, i);

		lvts_configure_polling_speed_and_filter(i);
	}

#if DUMP_LVTS_REGISTER_FOR_ZERO_RAW_ISSUE
	read_controller_reg_before_active();
#endif
}

static void lvts_disable_rgu_reset(void)
{

}

static void lvts_enable_rgu_reset(void)
{

}

void lvts_config_all_tc_hw_protect(int temperature, int temperature2)
{
	int i = 0;

	lvts_dbg_printk("%s, temperature=%d,temperature2=%d,\n",
					__func__, temperature, temperature2);

	/*spend 860~1463 us */
	/*Thermal need to config to direct reset mode
	 *this API provide by Weiqi Fu(RGU SW owner).
	 */
	lvts_disable_rgu_reset();

	for (i = 0; i < ARRAY_SIZE(lvts_tscpu_g_tc); i++) {
		if (lvts_tscpu_g_tc[i].ts_number == 0)
			continue;
		/* Move thermal HW protection ahead... */
		lvts_set_tc_trigger_hw_protect(temperature, temperature2, i);
	}

#ifndef LVTS_DYNAMIC_ENABLE_REBOOT
	/* Thermal need to config to direct reset mode
	 * this API provide by Weiqi Fu(RGU SW owner).
	 */
	lvts_enable_rgu_reset();
#else
	hw_protect_setting_done = 1;
#endif
}

void lvts_tscpu_reset_thermal(void)
{
	/* chip dependent, Have to confirm with DE */

	int temp = 0;


	/* reset AP thremal ctrl */
	/* TODO: Is it necessary to read INFRA_GLOBALCON_RST_0_SET? */
	temp = readl(INFRA_GLOBALCON_RST_0_SET);

	/* 1: Enables thermal control software reset */
	temp |= 0x00000001;
	mt_reg_sync_writel_print(temp, INFRA_GLOBALCON_RST_0_SET);

	/* TODO: How long to set the reset bit? */

	/* un reset */
	/* TODO: Is it necessary to read INFRA_GLOBALCON_RST_0_CLR? */
	temp = readl(INFRA_GLOBALCON_RST_0_CLR);

	/* 1: Enable reset Disables thermal control software reset */
	temp |= 0x00000001;

	mt_reg_sync_writel_print(temp, INFRA_GLOBALCON_RST_0_CLR);
}


void get_lvts_slope_intercept(struct TS_PTPOD *ts_info, enum
		thermal_bank_name ts_bank)
{
	struct TS_PTPOD ts_ptpod;
	int temp;

	/* chip dependent */

	temp = (0 - LVTS_COEFF_A_X_1000) * 2;
	temp /= 1000;
	ts_ptpod.ts_MTS = temp;

	temp = 500 * g_golden_temp + LVTS_COEFF_B_X_1000;
	temp /= 1000;
	ts_ptpod.ts_BTS = (temp - 25) * 4;

	ts_info->ts_MTS = ts_ptpod.ts_MTS;
	ts_info->ts_BTS = ts_ptpod.ts_BTS;
	lvts_dbg_printk("(LVTS) ts_MTS=%d, ts_BTS=%d\n",
			ts_ptpod.ts_MTS, ts_ptpod.ts_BTS);
}
EXPORT_SYMBOL(get_lvts_slope_intercept);

int lvts_tscpu_dump_cali_info(struct seq_file *m, void *v)
{
	int i;

	seq_printf(m, "lvts_cal : %d\n", g_use_fake_efuse?0:1);
	seq_printf(m, "[lvts_cal] g_golden_temp %d\n", g_golden_temp);

	for (i = 0; i < L_TS_LVTS_NUM; i++)
		seq_printf(m, "[lvts_cal] g_count_r%d = 0x%x\n",
				i, g_count_r[i]);

	for (i = 0; i < L_TS_LVTS_NUM; i++)
		seq_printf(m, "[lvts_cal] g_count_rc%d = 0x%x\n",
				i, g_count_rc[i]);

	return 0;
}

#ifdef LVTS_DYNAMIC_ENABLE_REBOOT
void lvts_enable_all_hw_protect(void)
{
	int i, offset;

	if (!tscpu_is_temp_valid() || !hw_protect_setting_done
		|| lvts_hw_protect_enabled) {
		lvts_dbg_printk("%s: skip, valid=%d, done=%d, en=%d\n",
			__func__, tscpu_is_temp_valid(),
			hw_protect_setting_done, lvts_hw_protect_enabled);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(lvts_tscpu_g_tc); i++) {
		if (lvts_tscpu_g_tc[i].ts_number == 0)
			continue;

		offset = lvts_tscpu_g_tc[i].tc_offset;
		/* enable trigger Hot SPM interrupt */
		mt_reg_sync_writel_print(
			readl(offset + LVTSMONINT_0) | 0x80000000,
			offset + LVTSMONINT_0);
	}

	lvts_enable_rgu_reset();

	/* clear offset after all HW reset are configured. */
	/* make sure LVTS controller uses latest sensor value to compare */
	for (i = 0; i < ARRAY_SIZE(lvts_tscpu_g_tc); i++) {
		if (lvts_tscpu_g_tc[i].ts_number == 0)
			continue;

		offset = lvts_tscpu_g_tc[i].tc_offset;
		/* clear offset */
		mt_reg_sync_writel_print(
			readl(offset + LVTSPROTCTL_0) & ~0xFFFF,
			offset + LVTSPROTCTL_0);
	}

	lvts_hw_protect_enabled = 1;
}

void lvts_disable_all_hw_protect(void)
{
	int i, offset;

	if (!tscpu_is_temp_valid() || !lvts_hw_protect_enabled) {
		lvts_dbg_printk("%s: skip, valid=%d, en=%d\n", __func__,
			tscpu_is_temp_valid(), lvts_hw_protect_enabled);
		return;
	}

	lvts_disable_rgu_reset();

	for (i = 0; i < ARRAY_SIZE(lvts_tscpu_g_tc); i++) {
		if (lvts_tscpu_g_tc[i].ts_number == 0)
			continue;

		offset = lvts_tscpu_g_tc[i].tc_offset;
		/* disable trigger SPM interrupt */
		mt_reg_sync_writel_print(
			readl(offset + LVTSMONINT_0) & 0x7FFFFFFF,
			offset + LVTSMONINT_0);
		/* set offset to 0x3FFF to avoid interrupt false triggered */
		/* large offset can guarantee temp check is always false */
		mt_reg_sync_writel_print(
			readl(offset + LVTSPROTCTL_0) | 0x3FFF,
			offset + LVTSPROTCTL_0);
	}

	lvts_hw_protect_enabled = 0;
}
#endif

