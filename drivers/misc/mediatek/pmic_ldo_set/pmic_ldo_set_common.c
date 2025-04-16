// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>

enum pmic_ldo_online_ic{
	LDO_UNKNOW,
	FAN53870,
	WL2868C,
	DIO8018,
};

static enum pmic_ldo_online_ic ldo_type = LDO_UNKNOW;
extern int is_fan53870_pmic(void);
extern int wl2868c_test_i2c_enable(void);
extern int is_DIO8018_pmic(void);

extern int fan53870_ldo1_set_voltage(unsigned int set_uV);
extern int fan53870_ldo1_disable(void);
extern int fan53870_cam_ldo_disable(unsigned int LDO_NUM);
extern int fan53870_cam_ldo_set_voltage(unsigned int LDO_NUM, int set_mv);
extern void enable_fan53870_gpio(int pwr_status);

extern int DIO8018_ldo1_set_voltage(unsigned int set_uV);
extern int DIO8018_ldo1_disable(void);
extern int DIO8018_cam_ldo_disable(unsigned int LDO_NUM);
extern int DIO8018_cam_ldo_set_voltage(unsigned int LDO_NUM, int set_mv);
extern void enable_DIO8018_gpio(int pwr_status);

extern int wl2868c_ldo_2_set_voltage(unsigned int set_uV);
extern int wl2868c_ldo_2_set_disable(void);
extern int wl2868c_ldo_set_disable(unsigned int ldo_num);
extern int wl2868c_voltage_output(unsigned int ldo_num, int vol);
extern void enable_wl2868c_gpio(int pwr_status);
#ifdef OPLUS_FEATURE_DISPLAY
extern int wl2868c_get_register_value(unsigned int ldo_num, int *vol);
#endif
struct pmic_ldo_operations{
	int (*ldo_set_voltage_mv)(unsigned int ldo_num, int set_mv);
	int (*ldo_2_set_voltage_uv)(unsigned int set_uv);
	int (*ldo_set_disable)(unsigned int ldo_num);
	int (*ldo_2_set_disable)(void);
	void (*enable_gpio)(int pwr_status);
	#ifdef OPLUS_FEATURE_DISPLAY
	int (*ldo_get_register_value)(unsigned int ldo_num, int *vol);
	#endif
};

static struct pmic_ldo_operations ldo_ops = {};

int pmic_ldo_get_type(void) {
	int ret = 0;
	if(ldo_type != LDO_UNKNOW) {
		return ret;
	}
	ret = is_fan53870_pmic();
	if(ret == 1) {
		ldo_type = FAN53870;
		ldo_ops.ldo_set_voltage_mv = fan53870_cam_ldo_set_voltage;
		ldo_ops.ldo_2_set_voltage_uv = fan53870_ldo1_set_voltage;
		ldo_ops.ldo_set_disable = fan53870_cam_ldo_disable;
		ldo_ops.ldo_2_set_disable = fan53870_ldo1_disable;
		ldo_ops.enable_gpio = enable_fan53870_gpio;
		return 0;
	} else {
		pr_err("%s, fan53870 no ok, fan53870_pmic status %d\n", __func__, ret);
		ret = wl2868c_test_i2c_enable();
		if(ret == 1) {
			ldo_type = WL2868C;
			ldo_ops.ldo_set_voltage_mv = wl2868c_voltage_output;
			ldo_ops.ldo_2_set_voltage_uv = wl2868c_ldo_2_set_voltage;
			ldo_ops.ldo_set_disable = wl2868c_ldo_set_disable;
			ldo_ops.ldo_2_set_disable = wl2868c_ldo_2_set_disable;
			ldo_ops.enable_gpio = enable_wl2868c_gpio;
			#ifdef OPLUS_FEATURE_DISPLAY
			ldo_ops.ldo_get_register_value = wl2868c_get_register_value;
			#endif
			return 0;
		} else {
			pr_err("%s, wl2868 no ok, wl2868 status %d\n", __func__, ret);
			ret = is_DIO8018_pmic();
			if(ret == 1) {
				ldo_type = DIO8018;
				ldo_ops.ldo_set_voltage_mv = DIO8018_cam_ldo_set_voltage;
				ldo_ops.ldo_2_set_voltage_uv = DIO8018_ldo1_set_voltage;
				ldo_ops.ldo_set_disable = DIO8018_cam_ldo_disable;
				ldo_ops.ldo_2_set_disable = DIO8018_ldo1_disable;
				ldo_ops.enable_gpio = enable_DIO8018_gpio;
				return 0;
			} else {
				pr_err("%s, DIO8018 no ok, DIO8018 status %d\n", __func__, ret);
				return -1;
			}
		}
	}
}
EXPORT_SYMBOL(pmic_ldo_get_type);
void pmic_gpio_enable(int pwr_status)
{
	if (!pmic_ldo_get_type()) {
		ldo_ops.enable_gpio(pwr_status);
	}
}
EXPORT_SYMBOL(pmic_gpio_enable);

