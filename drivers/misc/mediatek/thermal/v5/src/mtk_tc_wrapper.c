// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/seq_file.h>

#if IS_ENABLED(CONFIG_OF)
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#if IS_ENABLED(CONFIG_MTK_CLKMGR)
#include <mach/mtk_clkmgr.h>
#else
#include <linux/clk.h>
#endif

#include <tscpu_settings.h>
#include "mtk_idle.h"

#if CFG_THERMAL_KERNEL_IGNORE_HOT_SENSOR
#include <mt-plat/mtk_devinfo.h>
#endif

#if CONFIG_LVTS_ERROR_AEE_WARNING
#include <mt-plat/aee.h>
#include <linux/delay.h>
#if DUMP_VCORE_VOLTAGE
#include <linux/regulator/consumer.h>
#endif
struct lvts_error_data {
	int ts_temp[TS_ENUM_MAX][R_BUFFER_SIZE]; /* A ring buffer */
	int ts_temp_r[TS_ENUM_MAX][R_BUFFER_SIZE]; /* A ring buffer */
	int ts_temp_v[TS_ENUM_MAX][R_BUFFER_SIZE]; /* A ring buffer */
#if DUMP_VCORE_VOLTAGE
	int vcore_voltage[R_BUFFER_SIZE]; /* A ring buffer */
#endif
	int c_index; /* Current index points to the space to replace.*/
	int e_occurred; /* 1: An error occurred, 0: Nothing happened*/
	int f_count; /* Future count */
	enum thermal_sensor e_mcu;
	enum thermal_sensor e_lvts;
};
struct lvts_error_data g_lvts_e_data;
int tscpu_ts_mcu_temp_v[L_TS_MCU_NUM];
int tscpu_ts_lvts_temp_v[L_TS_LVTS_NUM];
#endif


#if CFG_THERM_LVTS
int tscpu_ts_lvts_temp[L_TS_LVTS_NUM];
int tscpu_ts_lvts_temp_r[L_TS_LVTS_NUM];
#endif

int tscpu_init_done;
int tscpu_curr_cpu_temp;
int tscpu_curr_gpu_temp;
#if CFG_THERMAL_KERNEL_IGNORE_HOT_SENSOR
static int ignore_hot_sensor;
#endif

static int tscpu_curr_max_ts_temp;

int mtk_idle_notifier_register(struct notifier_block *n)
{
	return 0;
}

void set_tscpu_init_done(int complete)
{
	tscpu_init_done = complete;
}

int get_immediate_none_wrap(void)
{
	return -127000;
}

/* chip dependent */
int get_immediate_cpuL_wrap(void)
{
	int curr_temp;

	curr_temp = MAX(
		MAX(tscpu_ts_lvts_temp[L_TS_LVTS1_0],
			tscpu_ts_lvts_temp[L_TS_LVTS1_1]),
		MAX(tscpu_ts_lvts_temp[L_TS_LVTS1_2],
			tscpu_ts_lvts_temp[L_TS_LVTS1_3])
		);

	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_cpuB_LVTS1_wrap(void)
{
	int curr_temp;

	curr_temp = MAX(tscpu_ts_lvts_temp[L_TS_LVTS2_0],
			tscpu_ts_lvts_temp[L_TS_LVTS2_1]);

	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}


int get_immediate_gpu_wrap(void)
{
	int curr_temp;

	curr_temp = MAX(tscpu_ts_lvts_temp[L_TS_LVTS3_0],
			tscpu_ts_lvts_temp[L_TS_LVTS3_1]);


	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;

}

int get_immediate_infa_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS3_2];

	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_connsys_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS3_3];

	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_md_wrap(void)
{
	int curr_temp;

	curr_temp = MAX(MAX(tscpu_ts_lvts_temp[L_TS_LVTS4_0],
		tscpu_ts_lvts_temp[L_TS_LVTS4_1]),
		tscpu_ts_lvts_temp[L_TS_LVTS4_2]);

	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;

}


/*
 * module			LVTS Plan
 *=====================================================
 * MCU_LITTLE		LVTS1-0, LVTS1-1, LVTS1-2, LVTS1-3
 * BIG		LVTS2-0, LVTS2-1
 * GPU		LVTS3-0, LVTS3-1
 * SOC		LVTS3-2
 * CONSYS		LVTS3-3
 * MD-4G		LVTS4-0
 * MD-5G		LVTS4-1
 * MD-3G		LVTS4-2
 */


/*
 * THERMAL_BANK0,	//L CPU (LVTS1)
 * THERMAL_BANK1,	//B CPU (LVTS2)
 * THERMAL_BANK2,	//GPU (LVTS3)
 * THERMAL_BANK3,	//MD   (LVTS4)
 */

int (*max_temperature_in_bank[THERMAL_BANK_NUM])(void) = {
	get_immediate_cpuB_LVTS1_wrap,
	get_immediate_cpuL_wrap,
	get_immediate_gpu_wrap,
	get_immediate_md_wrap
};

int get_immediate_cpuB_wrap(void)
{
	int curr_temp;

	curr_temp = get_immediate_cpuB_LVTS1_wrap();


	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}




int get_immediate_ts0_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS1_0];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_ts1_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS1_1];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_ts2_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS1_2];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_ts3_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS1_3];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_ts4_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS2_0];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_ts5_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS2_1];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_ts6_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS3_0];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_ts7_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS3_1];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_ts8_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS3_2];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_ts9_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS3_3];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

#if CFG_THERM_LVTS
int get_immediate_tslvts1_0_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[TS_LVTS1_0];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_tslvts1_1_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[TS_LVTS1_1];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}
int get_immediate_tslvts1_2_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[TS_LVTS1_2];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_tslvts1_3_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[TS_LVTS1_3];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_tslvts2_0_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[TS_LVTS2_0];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_tslvts2_1_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[TS_LVTS2_1];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_tslvts3_0_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[TS_LVTS3_0];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_tslvts3_1_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[TS_LVTS3_1];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_tslvts3_2_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[TS_LVTS3_2];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_tslvts3_3_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[TS_LVTS3_3];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_tslvts4_0_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[TS_LVTS4_0];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_tslvts4_1_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[TS_LVTS4_1];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_tslvts4_2_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[TS_LVTS4_2];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}



#endif

int get_immediate_tsabb_wrap(void)
{

	return 0;
}

int (*get_immediate_tsX[TS_ENUM_MAX])(void) = {

#if CFG_THERM_LVTS
	get_immediate_tslvts1_0_wrap,
	get_immediate_tslvts1_1_wrap,
	get_immediate_tslvts1_2_wrap,
	get_immediate_tslvts1_3_wrap,
	get_immediate_tslvts2_0_wrap,
	get_immediate_tslvts2_1_wrap,
	get_immediate_tslvts3_0_wrap,
	get_immediate_tslvts3_1_wrap,
	get_immediate_tslvts3_2_wrap,
	get_immediate_tslvts3_3_wrap,
	get_immediate_tslvts4_0_wrap,
	get_immediate_tslvts4_1_wrap,
	get_immediate_tslvts4_2_wrap
#endif
};

/**
 * this only returns latest stored max ts temp but not updated from TC.
 */
int tscpu_get_curr_max_ts_temp(void)
{
	return tscpu_curr_max_ts_temp;
}

#if CFG_THERM_LVTS
int tscpu_max_temperature(void)
{
	int i, j, max = 0;

#if CFG_LVTS_DOMINATOR
#if CFG_THERM_LVTS
	tscpu_dprintk("lvts_max_temperature %s, %d\n", __func__, __LINE__);

	for (i = 0; i < ARRAY_SIZE(lvts_tscpu_g_tc); i++) {
		for (j = 0; j < lvts_tscpu_g_tc[i].ts_number; j++) {
			if (i == 0 && j == 0) {
				max = tscpu_ts_lvts_temp[
					lvts_tscpu_g_tc[i].ts[j]];
			} else {
				if (max < tscpu_ts_lvts_temp[
						lvts_tscpu_g_tc[i].ts[j]])
					max = tscpu_ts_lvts_temp[
						lvts_tscpu_g_tc[i].ts[j]];
			}
		}
	}
#endif /* CFG_THERM_LVTS */
#else
	tscpu_dprintk("tscpu_get_temp %s, %d\n", __func__, __LINE__);

	for (i = 0; i < ARRAY_SIZE(tscpu_g_tc); i++) {
		for (j = 0; j < tscpu_g_tc[i].ts_number; j++) {
			if (i == 0 && j == 0) {
				max = tscpu_ts_mcu_temp[tscpu_g_tc[i].ts[j]];
			} else {
				if (max < tscpu_ts_mcu_temp[
						tscpu_g_tc[i].ts[j]])
					max = tscpu_ts_mcu_temp[
						tscpu_g_tc[i].ts[j]];
			}
		}
	}
#endif /* CFG_LVTS_DOMINATOR */
	return max;
}
#endif