int pmic_ldo_set_voltage_mv(unsigned int ldo_num, int set_mv)
{
	if(ldo_type != LDO_UNKNOW) {
		return ldo_ops.ldo_set_voltage_mv(ldo_num, set_mv);
	} else {
		if(!pmic_ldo_get_type())
			return ldo_ops.ldo_set_voltage_mv(ldo_num, set_mv);
	}
	return -1;
}
EXPORT_SYMBOL(pmic_ldo_set_voltage_mv);

#ifdef OPLUS_FEATURE_DISPLAY
int pmic_ldo_get_register(unsigned int ldo_num, int *get_mv)
{
	int ldo_reg_val = 0;
	if (NULL == get_mv) {
		pr_err("%s, param get_mv is null\n", __func__);
		return -1;
	}

	if(ldo_type != LDO_UNKNOW) {
		if (ldo_ops.ldo_get_register_value) {
			ldo_ops.ldo_get_register_value(ldo_num, &ldo_reg_val);
			*get_mv = ldo_reg_val;
			pr_debug("%s, wl2868 ldo val = %d\n", __func__, *get_mv);
			return 0;
		}
	} else {
		if(!pmic_ldo_get_type()) {
			if (ldo_ops.ldo_get_register_value) {
				ldo_ops.ldo_get_register_value(ldo_num, &ldo_reg_val);
				*get_mv = ldo_reg_val;
				pr_debug("%s, wl2868 ldo val = %d\n", __func__, *get_mv);
				return 0;
			}
		}
	}
	return -1;
}
EXPORT_SYMBOL(pmic_ldo_get_register);
#endif

int pmic_ldo_set_disable(unsigned int ldo_num)
{
	if(ldo_type != LDO_UNKNOW) {
		return ldo_ops.ldo_set_disable(ldo_num);
	} else {
		if(!pmic_ldo_get_type())
			return ldo_ops.ldo_set_disable(ldo_num);
	}
	return -1;
}
EXPORT_SYMBOL(pmic_ldo_set_disable);


/*
* set ldo 2 voltage.
* return : reg write error code;
*/
int pmic_ldo_2_set_voltage_uv(unsigned int set_uV)
{
	if(ldo_type != LDO_UNKNOW) {
		return ldo_ops.ldo_2_set_voltage_uv(set_uV);
	} else {
		if(!pmic_ldo_get_type())
			return ldo_ops.ldo_2_set_voltage_uv(set_uV);
	}
	return -1;
}
EXPORT_SYMBOL(pmic_ldo_2_set_voltage_uv);

/*
* set pmic_ldo IC disable,shutdown ldo2 output.
*/
int pmic_ldo_2_set_disable(void)
{
	if(ldo_type != LDO_UNKNOW) {
		return ldo_ops.ldo_2_set_disable();
	} else {
		if(!pmic_ldo_get_type())
			return ldo_ops.ldo_2_set_disable();
	}
	return -1;
}
EXPORT_SYMBOL(pmic_ldo_2_set_disable);
MODULE_LICENSE("GPL");