int tscpu_get_curr_temp(void)
{
	if (!tscpu_init_done)
		return 0;

	tscpu_update_tempinfo();

#if PRECISE_HYBRID_POWER_BUDGET
	/* update CPU/GPU temp data whenever TZ times out...
	 * If the update timing is aligned to TZ polling,
	 * this segment should be moved to TZ code instead of thermal
	 * controller driver
	 */
	/*
	 * module			LVTS Plan
	 *=====================================================
	 * MCU_LITTLE		LVTS1-0, LVTS1-1, LVTS1-2, LVTS1-3
	 * BIG		LVTS2-0, LVTS2-1
	 * GPU		LVTS3-0, LVTS3-1
	 * SOC		LVTS3-2
	 * CONSYS		LVTS3-3
	 * MD-4G		LVTS4-0
	 * MD-5G		LVTS4-1
	 * MD-3G		LVTS4-2
	 */



#if CFG_LVTS_DOMINATOR
#if CFG_THERM_LVTS
	tscpu_curr_cpu_temp =
	MAX(
		(MAX(
		MAX(tscpu_ts_lvts_temp[TS_LVTS1_0],
			tscpu_ts_lvts_temp[TS_LVTS1_1]),
		MAX(tscpu_ts_lvts_temp[TS_LVTS1_2],
			tscpu_ts_lvts_temp[TS_LVTS1_3])
		)),
		(MAX(
			tscpu_ts_lvts_temp[TS_LVTS2_0],
			tscpu_ts_lvts_temp[TS_LVTS2_1])
		)
	);

	tscpu_curr_gpu_temp = MAX(tscpu_ts_lvts_temp[TS_LVTS3_0],
								tscpu_ts_lvts_temp[TS_LVTS3_1]);
#endif /* CFG_THERM_LVTS */
#else
	/* It is platform dependent which TS is better to present CPU/GPU
	 * temperature
	 */
	tscpu_curr_cpu_temp = MAX(
		MAX(MAX(tscpu_ts_temp[TS_MCU5], tscpu_ts_temp[TS_MCU6]),
		MAX(tscpu_ts_temp[TS_MCU7], tscpu_ts_temp[TS_MCU8])),
		tscpu_ts_temp[TS_MCU9]);

	tscpu_curr_gpu_temp = MAX(tscpu_ts_temp[TS_MCU1],
		tscpu_ts_temp[TS_MCU2]);
#endif /* CFG_LVTS_DOMINATOR */
#endif /* PRECISE_HYBRID_POWER_BUDGET */

	/* though tscpu_max_temperature is common, put it in mtk_ts_cpu.c is
	 * weird.
	 */

	tscpu_curr_max_ts_temp = tscpu_max_temperature();

	return tscpu_curr_max_ts_temp;
}

#if CONFIG_LVTS_ERROR_AEE_WARNING
char mcu_s_array[TS_ENUM_MAX][17] = {
#if CFG_THERM_LVTS
	"TS_LVTS1_0",
	"TS_LVTS1_1",
	"TS_LVTS1_2",
	"TS_LVTS1_3",
	"TS_LVTS2_0",
	"TS_LVTS2_1",
	"TS_LVTS3_0",
	"TS_LVTS3_1",
	"TS_LVTS3_2",
	"TS_LVTS3_3",
	"TS_LVTS4_0",
	"TS_LVTS4_1",
	"TS_LVTS4_2"
 #endif
};

static void dump_lvts_error_info(void)
{
	int i, j, index, e_index, offset;
#if DUMP_LVTS_REGISTER
	int cnt, temp;
#endif
	enum thermal_sensor mcu_index, lvts_index;
	char buffer[512];

	mcu_index = g_lvts_e_data.e_mcu;
	lvts_index = g_lvts_e_data.e_lvts;
	index = g_lvts_e_data.c_index;
	e_index = (index + HISTORY_SAMPLES + 1) % R_BUFFER_SIZE;

	tscpu_dprintk("[LVTS_ERROR][DUMP] %s:%d and %s:%d error: |%d| > %d\n",
		mcu_s_array[mcu_index],
		g_lvts_e_data.ts_temp[mcu_index][e_index],
		mcu_s_array[lvts_index],
		g_lvts_e_data.ts_temp[lvts_index][e_index],
		g_lvts_e_data.ts_temp[mcu_index][e_index] -
		g_lvts_e_data.ts_temp[lvts_index][e_index],
		LVTS_ERROR_THRESHOLD);

	for (i = TS_MCU1; i <= TS_LVTS4_1; i++) {
		offset = sprintf(buffer, "[LVTS_ERROR][%s][DUMP] ",
				mcu_s_array[i]);

		for (j = 0; j < R_BUFFER_SIZE; j++) {
			index = (g_lvts_e_data.c_index + 1 + j)
					% R_BUFFER_SIZE;
			offset += sprintf(buffer + offset, "%d ",
					g_lvts_e_data.ts_temp[i][index]);

		}
		buffer[offset] = '\0';
		tscpu_dprintk("%s\n", buffer);
	}

	offset = sprintf(buffer, "[LVTS_ERROR][%s_R][DUMP] ",
			mcu_s_array[lvts_index]);

	for (j = 0; j < R_BUFFER_SIZE; j++) {
		index = (g_lvts_e_data.c_index + 1 + j) % R_BUFFER_SIZE;
		offset += sprintf(buffer + offset, "%d ",
			g_lvts_e_data.ts_temp_r[lvts_index][index]);
	}

	buffer[offset] = '\0';
	tscpu_dprintk("%s\n", buffer);

	offset = sprintf(buffer, "[LVTS_ERROR][%s_V][DUMP] ",
			mcu_s_array[mcu_index]);

	for (j = 0; j < R_BUFFER_SIZE; j++) {
		index = (g_lvts_e_data.c_index + 1 + j) % R_BUFFER_SIZE;
		offset += sprintf(buffer + offset, "%d ",
			g_lvts_e_data.ts_temp_v[mcu_index][index]);
	}
	buffer[offset] = '\0';
	tscpu_dprintk("%s\n", buffer);

	offset = sprintf(buffer, "[LVTS_ERROR][%s_V][DUMP] ",
			mcu_s_array[lvts_index]);

	for (j = 0; j < R_BUFFER_SIZE; j++) {
		index = (g_lvts_e_data.c_index + 1 + j) % R_BUFFER_SIZE;
		offset += sprintf(buffer + offset, "%d ",
			g_lvts_e_data.ts_temp_v[lvts_index][index]);
	}

	buffer[offset] = '\0';
	tscpu_dprintk("%s\n", buffer);

	dump_efuse_data();
#if DUMP_LVTS_REGISTER
	read_controller_reg_when_error();

	lvts_thermal_disable_all_periodoc_temp_sensing();
	lvts_wait_for_all_sensing_point_idle();

	read_device_reg_when_error();
	dump_lvts_register_value();
#endif
#if DUMP_VCORE_VOLTAGE
	offset = sprintf(buffer, "[LVTS_ERROR][Vcore_V][DUMP] ",
			mcu_s_array[lvts_index]);
	for (j = 0; j < R_BUFFER_SIZE; j++) {
		index = (g_lvts_e_data.c_index + 1 + j) % R_BUFFER_SIZE;
		offset += sprintf(buffer + offset, "%d ",
			g_lvts_e_data.vcore_voltage[index]);
	}
	buffer[offset] = '\0';
	tscpu_dprintk("%s\n", buffer);
#endif
#if DUMP_LVTS_REGISTER
	lvts_reset_device_and_stop_clk();
#endif
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_DEFAULT, __func__,
		"LVTS_ERROR: %s, %s diff: %d\n", mcu_s_array[mcu_index],
		mcu_s_array[lvts_index],
		g_lvts_e_data.ts_temp[mcu_index][e_index] -
		g_lvts_e_data.ts_temp[lvts_index][e_index]);
#endif

	g_lvts_e_data.e_occurred = 0;
	g_lvts_e_data.f_count = -1;
#if DUMP_LVTS_REGISTER
	clear_lvts_register_value_array();

	lvts_device_identification();
	lvts_Device_Enable_Init_all_Devices();
	lvts_device_read_count_RC_N();
	lvts_efuse_setting();

	lvts_thermal_disable_all_periodoc_temp_sensing();
	Enable_LVTS_CTRL_for_thermal_Data_Fetch();
	lvts_tscpu_thermal_initial_all_tc();
#endif
}

static void check_lvts_error(enum thermal_sensor mcu_index,
	enum thermal_sensor lvts_index)
{
	int temp;

	temp = tscpu_ts_temp[mcu_index] - tscpu_ts_temp[lvts_index];

	if (temp < 0)
		temp = temp * -1;

	/*Skip if LVTS thermal controllers doens't ready */
	if (temp > 100000)
		return;

	if (temp > LVTS_ERROR_THRESHOLD) {
		tscpu_dprintk("[LVTS_ERROR] %s:%d and %s:%d error: |%d| > %d\n",
			mcu_s_array[mcu_index],
			tscpu_ts_temp[mcu_index],
			mcu_s_array[lvts_index],
			tscpu_ts_temp[lvts_index],
			tscpu_ts_temp[mcu_index] -
			tscpu_ts_temp[lvts_index],
			LVTS_ERROR_THRESHOLD);
		g_lvts_e_data.e_occurred = 1;
		g_lvts_e_data.e_mcu = mcu_index;
		g_lvts_e_data.e_lvts = lvts_index;
		g_lvts_e_data.f_count = -1;
	}
}
void dump_lvts_error_data_info(void)
{
	char buffer[512];
	int offset, j;

	tscpu_dprintk("[LVTS_ERROR] c_index %d, e_occurred %d, f_count %d\n",
			g_lvts_e_data.c_index, g_lvts_e_data.e_occurred,
			g_lvts_e_data.f_count);
	tscpu_dprintk("[LVTS_ERROR] e_mcu %d, e_lvts %d\n", g_lvts_e_data.e_mcu,
			g_lvts_e_data.e_lvts);

	offset = sprintf(buffer, "[LVTS_ERROR][%s][DUMP] ",
			mcu_s_array[TS_LVTS1_0]);
	for (j = 0; j < R_BUFFER_SIZE; j++) {
		offset += sprintf(buffer + offset, "%d ",
				g_lvts_e_data.ts_temp[TS_LVTS1_0][j]);

	}
	buffer[offset] = '\0';
	tscpu_dprintk("%s\n", buffer);

	offset = sprintf(buffer, "[LVTS_ERROR][%s_raw][DUMP] ",
			mcu_s_array[TS_LVTS1_0]);
	for (j = 0; j < R_BUFFER_SIZE; j++) {
		offset += sprintf(buffer + offset, "%d ",
				g_lvts_e_data.ts_temp_r[TS_LVTS1_0][j]);

	}
	buffer[offset] = '\0';
	tscpu_dprintk("%s\n", buffer);
}
#endif

int tscpu_read_temperature_info(struct seq_file *m, void *v)
{
	seq_printf(m, "curr_temp = %d\n", tscpu_get_curr_max_ts_temp());

#if !defined(CFG_THERM_NO_AUXADC)
	tscpu_dump_cali_info(m, v);
#endif

#if CFG_THERM_LVTS
	seq_puts(m, "-----------------\n");
	lvts_tscpu_dump_cali_info(m, v);
#endif
	return 0;
}

static int thermal_idle_notify_call(struct notifier_block *nfb,
				unsigned long id,
				void *arg)
{
	switch (id) {
	case NOTIFY_DPIDLE_ENTER:
		break;
	case NOTIFY_SOIDLE_ENTER:
		break;
	case NOTIFY_DPIDLE_LEAVE:
		break;
	case NOTIFY_SOIDLE_LEAVE:
		break;
	case NOTIFY_SOIDLE3_ENTER:
#if LVTS_VALID_DATA_TIME_PROFILING
		SODI3_count++;
		if (SODI3_count != 1 && isTempValid == 0)
			noValid_count++;

		if (isTempValid == 1 || SODI3_count == 1)
			start_timestamp = thermal_get_current_time_us();

		isTempValid = 0;
#endif
		break;
	case NOTIFY_SOIDLE3_LEAVE:
#if CFG_THERM_LVTS
		lvts_sodi3_release_thermal_controller();
#endif
		break;
	default:
		/* do nothing */
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block thermal_idle_nfb = {
	.notifier_call = thermal_idle_notify_call,
};

#if IS_ENABLED(CONFIG_OF)
int get_io_reg_base(void)
{
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt6833-lvts");
	WARN_ON_ONCE(node == 0);
	if (node)
		/* Setup IO addresses */
		thermal_base = of_iomap(node, 0);

	/*get thermal irq num */
	thermal_irq_number = irq_of_parse_and_map(node, 0);
	tscpu_dprintk("[THERM_CTRL] thermal_irq_number=%d\n",
				thermal_irq_number);
	if (!thermal_irq_number) {
		/*TODO: need check "irq number"*/
		tscpu_dprintk("[THERM_CTRL] get irqnr 0 failed=%d\n",
				thermal_irq_number);
		return 0;
	}

	if (of_property_read_u32_index(node, "reg", 1, &thermal_phy_base)) {
		tscpu_dprintk("[THERM_CTRL] config error thermal_phy_base\n");
		return 0;
	}

	tscpu_dprintk("[THERM_CTRL] phy_base=0x%x\n", thermal_phy_base);



	node = of_find_compatible_node(NULL, NULL, "mediatek,mt6765-auxadc");
	WARN_ON_ONCE(node == 0);
	if (node) {
		/* Setup IO addresses */
		auxadc_ts_base = of_iomap(node, 0);
	}

	if (of_property_read_u32_index(node, "reg", 1, &auxadc_ts_phy_base)) {
		tscpu_dprintk("[THERM_CTRL] config error auxadc_ts_phy_base\n");
		return 0;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt6833-infracfg_ao");
	WARN_ON_ONCE(node == 0);
	if (node) {
		/* Setup IO addresses */
		infracfg_ao_base = of_iomap(node, 0);

		if (infracfg_ao_base)
			mtk_idle_notifier_register(&thermal_idle_nfb);
	}

	node = of_find_compatible_node(NULL, NULL,
					"mediatek,mt6833-apmixedsys");
	WARN_ON_ONCE(node == 0);
	if (node) {
		/* Setup IO addresses */
		th_apmixed_base = of_iomap(node, 0);
	}

	if (of_property_read_u32_index(node, "reg", 1, &apmixed_phy_base)) {
		tscpu_dprintk("[THERM_CTRL] config error apmixed_phy_base=\n");
		return 0;
	}

#if THERMAL_GET_AHB_BUS_CLOCK
	/* TODO: If this is required, it needs to confirm which node to read. */
	node = of_find_compatible_node(NULL, NULL, "mediatek,infrasys");
	if (!node) {
		pr_notice("[CLK_INFRACFG_AO] find node failed\n");
		return 0;
	}
	therm_clk_infracfg_ao_base = of_iomap(node, 0);
	if (!therm_clk_infracfg_ao_base) {
		pr_notice("[CLK_INFRACFG_AO] base failed\n");
		return 0;
	}
#endif
	return 1;
}
#endif

/* chip dependent */
int tscpu_thermal_clock_on(void)
{
	int ret = -1;

	/* Use CCF */
	ret = clk_prepare_enable(therm_main);
	if (ret)
		tscpu_dprintk("Cannot enable thermal clock.\n");

	return ret;
}

/* chip dependent */
int tscpu_thermal_clock_off(void)
{
	int ret = -1;

	/* Use CCF */
	clk_disable_unprepare(therm_main);
	return ret;
}

#if CFG_THERMAL_KERNEL_IGNORE_HOT_SENSOR
#define CPU_SEGMENT 7
int tscpu_check_cpu_segment(void)
{
	int val = (get_devinfo_with_index(CPU_SEGMENT) & 0xFF);

	if (val == 0x30)
		ignore_hot_sensor = 1;
	else
		ignore_hot_sensor = 0;

	return val;
}
#endif

