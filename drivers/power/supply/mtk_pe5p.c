// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/alarmtimer.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include "mtk_charger_algorithm_class.h"
#include "mtk_pe5p.h"

static int log_level = PE5P_DBG_LEVEL;
module_param(log_level, int, 0644);

static bool algo_waiver_test;
module_param(algo_waiver_test, bool, 0644);

int pe5p_get_log_level(void)
{
	return log_level;
}

#define MS_TO_NS(msec)		((msec) * (NSEC_PER_MSEC))

/* Parameters */
#define PE5P_VTA_INIT		5000	/* mV */
#define PE5P_ITA_INIT		3000	/* mA */
#define PE5P_TA_WDT_MIN		10000	/* ms */
#define PE5P_VTA_GAP_MIN	200	/* mV */
#define PE5P_VTA_VAR_MIN	103	/* % */
#define PE5P_ITA_TRACKING_GAP	150	/* mA */
#define PE5P_DVCHG_VBUSALM_GAP	100	/* mV */
#define PE5P_DVCHG_STARTUP_CONVERT_RATIO	210	/* % */
#define PE5P_DVCHG_CHARGING_CONVERT_RATIO	202	/* % */
#define PE5P_VBUSOVP_RATIO	110
#define PE5P_IBUSOCP_RATIO	110
#define PE5P_VBATOVP_RATIO	110
#define PE5P_IBATOCP_RATIO	110
#define PE5P_ITAOCP_RATIO	110
#define PE5P_IBUSUCPF_RECHECK		250	/* mA */
#define PE5P_VBUS_CALI_THRESHOLD	150	/* mV */
#define PE5P_CV_LOWER_BOUND_GAP		50	/* mV */
#define PE5P_INIT_POLLING_INTERVAL	500	/* ms */
#define PE5P_INIT_RETRY_MAX	3
#define PE5P_MEASURE_R_RETRY_MAX	3
#define PE5P_MEASURE_R_AVG_TIMES	10
#define PE5P_VSYS_UPPER_BOUND            8900    /* mV */
#define PE5P_VSYS_UPPER_BOUND_GAP        40      /* mV */


#define PE5P_HWERR_NOTIFY \
	(BIT(EVT_VBUSOVP) | BIT(EVT_IBUSOCP) | BIT(EVT_VBATOVP) | \
	 BIT(EVT_IBATOCP) | BIT(EVT_VOUTOVP) | BIT(EVT_VDROVP) | \
	 BIT(EVT_IBUSUCP_FALL))

#define PE5P_RESET_NOTIFY \
	(BIT(EVT_DETACH) | BIT(EVT_HARDRESET))

static const char *const pe5p_dvchg_role_name[PE5P_DVCHG_MAX] = {
	"master", "slave", "hv_master", "hv_slave", "buck_boost"
};

static const char *const pe5p_algo_state_name[PE5P_ALGO_STATE_MAX] = {
	"INIT", "MEASURE_R", "SS_SWCHG", "SS_DVCHG", "CC_CV", "STOP",
};

/* If there's no property in dts, these values will be applied */
static const struct pe5p_algo_desc algo_desc_defval = {
	.polling_interval = 500,
	.ta_cv_ss_repeat_tmin = 25,
	.vbat_cv = 8900,
	.start_soc_min = 5,
	.start_soc_max = 80,
	.start_vbat_min = 7100,
	.start_vbat_max = 8250,
	.idvchg_term = 250,
	.idvchg_step = 50,
	.ita_level = {5000, 2700, 2400, 2000},
	.rcable_level = {250, 278, 313, 375},
	.ita_level_dual = {5000, 3700, 3400, 3000},
	.rcable_level_dual = {188, 203, 221, 250},
	.idvchg_ss_init = 500,
	.idvchg_ss_step = 250,
	.idvchg_ss_step1 = 100,
	.idvchg_ss_step2 = 50,
	.idvchg_ss_step1_vbat = 8000,
	.idvchg_ss_step2_vbat = 8100,
	.ta_blanking = 500,
	.swchg_aicr = 0,
	.swchg_ichg = 0,
	.swchg_aicr_ss_init = 400,
	.swchg_aicr_ss_step = 200,
	.swchg_off_vbat = 8200,
	.force_ta_cv_vbat = 8200,
	.chg_time_max = 5400,
	.tta_level_def = {0, 0, 0, 0, 25, 40, 50, 60, 70},
	.tta_curlmt = {0, 0, 0, 0, 0, 300, 600, 900, -1},
	.tta_recovery_area = 3,
	.tbat_level_def = {0, 0, 0, 5, 25, 40, 50, 55, 60},
	.tbat_curlmt = {-1, -1, -1, 300, 0, 300, 600, 900, -1},
	.tbat_recovery_area = 3,
	.tdvchg_level_def = {0, 0, 0, 5, 25, 55, 60, 65, 70},
	.tdvchg_curlmt = {-1, -1, -1, 300, 0, 300, 600, 900, -1},
	.tdvchg_recovery_area = 3,
	.tswchg_level_def = {0, 0, 0, 5, 25, 55, 60, 65, 70},
	.tswchg_curlmt = {-1, -1, -1, 200, 0, 200, 300, 400, -1},
	.tswchg_recovery_area = 3,
	.ifod_threshold = 200,
	.rsw_min = 20,
	.ircmp_rbat = 40,
	.ircmp_vclamp = 0,
	.vta_cap_min = 14000,
	.vta_cap_max = 20000,
	.ita_cap_min = 1000,
	.allow_not_check_ta_status = true,
};

/*
 * @reset_ta: set output voltage/current of TA to 5V/3A and disable
 *            direct charge
 * @hardreset: send hardreset to port partner
 * Note: hardreset's priority is higher than reset_ta
 */
struct pe5p_stop_info {
	bool hardreset_ta;
	bool reset_ta;
};

static inline enum chg_idx to_chgidx(enum pe5p_dvchg_role role)
{
	if (role == PE5P_BUCK_BSTCHG)
		return CHG1;
	else if (role == PE5P_DVCHG_MASTER)
		return DVCHG1;
	else if (role == PE5P_DVCHG_SLAVE)
		return DVCHG2;
	else if (role == PE5P_HVDVCHG_MASTER)
		return HVDVCHG1;
	else if (role == PE5P_HVDVCHG_SLAVE)
		return HVDVCHG2;
	return CHG_MAX;
}

/* Check if there is error notification coming from H/W */
static bool pe5p_is_hwerr_notified(struct pe5p_algo_info *info)
{
	struct pe5p_algo_data *data = info->data;
	bool err = false;
	u32 hwerr = PE5P_HWERR_NOTIFY;

	mutex_lock(&data->notify_lock);
	if (data->ignore_ibusucpf)
		hwerr &= ~BIT(EVT_IBUSUCP_FALL);
	err = !!(data->notify & hwerr);
	if (err)
		PE5P_ERR("H/W error(0x%08X)", hwerr);
	mutex_unlock(&data->notify_lock);
	return err;
}

/*
 * Get ADC value from divider charger
 * Note: ibus will sum up value from all enabled chargers
 * (master dvchg, slave dvchg and swchg)
 */
static int pe5p_stop(struct pe5p_algo_info *info, struct pe5p_stop_info *sinfo);
static int pe5p_get_adc(struct pe5p_algo_info *info, enum pe5p_adc_channel chan,
			int *val)
{
	struct pe5p_algo_data *data = info->data;
	int ret, ibus;
	struct pe5p_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	if (atomic_read(&data->stop_algo)) {
		PE5P_INFO("stop algo\n");
		goto stop;
	}
	*val = 0;
	if (chan == PE5P_ADCCHAN_IBUS) {
		if (data->is_dvchg_en[PE5P_BUCK_BSTCHG]) {
			ret = pe5p_hal_get_adc(info->alg,
					       to_chgidx(PE5P_BUCK_BSTCHG),
					       PE5P_ADCCHAN_IBUS, &ibus);
			if (ret < 0) {
				PE5P_ERR("get buck bst ibus fail(%d)\n", ret);
				return ret;
			}
			*val += ibus;
		}
		if (data->is_swchg_en) {
			ret = pe5p_hal_get_adc(info->alg, CHG1,
					       PE5P_ADCCHAN_IBUS, &ibus);
			if (ret < 0) {
				PE5P_ERR("get swchg ibus fail(%d)\n", ret);
				return ret;
			}
			*val += ibus;
		}
		return 0;
	} else if (chan == PE5P_ADCCHAN_IBAT) {
		if (data->is_dvchg_en[PE5P_BUCK_BSTCHG]) {
			ret = pe5p_hal_get_adc(info->alg,
					       to_chgidx(PE5P_BUCK_BSTCHG),
					       PE5P_ADCCHAN_IBUS, &ibus);
			if (ret < 0) {
				PE5P_ERR("get buck bst ibus fail(%d)\n", ret);
				return ret;
			}
			*val += ibus * 2;
		}
		return 0;
	}
	return pe5p_hal_get_adc(info->alg, CHG1, chan, val);
stop:
	pe5p_stop(info, &sinfo);
	return -EIO;
}

/*
 * Calculate VBUS for divider charger
 * If divider charger is charging, the VBUS only needs to be 2 times of VOUT.
 */
static inline u32 pe5p_vout2vbus(struct pe5p_algo_info *info, u32 vout)
{
	struct pe5p_algo_data *data = info->data;
	u32 ratio = data->is_dvchg_en[PE5P_HVDVCHG_MASTER] ?
		PE5P_DVCHG_CHARGING_CONVERT_RATIO :
		PE5P_DVCHG_STARTUP_CONVERT_RATIO;

	return percent(vout, ratio);
}

/*
 * Maximum PE5P_VTA_VAR_MIN(%) variation (from PD's sepcification)
 * Keep vta_setting PE5P_VTA_VAR_MIN(%) higher than vta_measure
 * and make sure it has minimum gap, PE5P_VTA_GAP_MIN
 */
static inline u32 pe5p_vta_add_gap(struct pe5p_algo_info *info, u32 vta)
{
	return max(percent(vta, PE5P_VTA_VAR_MIN), vta + PE5P_VTA_GAP_MIN);
}

/*
 * Get output current and voltage measured by TA
 * and updates measured data
 */
static inline int pe5p_get_ta_cap(struct pe5p_algo_info *info)
{
	struct pe5p_algo_data *data = info->data;

	return pe5p_hal_get_ta_output(info->alg, &data->vta_measure,
				      &data->ita_measure);
}

/*
 * Get output current and voltage measured by TA
 * and updates measured data
 * If ta does not support measure capability, dvchg's ADC is used instead
 */
static inline int pe5p_get_ta_cap_by_supportive(struct pe5p_algo_info *info,
						 int *vta, int *ita)
{
	int ret;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_ta_auth_data *auth_data = &data->ta_auth_data;

	if (auth_data->support_meas_cap) {
		ret = pe5p_get_ta_cap(info);
		if (ret < 0) {
			PE5P_ERR("get ta cap fail(%d)\n", ret);
			return ret;
		}
		*vta = data->vta_measure;
		*ita = data->ita_measure;
		return 0;
	}
	ret = pe5p_get_adc(info, PE5P_ADCCHAN_VBUS, vta);
	if (ret < 0) {
		PE5P_ERR("get vbus fail(%d)\n", ret);
		return ret;
	}
	return pe5p_get_adc(info, PE5P_ADCCHAN_IBUS, ita);
}

/* Calculate ibat from ita */
static inline u32 pe5p_cal_ibat(struct pe5p_algo_info *info, u32 ita)
{
	struct pe5p_algo_data *data = info->data;

	return 2 * (data->is_swchg_en ? (ita - data->aicr_setting) : ita);
}

/*
 * Calculate calibrated output voltage of TA by measured resistence
 * Firstly, calculate voltage needed by divider charger
 * Finally, calculate voltage outputing from TA
 *
 * @ita: expected output current of TA
 * @vta: calibrated output voltage of TA
 */
static int pe5p_get_cali_vta(struct pe5p_algo_info *info, u32 ita, u32 *vta)
{
	int ret, vbat;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_ta_auth_data *auth_data = &data->ta_auth_data;
	u32 ibat, vbus, _vta, comp;

	ret = pe5p_get_adc(info, PE5P_ADCCHAN_VBAT, &vbat);
	if (ret < 0) {
		PE5P_ERR("get vbat fail(%d)\n", ret);
		return ret;
	}
	ibat = pe5p_cal_ibat(info, ita);
	vbus = pe5p_vout2vbus(info, vbat + div1000(ibat * data->r_sw));
	*vta = vbus + (data->vbus_cali + data->vta_comp +
	       div1000(ita * data->r_cable_by_swchg));
	if (data->is_dvchg_en[PE5P_HVDVCHG_MASTER]) {
		ret = pe5p_get_ta_cap(info);
		if (ret < 0) {
			PE5P_ERR("get ta cap fail(%d)\n", ret);
			return ret;
		}
		_vta = pe5p_vta_add_gap(info, data->vta_measure);
		if (_vta > *vta) {
			comp = _vta - *vta;
			data->vta_comp += comp;
			PE5P_DBG("comp,add=(%d,%d)\n", data->vta_comp, comp);
		}
		*vta = max(*vta, _vta);
	}
	if (*vta >= auth_data->vcap_max)
		*vta = auth_data->vcap_max;
	return 0;
}

/*
 * Tracking vbus of divider charger using vbusovp alarm
 * If vbusovp alarm is triggered, algorithm needs to come up with a new vbus
 */
static int pe5p_set_vbus_tracking(struct pe5p_algo_info *info)
{
	int ret, vbus;

	ret = pe5p_get_adc(info, PE5P_ADCCHAN_VBUS, &vbus);
	if (ret < 0) {
		PE5P_ERR("get vbus fail(%d)\n", ret);
		return ret;
	}
	return pe5p_hal_set_vbusovp_alarm(info->alg, HVDVCHG1,
					  vbus + PE5P_DVCHG_VBUSALM_GAP);
}

/* Calculate power limited ita according to TA's power limitation */
static u32 pe5p_get_ita_pwr_lmt_by_vta(struct pe5p_algo_info *info, u32 vta)
{
	struct pe5p_algo_data *data = info->data;
	struct pe5p_ta_auth_data *auth_data = &data->ta_auth_data;
	u32 ita_pwr_lmt;

	if (!auth_data->pwr_lmt)
		return data->ita_lmt;

	ita_pwr_lmt = precise_div(auth_data->pdp * 1000000, vta);
	/* Round to nearest level */
	if (auth_data->support_cc) {
		ita_pwr_lmt /= auth_data->ita_step;
		ita_pwr_lmt *= auth_data->ita_step;
	}
	return min(ita_pwr_lmt, data->ita_lmt);
}

static inline u32 pe5p_get_ita_tracking_max(u32 ita)
{
	return min_t(u32, percent(ita, PE5P_ITAOCP_RATIO),
		   (ita + PE5P_ITA_TRACKING_GAP));
}

/*
 * Set output capability of TA in CC mode and update setting in data
 *
 * @vta: output voltage of TA, mV
 * @ita: output current of TA, mA
 */
static int
pe5p_force_ta_cv(struct pe5p_algo_info *info, struct pe5p_stop_info *sinfo);
static inline int pe5p_set_ta_cap_cc(struct pe5p_algo_info *info, u32 vta,
				      u32 ita)
{
	int ret, vbat;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;
	struct pe5p_ta_auth_data *auth_data = &data->ta_auth_data;
	bool set_opt_vta = true;
	bool is_ta_cc = false;
	u32 opt_vta;
	struct pe5p_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	if (data->vta_setting == vta && data->ita_setting == ita &&
	    data->state != PE5P_ALGO_INIT)
		return 0;
	while (true) {
		if (atomic_read(&data->stop_algo)) {
			PE5P_INFO("stop algo\n");
			goto stop;
		}
		/* Check TA's PDP */
		data->ita_pwr_lmt = pe5p_get_ita_pwr_lmt_by_vta(info, vta);
		if (data->ita_pwr_lmt < ita) {
			PE5P_INFO("ita(%d) > ita_pwr_lmt(%d)\n", ita,
				 data->ita_pwr_lmt);
			ita = data->ita_pwr_lmt;
		}
		ret = pe5p_hal_set_ta_cap(info->alg, vta, ita);
		if (ret < 0) {
			PE5P_ERR("set ta cap fail(%d)\n", ret);
			return ret;
		}
		msleep(desc->ta_blanking);
		if (!data->is_dvchg_en[PE5P_HVDVCHG_MASTER])
			break;
		ret = pe5p_hal_is_ta_cc(info->alg, &is_ta_cc);
		if (ret < 0) {
			PE5P_ERR("get ta cc mode fail(%d)\n", ret);
			return ret;
		}
		ret = pe5p_get_ta_cap(info);
		if (ret < 0) {
			PE5P_ERR("get ta cap fail(%d)\n", ret);
			return ret;
		}
		PE5P_DBG("vta(set,meas,comp),ita(set,meas)=(%d,%d,%d),(%d,%d)\n",
			vta, data->vta_measure, data->vta_comp, ita,
			data->ita_measure);
		if (is_ta_cc) {
			opt_vta = pe5p_vta_add_gap(info, data->vta_measure);
			if (vta > opt_vta && set_opt_vta) {
				data->vta_comp -= (vta - opt_vta);
				vta = opt_vta;
				set_opt_vta = false;
				continue;
			}
			break;
		}
		if (vta >= auth_data->vcap_max) {
			PE5P_ERR("vta(%d) over capability(%d)\n", vta,
				auth_data->vcap_max);
			goto stop;
		}
		if (pe5p_is_hwerr_notified(info)) {
			PE5P_ERR("H/W error notified\n");
			goto stop;
		}
		ret = pe5p_get_adc(info, PE5P_ADCCHAN_VBAT, &vbat);
		if (ret < 0) {
			PE5P_ERR("get vbat fail(%d)\n", ret);
			return ret;
		}
		if (vbat >= data->vbat_cv) {
			PE5P_INFO("vbat(%d), decrease ita immediately\n", vbat);
			ita -= auth_data->ita_step;
			continue;
		}
		PE5P_ERR("Not in cc mode\n");
		if (data->ita_measure > pe5p_get_ita_tracking_max(data->ita_setting)) {
			ret = pe5p_force_ta_cv(info, &sinfo);
			if (ret < 0)
				goto stop;
			return 0;
		}
		set_opt_vta = false;
		data->vta_comp += auth_data->vta_step;
		vta += auth_data->vta_step;
		vta = min_t(u32, vta, auth_data->vcap_max);
	}
	data->vta_setting = vta;
	data->ita_setting = ita;
	PE5P_INFO("vta,ita = (%d,%d)\n", vta, ita);
	pe5p_set_vbus_tracking(info);

	return 0;
stop:
	pe5p_stop(info, &sinfo);
	return -EIO;
}

/*
 * Set TA's output voltage & current by a given current and
 * calculated voltage
 */
static inline int pe5p_set_ta_cap_cc_by_cali_vta(struct pe5p_algo_info *info,
						 u32 ita)
{
	int ret;
	u32 vta;

	ret = pe5p_get_cali_vta(info, ita, &vta);
	if (ret < 0) {
		PE5P_ERR("get cali vta fail(%d)\n", ret);
		return ret;
	}
	return pe5p_set_ta_cap_cc(info, vta, ita);
}

static inline void pe5p_update_ita_gap(struct pe5p_algo_info *info, u32 ita_gap)
{
	unsigned int i;
	u32 val = 0, avg_cnt = PE5P_ITA_GAP_WINDOW_SIZE;
	struct pe5p_algo_data *data = info->data;

	if (ita_gap < data->ita_gap_per_vstep)
		return;
	data->ita_gap_window_idx = (data->ita_gap_window_idx + 1) %
				   PE5P_ITA_GAP_WINDOW_SIZE;
	data->ita_gaps[data->ita_gap_window_idx] = ita_gap;

	for (i = 0; i < PE5P_ITA_GAP_WINDOW_SIZE; i++) {
		if (data->ita_gaps[i] == 0)
			avg_cnt--;
		else
			val += data->ita_gaps[i];
	}
	data->ita_gap_per_vstep = avg_cnt != 0 ? precise_div(val, avg_cnt) : 0;
}

static inline int pe5p_set_ta_cap_cv(struct pe5p_algo_info *info, u32 vta,
				     u32 ita)
{
	int ret, ita_meas_pre, ita_meas_post, vta_meas;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;
	struct pe5p_ta_auth_data *auth_data = &data->ta_auth_data;
	u32 vstep_cnt, ita_gap, vta_gap;
	struct pe5p_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	if (data->vta_setting == vta && data->ita_setting == ita)
		return 0;
	while (true) {
		if (pe5p_is_hwerr_notified(info)) {
			PE5P_ERR("H/W error notified\n");
			goto stop;
		}
		if (atomic_read(&data->stop_algo)) {
			PE5P_INFO("stop algo\n");
			goto stop;
		}
		if (vta > auth_data->vcap_max) {
			PE5P_ERR("vta(%d) over capability(%d)\n", vta,
				 auth_data->vcap_max);
			goto stop;
		}
		if (ita < auth_data->icap_min) {
			PE5P_INFO("ita(%d) under capability(%d)\n", ita,
				  auth_data->icap_min);
			ita = auth_data->icap_min;
		}
		vta_gap = abs(data->vta_setting - vta);

		/* Get ta cap before setting */
		ret = pe5p_get_ta_cap_by_supportive(info, &vta_meas,
						    &ita_meas_pre);
		if (ret < 0) {
			PE5P_ERR("get ta cap by supportive fail(%d)\n", ret);
			return ret;
		}

		/* Not to increase vta if it exceeds pwr_lmt */
		data->ita_pwr_lmt = pe5p_get_ita_pwr_lmt_by_vta(info, vta);
		if (vta > data->vta_setting &&
		    (data->ita_pwr_lmt <
		     ita_meas_pre + data->ita_gap_per_vstep)) {
			PE5P_INFO("ita_meas(%d) + ita_gap(%d) > pwr_lmt(%d)\n",
				  ita_meas_pre, data->ita_gap_per_vstep,
				  data->ita_pwr_lmt);
			return 0;
		}

		/* Set ta cap */
		ret = pe5p_hal_set_ta_cap(info->alg, vta, ita);
		if (ret < 0) {
			PE5P_ERR("set ta cap fail(%d)\n", ret);
			return ret;
		}
		if (vta_gap > auth_data->vta_step ||
		    data->state != PE5P_ALGO_SS_DVCHG)
			msleep(desc->ta_blanking);

		/* Get ta cap after setting */
		ret = pe5p_get_ta_cap_by_supportive(info, &vta_meas,
						    &ita_meas_post);
		if (ret < 0) {
			PE5P_ERR("get ta cap by supportive fail(%d)\n", ret);
			return ret;
		}

		if (data->is_dvchg_en[PE5P_HVDVCHG_MASTER] &&
		    (ita_meas_post > ita_meas_pre) &&
		    (vta > data->vta_setting)) {
			vstep_cnt = precise_div(max_t(u32, vta, vta_meas) -
						data->vta_setting,
						auth_data->vta_step);
			ita_gap = precise_div(ita_meas_post - ita_meas_pre,
					      vstep_cnt);
			pe5p_update_ita_gap(info, ita_gap);
			PE5P_INFO("ita gap(now,updated)=(%d,%d)\n",
				  ita_gap, data->ita_gap_per_vstep);
		}
		data->vta_setting = vta;
		data->ita_setting = ita;
		if (ita_meas_post <= pe5p_get_ita_tracking_max(ita))
			break;
		vta -= auth_data->vta_step;
		PE5P_INFO("ita_meas %dmA over setting %dmA, keep tracking...\n",
			  ita_meas_post, ita);
	}

	data->vta_measure = vta_meas;
	data->ita_measure = ita_meas_post;
	PE5P_INFO("vta(set,meas):(%d,%d),ita(set,meas):(%d,%d)\n",
		 data->vta_setting, data->vta_measure, data->ita_setting,
		 data->ita_measure);
	pe5p_hal_dump_registers(info->alg, HVDVCHG1);
	pe5p_hal_dump_registers(info->alg, HVDVCHG2);
	return 0;
stop:
	pe5p_stop(info, &sinfo);
	return -EIO;
}

static inline void pe5p_calculate_vbat_ircmp(struct pe5p_algo_info *info)
{
	int ret, ibat;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;
	u32 ircmp;

	if (!data->is_dvchg_en[PE5P_HVDVCHG_MASTER]) {
		data->vbat_ircmp = 0;
		return;
	}

	ret = pe5p_get_adc(info, PE5P_ADCCHAN_IBAT, &ibat);
	if (ret < 0) {
		PE5P_ERR("get ibat fail(%d)\n", ret);
		return;
	}
	ircmp = max(div1000(ibat * data->r_bat), desc->ircmp_vclamp);
	/*
	 * For safety,
	 * if state is CC_CV, ircmp can only be smaller than previous one
	 */
	if (data->state == PE5P_ALGO_CC_CV)
		ircmp = min(data->vbat_ircmp, ircmp);
	data->vbat_ircmp = min(desc->ircmp_vclamp, ircmp);
	PE5P_INFO("vbat_ircmp(vclamp,ibat,rbat)=%d(%d,%d,%d)\n",
		 data->vbat_ircmp, desc->ircmp_vclamp, ibat, data->r_bat);
}

static inline void pe5p_select_vbat_cv(struct pe5p_algo_info *info)
{
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;
	u32 cv = data->vbat_cv;
	u32 cv_no_ircmp = desc->vbat_cv;

	mutex_lock(&data->ext_lock);
	if (data->cv_limit > 0)
		cv_no_ircmp = min_t(u32, cv_no_ircmp, data->cv_limit);

	if (cv_no_ircmp != data->vbat_cv_no_ircmp)
		data->vbat_cv_no_ircmp = cv_no_ircmp;

	cv = data->vbat_cv_no_ircmp + data->vbat_ircmp;
	if (cv == data->vbat_cv)
		goto out;

	data->vbat_cv = cv;
	data->cv_lower_bound = data->vbat_cv - PE5P_CV_LOWER_BOUND_GAP;
out:
	PE5P_INFO("vbat_cv(org,limit,no_ircmp,low_bound)=%d(%d,%d,%d,%d)\n",
		  data->vbat_cv, desc->vbat_cv, data->cv_limit,
		  data->vbat_cv_no_ircmp, data->cv_lower_bound);
	mutex_unlock(&data->ext_lock);
}

/*
 * Select current limit according to severial status
 * If switching charger is charging, add AICR setting to ita
 * For now, the following features are taken into consider
 * 1. Resistence
 * 2. Phone's thermal throttling
 * 3. TA's power limit
 * 4. TA's temperature
 * 5. Battery's temperature
 * 6. Divider charger's temperature
 */
static inline int pe5p_get_ita_lmt(struct pe5p_algo_info *info)
{
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;
	u32 ita = data->ita_lmt;

	mutex_lock(&data->ext_lock);
	if (data->input_current_limit >= 0)
		ita = min_t(u32, ita, data->input_current_limit);
	if (data->ita_pwr_lmt > 0)
		ita = min(ita, data->ita_pwr_lmt);
	if (data->tried_dual_dvchg) {
		ita = min(ita, data->ita_lmt - (2 * desc->tta_curlmt[data->tta_level]));
		ita = min(ita, data->ita_lmt - (2 * desc->tbat_curlmt[data->tbat_level]));
		ita = min(ita, data->ita_lmt - (2 * desc->tdvchg_curlmt[data->tdvchg_level]));
	} else {
		ita = min(ita, data->ita_lmt - desc->tta_curlmt[data->tta_level]);
		ita = min(ita, data->ita_lmt - desc->tbat_curlmt[data->tbat_level]);
		ita = min(ita, data->ita_lmt - desc->tdvchg_curlmt[data->tdvchg_level]);
	}
	PE5P_INFO("ita(org,tta,tbat,tdvchg,prlmt,throt)=%d(%d,%d,%d,%d,%d,%d)\n",
		 ita, data->ita_lmt, desc->tta_curlmt[data->tta_level],
		 desc->tbat_curlmt[data->tbat_level],
		 desc->tdvchg_curlmt[data->tdvchg_level], data->ita_pwr_lmt,
		 data->input_current_limit);
	mutex_unlock(&data->ext_lock);
	return ita;
}

static inline int pe5p_get_idvchg_lmt(struct pe5p_algo_info *info)
{
	u32 ita_lmt, idvchg_lmt;
	struct pe5p_algo_data *data = info->data;

	ita_lmt = pe5p_get_ita_lmt(info);
	idvchg_lmt = min(data->idvchg_cc, ita_lmt);
	PE5P_INFO("idvchg_lmt(ita_lmt,idvchg_cc)=%d(%d,%d)\n", idvchg_lmt,
		 ita_lmt, data->idvchg_cc);
	return idvchg_lmt;
}

/* Calculate VBUSOV S/W level */
static u32 pe5p_get_dvchg_vbusovp(struct pe5p_algo_info *info, u32 ita)
{
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;
	u32 vout, ibat;

	ibat = pe5p_cal_ibat(info, ita);
	vout = desc->vbat_cv + div1000(ibat * data->r_sw);
	return min(percent(pe5p_vout2vbus(info, vout), PE5P_VBUSOVP_RATIO),
		   data->vbusovp);
}

/* Calculate IBUSOC S/W level */
static u32 pe5p_get_dvchg_ibusocp(struct pe5p_algo_info *info, u32 ita)
{
	struct pe5p_algo_data *data = info->data;
	u32 ibus, ratio = PE5P_IBUSOCP_RATIO;

	ibus = data->is_swchg_en ? (ita - data->aicr_setting) : ita;
	/* Add 10% for unbalance tolerance */
	if (data->is_dvchg_en[PE5P_HVDVCHG_SLAVE]) {
		ibus = precise_div(ibus, 2);
		ratio += 10;
	}
	return percent(ibus, ratio);
}

/* Calculate VBATOV S/W level */
static u32 pe5p_get_vbatovp(struct pe5p_algo_info *info)
{
	struct pe5p_algo_desc *desc = info->desc;

	return percent(desc->vbat_cv + desc->ircmp_vclamp, PE5P_VBATOVP_RATIO);
}

/* Calculate IBATOC S/W level */
static u32 pe5p_get_ibatocp(struct pe5p_algo_info *info, u32 ita)
{
	struct pe5p_algo_data *data = info->data;
	u32 ibat;

	ibat = pe5p_cal_ibat(info, ita);
	if (data->is_swchg_en)
		ibat += data->ichg_setting;
	return percent(ibat, PE5P_IBATOCP_RATIO);
}

/* Calculate ITAOC S/W level */
static u32 pe5p_get_itaocp(struct pe5p_algo_info *info)
{
	struct pe5p_algo_data *data = info->data;

	return percent(data->ita_setting, PE5P_ITAOCP_RATIO);
}

static int pe5p_set_dvchg_protection(struct pe5p_algo_info *info, bool dual)
{
	int ret;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;
	struct pe5p_ta_auth_data *auth_data = &data->ta_auth_data;
	u32 vout, idvchg_lmt;
	u32 vbusovp, ibusocp, vbatovp;

	/* VBUSOVP */
	vout = desc->vbat_cv + div1000(2 * data->idvchg_cc * data->r_sw);
	vbusovp = percent(pe5p_vout2vbus(info, vout), PE5P_VBUSOVP_RATIO);
	vbusovp = min_t(u32, vbusovp, auth_data->vcap_max);
	ret = pe5p_hal_set_vbusovp(info->alg, HVDVCHG1, vbusovp);
	if (ret < 0) {
		PE5P_ERR("set vbusovp fail(%d)\n", ret);
		return ret;
	}
	data->vbusovp = vbusovp;

	/* IBUSOCP */
	idvchg_lmt = min_t(u32, data->idvchg_cc, auth_data->ita_max);
	ibusocp = percent(idvchg_lmt, PE5P_IBUSOCP_RATIO);
	PE5P_INFO("ibusocp,idvchg_cc,ita_max,idvchg_lmt:%d,%d,%d,%d\n",
		   ibusocp, data->idvchg_cc, auth_data->ita_max, idvchg_lmt);
	if (data->is_dvchg_exist[PE5P_HVDVCHG_SLAVE] && dual) {
		ret = pe5p_hal_set_ibusocp(info->alg, HVDVCHG2, ibusocp);
		if (ret < 0) {
			PE5P_ERR("set slave ibusocp fail(%d)\n", ret);
			return ret;
		}
	}
	ret = pe5p_hal_set_ibusocp(info->alg, HVDVCHG1, ibusocp);
	if (ret < 0) {
		PE5P_ERR("set ibusocp fail(%d)\n", ret);
		return ret;
	}

	/* VBATOVP */
	vbatovp = percent(desc->vbat_cv + desc->ircmp_vclamp,
			  PE5P_VBATOVP_RATIO);
	ret = pe5p_hal_set_vbatovp(info->alg, HVDVCHG1, vbatovp);
	if (ret < 0) {
		PE5P_ERR("set vbatovp fail(%d)\n", ret);
		return ret;
	}

	PE5P_INFO("vbusovp,ibusocp,vbatovp = (%d,%d,%d)\n",
		 vbusovp, ibusocp, vbatovp);
	return 0;
}

static int pe5p_set_hv_dvchg_protection(struct pe5p_algo_info *info,
					enum chg_idx chgidx, bool start)
{
	int ret;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;
	struct pe5p_ta_auth_data *auth_data = &data->ta_auth_data;
	u32 ibusocp = start ? auth_data->ita_max : PE5P_ITA_INIT;
	u32 vbusovp = start ? desc->vta_cap_max : PE5P_VTA_INIT;
	u32 vbatovp = start ? precise_div(desc->vta_cap_max, 2) : PE5P_VTA_INIT;

	ret = pe5p_hal_set_ibusocp(info->alg, chgidx, ibusocp + 2000);
	if (ret < 0) {
		PE5P_ERR("set chgidx%d ibusocp fail(%d)\n", chgidx, ret);
		return ret;
	}
	ret = pe5p_hal_set_vbusovp(info->alg, chgidx, vbusovp + 2000);
	if (ret < 0) {
		PE5P_ERR("set chgidx%d vbusocp fail(%d)\n", chgidx, ret);
		return ret;
	}
	ret = pe5p_hal_set_vbatovp(info->alg, chgidx, vbatovp + 2000);
	if (ret < 0) {
		PE5P_ERR("set chgidx%d vbatocp fail(%d)\n", chgidx, ret);
		return ret;
	}
	return 0;
}

/*
 * Enable/Disable divider charger
 *
 * @en: enable/disable
 */
static int pe5p_enable_dvchg_charging(struct pe5p_algo_info *info,
				      enum pe5p_dvchg_role role, bool en)
{
	int ret;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;

	if (!data->is_dvchg_exist[role])
		return -ENODEV;
	if (data->is_dvchg_en[role] == en)
		return 0;
	PE5P_INFO("en[%s] = %d\n", pe5p_dvchg_role_name[role], en);
	ret = pe5p_hal_enable_charging(info->alg, to_chgidx(role), en);
	if (ret < 0) {
		PE5P_ERR("en chg fail(%d)\n", ret);
		return ret;
	}
	data->is_dvchg_en[role] = en;
	msleep(desc->ta_blanking);
	return 0;
}

/*
 * Set protection parameters, disable swchg and  enable divider charger
 *
 * @en: enable/disable
 */
static int pe5p_set_hvdvchg_charging(struct pe5p_algo_info *info, bool en)
{
	int ret;
	struct pe5p_algo_data *data = info->data;

	if (!data->is_dvchg_exist[PE5P_HVDVCHG_MASTER])
		return -ENODEV;

	PE5P_INFO("en = %d\n", en);

	if (en) {
		ret = pe5p_hal_enable_hz(info->alg, CHG1, true);
		if (ret < 0) {
			PE5P_ERR("set swchg hz fail(%d)\n", ret);
			return ret;
		}
		ret = pe5p_set_dvchg_protection(info, false);
		if (ret < 0) {
			PE5P_ERR("set protection fail(%d)\n", ret);
			return ret;
		}
		ret = pe5p_set_operating_mode(info->alg, HVDVCHG1, 1);
		if (ret)
			return ret;
	}
	ret = pe5p_enable_dvchg_charging(info, PE5P_HVDVCHG_MASTER, en);
	if (ret < 0)
		return ret;
	if (!en) {
		ret = pe5p_hal_enable_hz(info->alg, CHG1, false);
		if (ret < 0) {
			PE5P_ERR("disable buck boost chg hz fail(%d)\n", ret);
			return ret;
		}
	}

	return pe5p_hal_enable_charging(info->alg, CHG1, !en);
}

/*
 * Enable charging of switching charger
 * For divide by two algorithm, according to swchg_ichg to decide enable or not
 *
 * @en: enable/disable
 */
static int pe5p_enable_swchg_charging(struct pe5p_algo_info *info, bool en)
{
	int ret;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;

	PE5P_INFO("en = %d\n", en);
	if (en) {
		ret = pe5p_hal_enable_charging(info->alg, CHG1, true);
		if (ret < 0) {
			PE5P_ERR("en swchg fail(%d)\n", ret);
			return ret;
		}
		ret = pe5p_hal_enable_hz(info->alg, CHG1, false);
		if (ret < 0) {
			PE5P_ERR("disable hz fail(%d)\n", ret);
			return ret;
		}
	} else {
		ret = pe5p_hal_enable_hz(info->alg, CHG1, true);
		if (ret < 0) {
			PE5P_ERR("disable hz fail(%d)\n", ret);
			return ret;
		}
		ret = pe5p_hal_enable_charging(info->alg, CHG1, false);
		if (ret < 0) {
			PE5P_ERR("en swchg fail(%d)\n", ret);
			return ret;
		}
	}
	data->is_swchg_en = en;
	ret = pe5p_hal_set_vbatovp_alarm(info->alg, DVCHG1,
		en ? desc->swchg_off_vbat : desc->vbat_cv);
	if (ret < 0) {
		PE5P_ERR("set vbatovp alarm fail(%d)\n", ret);
		return ret;
	}
	return 0;
}

/*
 * Set AICR & ICHG of switching charger
 *
 * @aicr: setting of AICR
 * @ichg: setting of ICHG
 */
static int pe5p_set_swchg_cap(struct pe5p_algo_info *info, u32 aicr)
{
	int ret;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;
	u32 ichg, vbat, vbus;

	if (aicr == data->aicr_setting)
		goto set_ichg;
	ret = pe5p_hal_set_aicr(info->alg, CHG1, aicr);
	if (ret < 0) {
		PE5P_ERR("set aicr fail(%d)\n", ret);
		return ret;
	}
	data->aicr_setting = aicr;
set_ichg:
	ret = pe5p_get_adc(info, PE5P_ADCCHAN_VBAT, &vbat);
	if (ret < 0) {
		PE5P_ERR("get vbat fail(%d)\n", ret);
		return ret;
	}
	ret = pe5p_get_adc(info, PE5P_ADCCHAN_VBUS, &vbus);
	if (ret < 0) {
		PE5P_ERR("get vbus fail(%d)\n", ret);
		return ret;
	}
	/* 90% charging efficiency */
	ichg = precise_div(percent(vbus * aicr, 90), vbat);
	ichg = min(ichg, desc->swchg_ichg);
	if (ichg == data->ichg_setting)
		return 0;
	ret = pe5p_hal_set_ichg(info->alg, CHG1, ichg);
	if (ret < 0) {
		PE5P_ERR("set_ichg fail(%d)\n", ret);
		return ret;
	}
	data->ichg_setting = ichg;
	PE5P_INFO("AICR = %d, ICHG = %d\n", aicr, ichg);
	return 0;
}

/*
 * Enable TA by algo
 *
 * @en: enable/disable
 * @mV: requested output voltage
 * @mA: requested output current
 */
static int pe5p_enable_ta_charging(struct pe5p_algo_info *info, bool en, int mV,
				   int mA)
{
	int ret;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;
	u32 wdt = max_t(u32, desc->polling_interval * 2, PE5P_TA_WDT_MIN);

	PE5P_INFO("en = %d\n", en);
	if (en) {
		ret = pe5p_hal_set_ta_wdt(info->alg, wdt);
		if (ret < 0) {
			PE5P_ERR("set ta wdt fail(%d)\n", ret);
			return ret;
		}
		ret = pe5p_hal_enable_ta_wdt(info->alg, true);
		if (ret < 0) {
			PE5P_ERR("en ta wdt fail(%d)\n", ret);
			return ret;
		}
	}
	ret = pe5p_hal_enable_ta_charging(info->alg, en, mV, mA);
	if (ret < 0) {
		PE5P_ERR("en ta charging fail(%d)\n", ret);
		return ret;
	}
	if (!en) {
		ret = pe5p_hal_enable_ta_wdt(info->alg, false);
		if (ret < 0)
			PE5P_ERR("disable ta wdt fail(%d)\n", ret);
	}
	data->vta_setting = mV;
	data->ita_setting = mA;
	return ret;
}

static int pe5p_send_notification(struct pe5p_algo_info *info,
				  unsigned long val,
				  struct chg_alg_notify *notify)
{
	return srcu_notifier_call_chain(&info->alg->evt_nh, val, notify);
}

/* Stop PE5.0 charging and reset parameter */
static int pe5p_stop(struct pe5p_algo_info *info, struct pe5p_stop_info *sinfo)
{
	struct pe5p_algo_data *data = info->data;
	struct chg_alg_notify notify = {
		.evt = EVT_ALGO_STOP,
	};
	int ret = 0;

	if (data->state == PE5P_ALGO_STOP) {
		/*
		 * Always clear stop_algo,
		 * in case it is called from pe5p_stop_algo
		 */
		atomic_set(&data->stop_algo, 0);
		PE5P_DBG("already stop\n");
		return 0;
	}

	PE5P_INFO("reset ta(%d), hardreset ta(%d)\n", sinfo->reset_ta,
		 sinfo->hardreset_ta);
	data->state = PE5P_ALGO_STOP;
	atomic_set(&data->stop_algo, 0);
	alarm_cancel(&data->timer);

	if (data->is_swchg_en)
		pe5p_enable_swchg_charging(info, false);
	ret = pe5p_enable_dvchg_charging(info, PE5P_HVDVCHG_SLAVE, false);
	if (ret < 0) {
		PE5P_ERR("disable slave hvdvchg fail(%d)\n", ret);
		return ret;
	}
	ret = pe5p_set_hvdvchg_charging(info, false);
	if (ret < 0) {
		PE5P_ERR("en dvchg fail\n");
		return ret;
	}
	if (!(data->notify & PE5P_RESET_NOTIFY)) {
		if (sinfo->hardreset_ta)
			pe5p_hal_send_ta_hardreset(info->alg);
		else if (sinfo->reset_ta) {
			pe5p_hal_set_ta_cap(info->alg, PE5P_VTA_INIT,
					    PE5P_ITA_INIT);
			pe5p_enable_ta_charging(info, false, PE5P_VTA_INIT,
						PE5P_ITA_INIT);
		}
	}
	pe5p_set_hv_dvchg_protection(info, to_chgidx(PE5P_HVDVCHG_MASTER),
				     false);
	pe5p_set_hv_dvchg_protection(info, to_chgidx(PE5P_HVDVCHG_SLAVE),
				     false);
	pe5p_hal_enable_sw_vbusovp(info->alg, true);
//	pe5p_hal_set_vacovp(info->alg, CHG1, 7000);
	pe5p_send_notification(info, EVT_ALGO_STOP, &notify);
	return 0;
}

static inline void pe5p_init_algo_data(struct pe5p_algo_info *info)
{
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;
	struct pe5p_ta_auth_data *auth_data = &data->ta_auth_data;
	u32 *rcable_level = desc->rcable_level;
	u32 *ita_level = desc->ita_level;

	data->ita_lmt = min_t(u32, ita_level[PE5P_RCABLE_NORMAL],
			    auth_data->ita_max);
	data->idvchg_ss_init = max_t(u32, data->idvchg_ss_init,
				auth_data->icap_min);
	data->idvchg_ss_init = min(data->idvchg_ss_init, data->ita_lmt);
	data->ita_pwr_lmt = 0;
	data->idvchg_cc = ita_level[PE5P_RCABLE_NORMAL] - desc->swchg_aicr;
	data->idvchg_term = desc->idvchg_term;
	data->err_retry_cnt = 0;
	data->is_swchg_en = false;
	data->is_dvchg_en[PE5P_BUCK_BSTCHG] = true;
	data->is_dvchg_en[PE5P_HVDVCHG_MASTER] = false;
	data->is_dvchg_en[PE5P_HVDVCHG_SLAVE] = false;
	data->suspect_ta_cc = false;
	data->aicr_setting = 0;
	data->ichg_setting = 0;
	data->vta_setting = PE5P_VTA_INIT;
	data->ita_setting = PE5P_ITA_INIT;
	data->ita_gap_per_vstep = 0;
	data->ita_gap_window_idx = 0;
	memset(data->ita_gaps, 0, sizeof(data->ita_gaps));
	data->is_vbat_over_cv = false;
	data->ignore_ibusucpf = false;
	data->force_ta_cv = false;
	data->vbat_cv = desc->vbat_cv;
	data->vbat_cv_no_ircmp = desc->vbat_cv;
	data->cv_lower_bound = desc->vbat_cv - PE5P_CV_LOWER_BOUND_GAP;
	data->vta_comp = 0;
	data->zcv = 0;
	data->r_bat = desc->ircmp_rbat;
	data->r_sw = desc->rsw_min;
	data->r_cable = rcable_level[PE5P_RCABLE_NORMAL];
	data->r_cable_by_swchg = rcable_level[PE5P_RCABLE_NORMAL];
	data->tbat_level = PE5P_THERMAL_NORMAL;
	data->tta_level = PE5P_THERMAL_NORMAL;
	data->tdvchg_level = PE5P_THERMAL_NORMAL;
	data->tswchg_level = PE5P_THERMAL_NORMAL;
	data->run_once = true;
	data->state = PE5P_ALGO_INIT;
	mutex_lock(&data->notify_lock);
	data->notify = 0;
	mutex_unlock(&data->notify_lock);
	data->stime = ktime_get_boottime();
}

static int pe5p_earily_restart(struct pe5p_algo_info *info)
{
	int ret;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_ta_auth_data *auth_data = &data->ta_auth_data;

	ret = pe5p_enable_dvchg_charging(info, PE5P_HVDVCHG_SLAVE, false);
	if (ret < 0) {
		PE5P_ERR("disable slave hvdvchg fail(%d)\n", ret);
		return ret;
	}
	ret = pe5p_enable_dvchg_charging(info, PE5P_HVDVCHG_MASTER, false);
	if (ret < 0) {
		PE5P_ERR("disable master hvdvchg fail(%d)\n", ret);
		return ret;
	}
	if (auth_data->support_cc) {
		ret = pe5p_set_ta_cap_cc(info, PE5P_VTA_INIT, PE5P_ITA_INIT);
		if (ret < 0) {
			PE5P_ERR("set ta cap fail(%d)\n", ret);
			return ret;
		}
	}
	ret = pe5p_enable_ta_charging(info, false, PE5P_VTA_INIT,
				      PE5P_ITA_INIT);
	if (ret < 0) {
		PE5P_ERR("disable ta charging fail(%d)\n", ret);
		return ret;
	}
	pe5p_init_algo_data(info);
	return 0;
}

/*
 * Start pe50 timer and run algo
 * It cannot start algo again if algo has been started once before
 * Run once flag will be reset after plugging out TA
 */
static inline int pe5p_start(struct pe5p_algo_info *info)
{
	int ret, ibus, vbat, vbus, ita, i;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;
	struct pe5p_ta_auth_data *auth_data = &data->ta_auth_data;
	ktime_t ktime = ktime_set(0, MS_TO_NS(PE5P_INIT_POLLING_INTERVAL));

	PE5P_DBG("++\n");

	if (data->run_once) {
		PE5P_ERR("already run PE5.0 once\n");
		return -EINVAL;
	}

	if (data->is_dvchg_exist[PE5P_HVDVCHG_MASTER]) {
		ret = pe5p_set_hv_dvchg_protection(info,
					  to_chgidx(PE5P_HVDVCHG_MASTER), true);
		if (ret < 0) {
			PE5P_ERR("set hv dvchg1 protection fail(%d)\n", ret);
			return ret;
		}

		ret = pe5p_hal_init_chip(info->alg,
					 to_chgidx(PE5P_HVDVCHG_MASTER));
		if (ret < 0) {
			PE5P_ERR("init hv dvchg1 chip fail(%d)\n", ret);
			return ret;
		}
	}

	if (data->is_dvchg_exist[PE5P_HVDVCHG_SLAVE]) {
		ret = pe5p_set_hv_dvchg_protection(info,
					  to_chgidx(PE5P_HVDVCHG_SLAVE), true);
		if (ret < 0) {
			PE5P_ERR("set hv dvchg2 protection fail(%d)\n", ret);
			return ret;
		}

		ret = pe5p_hal_init_chip(info->alg,
					 to_chgidx(PE5P_HVDVCHG_SLAVE));
		if (ret < 0) {
			PE5P_ERR("init hv dvchg2 chip fail(%d)\n", ret);
			return ret;
		}
	}

	ret = pe5p_set_ta_cap_cv(info, 9000, 2000);
	if (ret < 0)
		PE5P_ERR("set ta cap fail(%d)\n", ret);

	data->idvchg_ss_init = desc->idvchg_ss_init;
	ret = pe5p_hal_set_aicr(info->alg, CHG1, 3000);
	if (ret < 0) {
		PE5P_ERR("set aicr fail(%d)\n", ret);
		goto start;
	}
	ret = pe5p_hal_set_ichg(info->alg, CHG1, 3000);
	if (ret < 0) {
		PE5P_ERR("set ichg fail(%d)\n", ret);
		goto start;
	}
	ret = pe5p_hal_enable_charging(info->alg, CHG1, true);
	if (ret < 0) {
		PE5P_ERR("en buck boost chg fail(%d)\n", ret);
		goto start;
	}
	msleep(1000);
	ret = pe5p_hal_get_adc(info->alg, CHG1, PE5P_ADCCHAN_VBUS, &vbus);
	if (ret < 0) {
		PE5P_ERR("get swchg vbus fail(%d)\n", ret);
		goto start;
	}
	ret = pe5p_hal_get_adc(info->alg, CHG1, PE5P_ADCCHAN_IBUS, &ibus);
	if (ret < 0) {
		PE5P_ERR("get swchg ibus fail(%d)\n", ret);
		goto start;
	}
	ret = pe5p_hal_get_adc(info->alg, CHG1, PE5P_ADCCHAN_VBAT, &vbat);
	if (ret < 0) {
		PE5P_ERR("get swchg vbat fail(%d)\n", ret);
		goto start;
	}
	PE5P_INFO("vbus=%d,ibus=%d,vbat=%d\n", vbus, ibus, vbat);
	ita = precise_div(percent(vbus * ibus, 90), vbat);
	if (ita < desc->idvchg_term) {
		PE5P_ERR("estimated ita(%d) < idvchg_term(%d)\n", ita,
			desc->idvchg_term);
		return -EINVAL;
	}
	/* Update idvchg_ss_init */
	if (ita >= auth_data->icap_min) {
		PE5P_INFO("set idvchg_ss_init(%d)->(%d)\n",
			  desc->idvchg_ss_init, ita);
		data->idvchg_ss_init = ita;
	}
start:
	/* disable buck boost charger */
	ret = pe5p_hal_enable_charging(info->alg, CHG1, false);
	if (ret < 0) {
		PE5P_ERR("disable charger fail\n");
		return ret;
	}
	msleep(1000); /* wait for battery to recovery */

	/* Check HVDVCHG registers stat first */
	for (i = PE5P_DVCHG_MASTER; i < PE5P_DVCHG_MAX; i++) {
		if (!data->is_dvchg_exist[i])
			continue;
		ret = pe5p_hal_init_chip(info->alg, to_chgidx(i));
		if (ret < 0) {
			PE5P_ERR("(%s) init chip fail(%d)\n",
				pe5p_dvchg_role_name[i], ret);
			return ret;
		}
	}

	/* Parameters that only reset by restarting from outside */
	mutex_lock(&data->ext_lock);
	data->input_current_limit = -1;
	data->cv_limit = -1;
	mutex_unlock(&data->ext_lock);
	data->tried_dual_dvchg = false;
	pe5p_init_algo_data(info);
	alarm_start_relative(&data->timer, ktime);
	return 0;
}

/* =================================================================== */
/* PE5.0 Algo State Machine                                              */
/* =================================================================== */

static int pe5p_calculate_rcable_by_swchg(struct pe5p_algo_info *info)
{
	struct pe5p_algo_data *data = info->data;
	int vbus1 = 0, vbus2 = 0, vbus_max = 0, vbus_min = 0;
	int ibus1 = 0, ibus2 = 0, ibus_max = 0, ibus_min = 0;
	int ret = 0, aicr = 0, ichg = 0, i = 0;
	int val_vbus = 0, val_ibus = 0;

	ret = pe5p_hal_get_aicr(info->alg, CHG1, &aicr);
	if (ret < 0) {
		PE5P_ERR("get aicr fail(%d)\n", ret);
		return ret;
	}

	ret = pe5p_hal_get_ichg(info->alg, CHG1, &ichg);
	if (ret < 0) {
		PE5P_ERR("get ichg fail(%d)\n", ret);
		return ret;
	}

	ret = pe5p_hal_set_aicr(info->alg, CHG1, 300);
	if (ret < 0) {
		PE5P_ERR("set aicr fail(%d)\n", ret);
		return ret;
	}

	ret = pe5p_hal_set_ichg(info->alg, CHG1, 3000);
	if (ret < 0) {
		PE5P_ERR("set ichg fail(%d)\n", ret);
		return ret;
	}

	pe5p_hal_enable_charging(info->alg, CHG1, true);

	for (i = 0; i < PE5P_MEASURE_R_AVG_TIMES + 2; i++) {
		ret = pe5p_hal_get_adc(info->alg, CHG1,
					PE5P_ADCCHAN_VBUS, &val_vbus);
		if (ret < 0) {
			PE5P_ERR("get vbus fail(%d)\n", ret);
			return ret;
		}
		ret = pe5p_hal_get_adc(info->alg, CHG1,
				       PE5P_ADCCHAN_IBUS, &val_ibus);
		if (ret < 0) {
			PE5P_ERR("get ibus fail(%d)\n", ret);
			return ret;
		}

		if (i == 0) {
			vbus_max = vbus_min = val_vbus;
			ibus_max = ibus_min = val_ibus;
		} else {
			vbus_max = max(vbus_max, val_vbus);
			ibus_max = max(ibus_max, val_ibus);
			vbus_min = min(vbus_min, val_vbus);
			ibus_min = min(ibus_min, val_ibus);
		}
		vbus1 += val_vbus;
		ibus1 += val_ibus;
		PE5P_ERR("vbus=%d ibus=%d vbus(max,min)=(%d,%d)",
			val_vbus, val_ibus, vbus_max, vbus_min);
		PE5P_ERR("ibus(max,min)=(%d,%d) vbus1=%d ibus1=%d",
			ibus_max, ibus_min, vbus1, ibus1);
	}

	vbus1 -= (vbus_min + vbus_max);
	vbus1 = precise_div(vbus1, PE5P_MEASURE_R_AVG_TIMES);

	ibus1 -= (ibus_min + ibus_max);
	ibus1 = precise_div(ibus1, PE5P_MEASURE_R_AVG_TIMES);

	ret = pe5p_hal_set_aicr(info->alg, CHG1, 400);
	if (ret < 0) {
		PE5P_ERR("set aicr fail(%d)\n", ret);
		return ret;
	}

	for (i = 0; i < PE5P_MEASURE_R_AVG_TIMES + 2; i++) {
		ret = pe5p_hal_get_adc(info->alg, CHG1,
				       PE5P_ADCCHAN_VBUS, &val_vbus);
		if (ret < 0) {
			PE5P_ERR("get vbus fail(%d)\n", ret);
			return ret;
		}
		ret = pe5p_hal_get_adc(info->alg, CHG1,
				       PE5P_ADCCHAN_IBUS, &val_ibus);
		if (ret < 0) {
			PE5P_ERR("get ibus fail(%d)\n", ret);
			return ret;
		}

		if (i == 0) {
			vbus_max = vbus_min = val_vbus;
			ibus_max = ibus_min = val_ibus;
		} else {
			vbus_max = max(vbus_max, val_vbus);
			ibus_max = max(ibus_max, val_ibus);
			vbus_min = min(vbus_min, val_vbus);
			ibus_min = min(ibus_min, val_ibus);
		}
		vbus2 += val_vbus;
		ibus2 += val_ibus;
		PE5P_ERR("vbus=%d ibus=%d vbus(max,min)=(%d,%d)",
			val_vbus, val_ibus, vbus_max, vbus_min);
		PE5P_ERR("ibus(max,min)=(%d,%d) vbus2=%d ibus2=%d",
			ibus_max, ibus_min, vbus2, ibus2);
	}

	vbus2 -= (vbus_min + vbus_max);
	vbus2 = precise_div(vbus2, PE5P_MEASURE_R_AVG_TIMES);

	ibus2 -= (ibus_min + ibus_max);
	ibus2 = precise_div(ibus2, PE5P_MEASURE_R_AVG_TIMES);

	data->r_cable_by_swchg = precise_div(abs(vbus2 - vbus1) * 1000,
					     abs(ibus2 - ibus1));

	pe5p_hal_enable_charging(info->alg, CHG1, false);

	ret = pe5p_hal_set_aicr(info->alg, CHG1, aicr);
	if (ret < 0) {
		PE5P_ERR("set aicr fail(%d)\n", ret);
		return ret;
	}

	ret = pe5p_hal_set_ichg(info->alg, CHG1, ichg);
	if (ret < 0) {
		PE5P_ERR("set ichg fail(%d)\n", ret);
		return ret;
	}

	return 0;
}

static int pe5p_algo_init_with_ta_cc(struct pe5p_algo_info *info)
{
	int ret, i, vbus, vbat;
	int ita_avg = 0, vta_avg = 0, vbus_avg = 0, vbat_avg = 0;
	const int avg_times = 10;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;
	struct pe5p_ta_auth_data *auth_data = &data->ta_auth_data;
	struct pe5p_stop_info sinfo = {
		.hardreset_ta = false,
		.reset_ta = true,
	};

	PE5P_DBG("++\n");

	/* Change charging policy first */
	ret = pe5p_enable_ta_charging(info, true, PE5P_VTA_INIT, PE5P_ITA_INIT);
	if (ret < 0) {
		PE5P_ERR("enable ta charging fail(%d)\n", ret);
		sinfo.hardreset_ta = true;
		goto err;
	}

	/* Check VBAT after disabling CHG_EN and before enabling HZ */
	for (i = 0; i < avg_times; i++) {
		ret = pe5p_get_adc(info, PE5P_ADCCHAN_VBAT, &vbat);
		if (ret < 0) {
			PE5P_ERR("get vbus fail(%d)\n", ret);
			goto err;
		}
		vbat_avg += vbat;
	}
	vbat_avg = precise_div(vbat_avg, avg_times);
	data->zcv = vbat_avg;

	if (vbat_avg > desc->start_vbat_max) {
		PE5P_INFO("finish PE5.0 Plus, vbat(%d) > %d\n", vbat_avg,
			 desc->start_vbat_max);
		goto out;
	}

	ret = pe5p_set_ta_cap_cv(info, 9000, auth_data->ita_max);
	if (ret < 0) {
		PE5P_ERR("set ta cap fail(%d)\n", ret);
		goto err;
	}

	ret = pe5p_calculate_rcable_by_swchg(info);
	if (ret < 0)
		PE5P_ERR("calculate rcable by swchg fail(%d)\n", ret);

	ret = pe5p_hal_enable_hz(info->alg, CHG1, true);
	if (ret < 0) {
		PE5P_ERR("set buckchg hz fail(%d)\n", ret);
		goto err;
	}
	msleep(500); /* Wait current stable */

	/* Initial setting, no need to check ita_lmt */
	ret = pe5p_set_ta_cap_cc_by_cali_vta(info, data->idvchg_ss_init);
	if (ret < 0) {
		PE5P_ERR("set ta cap by algo fail(%d)\n", ret);
		sinfo.hardreset_ta = true;
		goto err;
	}

	for (i = 0; i < avg_times; i++) {
		ret = pe5p_get_ta_cap(info);
		if (ret < 0) {
			PE5P_ERR("get ta cap fail(%d)\n", ret);
			sinfo.hardreset_ta = true;
			goto err;
		}
		ret = pe5p_get_adc(info, PE5P_ADCCHAN_VBUS, &vbus);
		if (ret < 0) {
			PE5P_ERR("get vbus fail(%d)\n", ret);
			goto err;
		}
		ita_avg += data->ita_measure;
		vta_avg += data->vta_measure;
		vbus_avg += vbus;
	}
	ita_avg = precise_div(ita_avg, avg_times);
	vta_avg = precise_div(vta_avg, avg_times);
	vbus_avg = precise_div(vbus_avg, avg_times);

	/* vbus calibration: voltage difference between TA & device */
	data->vbus_cali = vta_avg - vbus_avg;
	PE5P_INFO("avg(ita,vta,vbus,vbat):(%d, %d, %d, %d), vbus cali:%d\n",
		  ita_avg, vta_avg, vbus_avg, vbat_avg, data->vbus_cali);
	if (abs(data->vbus_cali) > PE5P_VBUS_CALI_THRESHOLD) {
		PE5P_ERR("vbus cali (%d) > (%d)\n", data->vbus_cali,
			 PE5P_VBUS_CALI_THRESHOLD);
		goto err;
	}
	if (ita_avg > desc->ifod_threshold) {
		PE5P_ERR("foreign object detected, ita(%d) > (%d)\n",
			 ita_avg, desc->ifod_threshold);
		goto err;
	}

	ret = pe5p_set_hvdvchg_charging(info, true);
	if (ret < 0) {
		PE5P_ERR("en dvchg fail\n");
		goto err;
	}

	ret = pe5p_set_ta_cap_cc_by_cali_vta(info, data->idvchg_ss_init);
	if (ret < 0) {
		PE5P_ERR("set ta cap by algo fail(%d)\n", ret);
		sinfo.hardreset_ta = true;
		goto err;
	}
	data->err_retry_cnt = 0;
	data->state = PE5P_ALGO_MEASURE_R;
	return 0;

err:
	if (data->err_retry_cnt < PE5P_INIT_RETRY_MAX) {
		data->err_retry_cnt++;
		return 0;
	}
out:
	return pe5p_stop(info, &sinfo);
}

static int pe5p_algo_init_with_ta_cv(struct pe5p_algo_info *info)
{
	int ret, i, vbus, vbat, vout;
	int ita_avg = 0, vta_avg = 0, vbus_avg = 0, vbat_avg = 0;
	bool err;
	u32 vta;
	const int avg_times = 10;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;
	struct pe5p_ta_auth_data *auth_data = &data->ta_auth_data;
	struct pe5p_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	PE5P_DBG("++\n");

	/* Change charging policy first */
	ret = pe5p_enable_ta_charging(info, true, PE5P_VTA_INIT, PE5P_ITA_INIT);
	if (ret < 0) {
		PE5P_ERR("enable ta charge fail(%d)\n", ret);
		sinfo.hardreset_ta = true;
		goto err;
	}

	/* Check VBAT after disabling CHG_EN and before enabling HZ */
	for (i = 0; i < avg_times; i++) {
		ret = pe5p_get_adc(info, PE5P_ADCCHAN_VBAT, &vbat);
		if (ret < 0) {
			PE5P_ERR("get vbus fail(%d)\n", ret);
			goto err;
		}
		vbat_avg += vbat;
	}
	vbat_avg = precise_div(vbat_avg, avg_times);
	data->zcv = vbat_avg;
	PE5P_INFO("avg(vbat):(%d)\n", vbat_avg);

	if (vbat_avg >= desc->start_vbat_max) {
		PE5P_INFO("finish PE5.0, vbat(%d) > %d\n", vbat_avg,
			  desc->start_vbat_max);
		goto out;
	}

	ret = pe5p_set_ta_cap_cv(info, 9000, auth_data->ita_max);
	if (ret < 0) {
		PE5P_ERR("set ta cap fail(%d)\n", ret);
		goto err;
	}

	ret = pe5p_calculate_rcable_by_swchg(info);
	if (ret < 0)
		PE5P_ERR("calculate rcable by swchg fail(%d)\n", ret);

	ret = pe5p_hal_enable_hz(info->alg, CHG1, true);
	if (ret < 0) {
		PE5P_ERR("set buckchg hz fail(%d)\n", ret);
		goto err;
	}
	msleep(500); /* Wait current stable */

	ret = pe5p_get_adc(info, PE5P_ADCCHAN_VBUS, &vbus);
	if (ret < 0) {
		PE5P_ERR("get vbus fail(%d)\n", ret);
		goto err;
	}
	ret = pe5p_hal_sync_ta_volt(info->alg, vbus);
	if (ret < 0 && ret != -EOPNOTSUPP) {
		PE5P_ERR("sync ta setting fail(%d)\n", ret);
		goto err;
	}
	ret = pe5p_get_adc(info, PE5P_ADCCHAN_VBAT, &vout);
	if (ret < 0) {
		PE5P_ERR("get vout fail(%d)\n", ret);
		goto err;
	}

	/* Adjust VBUS to make sure DVCHG can be turned on */
	vta = pe5p_vout2vbus(info, vout);
	ret = pe5p_set_ta_cap_cv(info, vta, data->idvchg_ss_init);
	if (ret < 0) {
		PE5P_ERR("set ta cap fail(%d)\n", ret);
		goto err;
	}
	while (true) {
		ret = pe5p_hal_is_vbuslowerr(info->alg, HVDVCHG1, &err);
		if (ret < 0) {
			PE5P_ERR("get vbuslowerr fail(%d)\n", ret);
			goto err;
		}
		if (!err)
			break;
		vta = data->vta_setting + auth_data->vta_step;
		ret = pe5p_set_ta_cap_cv(info, vta, data->idvchg_ss_init);
		if (ret < 0) {
			PE5P_ERR("set ta cap fail(%d)\n", ret);
			goto err;
		}
	}

	for (i = 0; i < avg_times; i++) {
		if (auth_data->support_meas_cap) {
			ret = pe5p_get_ta_cap(info);
			if (ret < 0) {
				PE5P_ERR("get ta cap fail(%d)\n", ret);
				sinfo.hardreset_ta = true;
				goto err;
			}
			ita_avg += data->ita_measure;
			vta_avg += data->vta_measure;
			ret = pe5p_get_adc(info, PE5P_ADCCHAN_VBUS, &vbus);
			if (ret < 0) {
				PE5P_ERR("get vbus fail(%d)\n", ret);
				goto err;
			}
			vbus_avg += vbus;
		}
	}
	if (auth_data->support_meas_cap) {
		ita_avg = precise_div(ita_avg, avg_times);
		vta_avg = precise_div(vta_avg, avg_times);
		vbus_avg = precise_div(vbus_avg, avg_times);
	}

	if (auth_data->support_meas_cap) {
		/* vbus calibration: voltage difference between TA & device */
		data->vbus_cali = vta_avg - vbus_avg;
		PE5P_INFO("avg(ita,vta,vbus):(%d,%d,%d), vbus_cali:%d\n",
			  ita_avg, vta_avg, vbus_avg, data->vbus_cali);
		if (abs(data->vbus_cali) > PE5P_VBUS_CALI_THRESHOLD) {
			PE5P_ERR("vbus cali (%d) > (%d)\n", data->vbus_cali,
				 PE5P_VBUS_CALI_THRESHOLD);
			goto err;
		}
		if (ita_avg > desc->ifod_threshold) {
			PE5P_ERR("foreign object detected, ita(%d) > (%d)\n",
				 ita_avg, desc->ifod_threshold);
			goto err;
		}
	}

	ret = pe5p_set_hvdvchg_charging(info, true);
	if (ret < 0) {
		PE5P_ERR("en dvchg fail\n");
		goto err;
	}

	/* Get ita measure after enable dvchg */
	ret = pe5p_get_ta_cap_by_supportive(info, &data->vta_measure,
					    &data->ita_measure);
	if (ret < 0) {
		PE5P_ERR("get ta cap fail(%d)\n", ret);
		sinfo.hardreset_ta = auth_data->support_meas_cap;
		goto out;
	}
	data->err_retry_cnt = 0;
	data->state = PE5P_ALGO_MEASURE_R;
	return 0;
err:
	if (data->err_retry_cnt < PE5P_INIT_RETRY_MAX) {
		data->err_retry_cnt++;
		return 0;
	}
out:
	return pe5p_stop(info, &sinfo);
}

/*
 * PE5.0 algorithm initial state
 * It does Foreign Object Detection(FOD)
 */
static int pe5p_algo_init(struct pe5p_algo_info *info)
{
	struct pe5p_algo_data *data = info->data;
	struct pe5p_ta_auth_data *auth_data = &data->ta_auth_data;

	return auth_data->support_cc ? pe5p_algo_init_with_ta_cc(info) :
				       pe5p_algo_init_with_ta_cv(info);
}

struct meas_r_info {
	u32 vbus;
	u32 ibus;
	u32 vbat;
	u32 ibat;
	u32 vout;
	u32 vta;
	u32 ita;
	u32 r_cable;
	u32 r_bat;
	u32 r_sw;
};

static int pe5p_algo_get_r_info(struct pe5p_algo_info *info,
				struct meas_r_info *r_info,
				struct pe5p_stop_info *sinfo)
{
	int ret;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_ta_auth_data *auth_data = &data->ta_auth_data;

	memset(r_info, 0, sizeof(struct meas_r_info));
	if (auth_data->support_meas_cap) {
		ret = pe5p_get_ta_cap(info);
		if (ret < 0) {
			PE5P_ERR("get ta cap fail(%d)\n", ret);
			sinfo->hardreset_ta = true;
			return ret;
		}
		r_info->ita = data->ita_measure;
		r_info->vta = data->vta_measure;
	}
	ret = pe5p_get_adc(info, PE5P_ADCCHAN_VBUS, &r_info->vbus);
	if (ret < 0) {
		PE5P_ERR("get vbus fail(%d)\n", ret);
		return ret;
	}
	ret = pe5p_get_adc(info, PE5P_ADCCHAN_IBUS, &r_info->ibus);
	if (ret < 0) {
		PE5P_ERR("get ibus fail(%d)\n", ret);
		return ret;
	}
	ret = pe5p_get_adc(info, PE5P_ADCCHAN_VBAT, &r_info->vout);
	if (ret < 0) {
		PE5P_ERR("get vout fail(%d)\n", ret);
		return ret;
	}
	ret = pe5p_get_adc(info, PE5P_ADCCHAN_VBAT, &r_info->vbat);
	if (ret < 0) {
		PE5P_ERR("get vbat fail(%d)\n", ret);
		return ret;
	}
	ret = pe5p_get_adc(info, PE5P_ADCCHAN_IBAT, &r_info->ibat);
	if (ret < 0) {
		PE5P_ERR("get ibat fail(%d)\n", ret);
		return ret;
	}
	PE5P_DBG("vta:%d,ita:%d,vbus:%d,ibus:%d,vout:%d,vbat:%d,ibat:%d\n",
		 r_info->vta, r_info->ita, r_info->vbus, r_info->ibus,
		 r_info->vout, r_info->vbat, r_info->ibat);
	return 0;
}

static int pe5p_algo_cal_r_info_with_ta_cap(struct pe5p_algo_info *info,
					    struct pe5p_stop_info *sinfo)
{
	int ret, i;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;
	struct meas_r_info r_info, max_r_info, min_r_info;
	struct pe5p_stop_info _sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	memset(&max_r_info, 0, sizeof(struct meas_r_info));
	memset(&min_r_info, 0, sizeof(struct meas_r_info));
	data->r_bat = data->r_sw = data->r_cable = 0;
	for (i = 0; i < PE5P_MEASURE_R_AVG_TIMES + 2; i++) {
		if (atomic_read(&data->stop_algo)) {
			PE5P_INFO("stop algo\n");
			goto stop;
		}
		ret = pe5p_algo_get_r_info(info, &r_info, sinfo);
		if (ret < 0) {
			PE5P_ERR("get r info fail(%d)\n", ret);
			return ret;
		}
		if (r_info.ibat == 0) {
			PE5P_ERR("ibat == 0 fail\n");
			return -EINVAL;
		}
		if (r_info.ita == 0) {
			PE5P_ERR("ita == 0 fail\n");
			sinfo->hardreset_ta = true;
			return -EINVAL;
		}
		if (r_info.ita < data->idvchg_term &&
		    r_info.vbat >= data->vbat_cv) {
			PE5P_INFO("finish PE5.0 charging\n");
			return -EINVAL;
		}

		/* Use absolute instead of relative calculation */
		r_info.r_bat = precise_div(abs(r_info.vbat - data->zcv) * 1000,
					   abs(r_info.ibat));
		if (r_info.r_bat > desc->ircmp_rbat)
			r_info.r_bat = desc->ircmp_rbat;

		r_info.r_sw = precise_div(abs(r_info.vout - r_info.vbat) * 1000,
					  abs(r_info.ibat));
		if (r_info.r_sw < desc->rsw_min)
			r_info.r_sw = desc->rsw_min;

		r_info.r_cable = precise_div(abs(r_info.vta - data->vbus_cali -
					     r_info.vbus) * 1000,
					     abs(r_info.ita));

		PE5P_INFO("r_sw:%d, r_bat:%d, r_cable:%d\n", r_info.r_sw,
			  r_info.r_bat, r_info.r_cable);

		if (i == 0) {
			memcpy(&max_r_info, &r_info,
			       sizeof(struct meas_r_info));
			memcpy(&min_r_info, &r_info,
			       sizeof(struct meas_r_info));
		} else {
			max_r_info.r_bat = max(max_r_info.r_bat, r_info.r_bat);
			max_r_info.r_sw = max(max_r_info.r_sw, r_info.r_sw);
			max_r_info.r_cable = max(max_r_info.r_cable,
						 r_info.r_cable);
			min_r_info.r_bat = min(min_r_info.r_bat, r_info.r_bat);
			min_r_info.r_sw = min(min_r_info.r_sw, r_info.r_sw);
			min_r_info.r_cable = min(min_r_info.r_cable,
						 r_info.r_cable);
		}
		data->r_bat += r_info.r_bat;
		data->r_sw += r_info.r_sw;
		data->r_cable += r_info.r_cable;
	}
	data->r_bat -= (max_r_info.r_bat + min_r_info.r_bat);
	data->r_sw -= (max_r_info.r_sw + min_r_info.r_sw);
	data->r_cable -= (max_r_info.r_cable + min_r_info.r_cable);
	data->r_bat = precise_div(data->r_bat, PE5P_MEASURE_R_AVG_TIMES);
	data->r_sw = precise_div(data->r_sw, PE5P_MEASURE_R_AVG_TIMES);
	data->r_cable = precise_div(data->r_cable,
				    PE5P_MEASURE_R_AVG_TIMES);
	data->r_total = data->r_bat + data->r_sw + data->r_cable;
	return 0;
stop:
	pe5p_stop(info, &_sinfo);
	return -EIO;
}

static int pe5p_select_ita_lmt_by_r(struct pe5p_algo_info *info, bool dual)
{
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;
	struct pe5p_ta_auth_data *auth_data = &data->ta_auth_data;
	u32 ita_lmt_by_r, ita_lmt;
	u32 *rcable_level = dual ? desc->rcable_level_dual : desc->rcable_level;
	u32 *ita_level = dual ? desc->ita_level_dual : desc->ita_level;

	if (!auth_data->support_meas_cap) {
		ita_lmt_by_r = ita_level[PE5P_RCABLE_NORMAL];
		goto out;
	}
	if (data->r_cable <= rcable_level[PE5P_RCABLE_NORMAL])
		ita_lmt_by_r = ita_level[PE5P_RCABLE_NORMAL];
	else if (data->r_cable <= rcable_level[PE5P_RCABLE_BAD1])
		ita_lmt_by_r = ita_level[PE5P_RCABLE_BAD1];
	else if (data->r_cable <= rcable_level[PE5P_RCABLE_BAD2])
		ita_lmt_by_r = ita_level[PE5P_RCABLE_BAD2];
	else if (data->r_cable <= rcable_level[PE5P_RCABLE_BAD3])
		ita_lmt_by_r = ita_level[PE5P_RCABLE_BAD3];
	else {
		PE5P_ERR("r_cable_by_swchg(%d) too worse\n", data->r_cable_by_swchg);
		PE5P_ERR("r_cable(%d) too worse\n", data->r_cable);
		return -EINVAL;
	}
	PE5P_ERR("r_cable_by_swchg: %d\n", data->r_cable_by_swchg);
	PE5P_ERR("r_cable: %d\n", data->r_cable);
out:
	PE5P_INFO("ita limited by r = %d\n", ita_lmt_by_r);
	data->ita_lmt = min_t(u32, ita_lmt_by_r, auth_data->ita_max);
	data->ita_pwr_lmt = pe5p_get_ita_pwr_lmt_by_vta(info,
							data->vta_setting);
	ita_lmt = pe5p_get_ita_lmt(info);
	if (ita_lmt < data->idvchg_term) {
		PE5P_ERR("ita_lmt(%d) < dvchg_term(%d)\n", ita_lmt,
			 data->idvchg_term);
		return -EINVAL;
	}
	return 0;
}

static int pe5p_algo_measure_r_with_ta_cc(struct pe5p_algo_info *info)
{
	int ret;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;
	u32 ita, idvchg_lmt;
	u32 rcable_retry_level = (data->is_dvchg_exist[PE5P_HVDVCHG_SLAVE] &&
				  !data->tried_dual_dvchg) ?
				  desc->rcable_level_dual[PE5P_RCABLE_NORMAL] :
				  desc->rcable_level[PE5P_RCABLE_NORMAL];
	struct pe5p_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	PE5P_DBG("++\n");

	ret = pe5p_algo_cal_r_info_with_ta_cap(info, &sinfo);
	if (ret < 0)
		goto err;
	if (data->r_cable > rcable_retry_level &&
	    data->err_retry_cnt < PE5P_MEASURE_R_RETRY_MAX) {
		PE5P_INFO("rcable(%d) is worse than normal\n", data->r_cable);
		goto err;
	}
	PE5P_ERR("avg_r(sw,bat,cable):(%d,%d,%d), r_total:%d\n",
		 data->r_sw, data->r_bat, data->r_cable, data->r_total);

	/* If haven't tried dual hvdvchg, try it once */
	if (data->is_dvchg_exist[PE5P_HVDVCHG_SLAVE] &&
	    !data->tried_dual_dvchg && !data->is_dvchg_en[PE5P_HVDVCHG_SLAVE]) {
		PE5P_INFO("try dual hvdvchg\n");
		data->tried_dual_dvchg = true;
		data->idvchg_term = 2 * desc->idvchg_term;
		data->idvchg_cc = desc->ita_level_dual[PE5P_RCABLE_NORMAL] -
				  desc->swchg_aicr;
		ret = pe5p_select_ita_lmt_by_r(info, true);
		if (ret < 0) {
			PE5P_ERR("select dual hvdvchg ita lmt fail(%d)\n", ret);
			goto single_dvchg_select_ita;
		}
		/* Turn on slave dvchg if idvchg_lmt >= 2 * idvchg_term */
		idvchg_lmt = pe5p_get_idvchg_lmt(info);
		if (idvchg_lmt < data->idvchg_term) {
			PE5P_ERR("idvchg_lmt(%d) < 2 * idvchg_term(%d)\n",
				 idvchg_lmt, data->idvchg_term);
			goto single_dvchg_select_ita;
		}
		ret = pe5p_enable_dvchg_charging(info, PE5P_HVDVCHG_MASTER,
						 false);
		if (ret < 0) {
			PE5P_ERR("disable master hvdvchg fail(%d)\n", ret);
			goto single_dvchg_restart;
		}
		data->ignore_ibusucpf = true;
		ret = pe5p_set_dvchg_protection(info, true);
		if (ret < 0) {
			PE5P_ERR("set dual hvdvchg protection fail(%d)\n", ret);
			goto single_dvchg_restart;
		}
		ret = pe5p_set_operating_mode(info->alg, HVDVCHG2, 1);
		if (ret)
			goto single_dvchg_restart;
		ret = pe5p_enable_dvchg_charging(info, PE5P_HVDVCHG_SLAVE,
						 true);
		if (ret < 0) {
			PE5P_ERR("en slave hvdvchg fail(%d)\n", ret);
			goto single_dvchg_restart;
		}
		ita = max(data->idvchg_term, data->ita_setting);
		ita = min(ita, idvchg_lmt);
		ret = pe5p_set_ta_cap_cc_by_cali_vta(info, ita);
		if (ret < 0) {
			PE5P_ERR("set ta cap fail(%d)\n", ret);
			sinfo.hardreset_ta = true;
			goto out;
		}
		ret = pe5p_set_operating_mode(info->alg, HVDVCHG1, 1);
		if (ret)
			goto single_dvchg_restart;
		ret = pe5p_enable_dvchg_charging(info, PE5P_HVDVCHG_MASTER,
						 true);
		if (ret < 0) {
			PE5P_ERR("en master hvdvchg fail(%d)\n", ret);
			goto single_dvchg_restart;
		}
		goto ss_dvchg;
single_dvchg_restart:
		ret = pe5p_earily_restart(info);
		if (ret < 0) {
			PE5P_ERR("earily restart fail(%d)\n", ret);
			goto out;
		}
		return 0;
	}
single_dvchg_select_ita:
	data->idvchg_term = desc->idvchg_term;
	data->idvchg_cc = desc->ita_level[PE5P_RCABLE_NORMAL] -
			  desc->swchg_aicr;
	ret = pe5p_select_ita_lmt_by_r(info, false);
	if (ret < 0) {
		PE5P_ERR("select dvchg ita lmt fail(%d)\n", ret);
		goto out;
	}
ss_dvchg:
	data->err_retry_cnt = 0;
	data->state = PE5P_ALGO_SS_DVCHG;
	return 0;
err:
	if (data->err_retry_cnt < PE5P_MEASURE_R_RETRY_MAX) {
		data->err_retry_cnt++;
		return 0;
	}
out:
	return pe5p_stop(info, &sinfo);
}

static int pe5p_algo_measure_r_with_ta_cv(struct pe5p_algo_info *info)
{
	int ret;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;
	struct pe5p_ta_auth_data *auth_data = &data->ta_auth_data;
	u32 rcable_retry_level = (data->is_dvchg_exist[PE5P_HVDVCHG_SLAVE] &&
				  !data->tried_dual_dvchg) ?
				  desc->rcable_level_dual[PE5P_RCABLE_NORMAL] :
				  desc->rcable_level[PE5P_RCABLE_NORMAL];
	struct pe5p_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	PE5P_DBG("++\n");

	/*
	 * Ignore measuring r,
	 * treat as normal cable if meas_cap is not supported
	 */
	if (!auth_data->support_meas_cap) {
		PE5P_INFO("ignore measuring resistance\n");
		goto select_ita;
	}
	ret = pe5p_algo_cal_r_info_with_ta_cap(info, &sinfo);
	if (ret < 0) {
		PE5P_ERR("get r info fail(%d)\n", ret);
		goto err;
	}
	if (data->r_cable > rcable_retry_level &&
	    data->err_retry_cnt < PE5P_MEASURE_R_RETRY_MAX) {
		PE5P_INFO("rcable(%d) is worse than normal\n", data->r_cable);
		goto err;
	}
	PE5P_ERR("avg_r(sw,bat,cable):(%d,%d,%d), r_total:%d\n",
		 data->r_sw, data->r_bat, data->r_cable, data->r_total);
select_ita:
	ret = pe5p_select_ita_lmt_by_r(info, false);
	if (ret < 0) {
		PE5P_ERR("select dvchg ita lmt fail(%d)\n", ret);
		goto out;
	}
	data->err_retry_cnt = 0;
	data->state = PE5P_ALGO_SS_DVCHG;
	return 0;
err:
	if (data->err_retry_cnt < PE5P_MEASURE_R_RETRY_MAX) {
		data->err_retry_cnt++;
		return 0;
	}
out:
	return pe5p_stop(info, &sinfo);
}

/* Measure resistance of cable/battery/sw and get corressponding ita limit */
static int pe5p_algo_measure_r(struct pe5p_algo_info *info)
{
	struct pe5p_algo_data *data = info->data;
	struct pe5p_ta_auth_data *auth_data = &data->ta_auth_data;

	return (auth_data->support_cc && !data->force_ta_cv) ?
	       pe5p_algo_measure_r_with_ta_cc(info) :
	       pe5p_algo_measure_r_with_ta_cv(info);
}

static int pe5p_check_slave_dvchg_off(struct pe5p_algo_info *info)
{
	int ret;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;

	data->idvchg_cc = desc->ita_level[PE5P_RCABLE_NORMAL] -
			  desc->swchg_aicr;
	data->idvchg_term = desc->idvchg_term;
	ret = pe5p_enable_dvchg_charging(info, PE5P_HVDVCHG_SLAVE, false);
	if (ret < 0) {
		PE5P_ERR("disable slave dvchg fail(%d)\n", ret);
		return ret;
	}
	ret = pe5p_select_ita_lmt_by_r(info, false);
	if (ret < 0) {
		PE5P_ERR("select dvchg ita lmt fail(%d)\n", ret);
		return ret;
	}
	ret = pe5p_set_dvchg_protection(info, false);
	if (ret < 0) {
		PE5P_ERR("dvchg protection fail(%d)\n", ret);
		return ret;
	}
	return 0;
}

static int pe5p_force_ta_cv(struct pe5p_algo_info *info,
			    struct pe5p_stop_info *sinfo)
{
	int ret;
	u32 ita, vta;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_ta_auth_data *auth_data = &data->ta_auth_data;

	PE5P_DBG("++\n");

	ret = pe5p_get_ta_cap(info);
	if (ret < 0) {
		PE5P_ERR("get ta cap fail\n");
		sinfo->hardreset_ta = true;
		return ret;
	}
	ita = min(data->ita_measure, data->ita_setting);
	vta = min_t(u32, data->vta_measure, auth_data->vcap_max);
	ret = pe5p_set_ta_cap_cv(info, vta, ita);
	if (ret < 0) {
		PE5P_ERR("set ta cap fail\n");
		return ret;
	}
	data->force_ta_cv = true;
	return 0;
}

static int pe5p_check_force_ta_cv(struct pe5p_algo_info *info,
				  struct pe5p_stop_info *sinfo)
{
	int ret;
	u32 vbat;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;

	ret = pe5p_get_adc(info, PE5P_ADCCHAN_VBAT, &vbat);
	if (ret < 0) {
		PE5P_ERR("get vbat fail(%d)\n", ret);
		return ret;
	}

	if (desc->force_ta_cv_vbat != 0 && vbat >= desc->force_ta_cv_vbat &&
	    !data->is_swchg_en) {
		ret = pe5p_force_ta_cv(info, sinfo);
		if (ret < 0) {
			PE5P_ERR("force ta cv fail(%d)\n", ret);
			return ret;
		}
	}
	return 0;
}

static int pe5p_algo_ss_dvchg_with_ta_cc(struct pe5p_algo_info *info)
{
	int ret, vbat;
	u32 ita, ita_lmt, idvchg_lmt;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;
	struct pe5p_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	PE5P_DBG("++\n");

	ret = pe5p_check_force_ta_cv(info, &sinfo);
	if (ret < 0) {
		PE5P_ERR("check force ta cv fail(%d)\n", ret);
		goto err;
	}
	if (data->force_ta_cv) {
		PE5P_INFO("force switching to ta cv mode\n");
		return 0;
	}

	ret = pe5p_get_ta_cap(info);
	if (ret < 0) {
		PE5P_ERR("get ta cap fail(%d)\n", ret);
		sinfo.hardreset_ta = true;
		goto err;
	}
	ret = pe5p_get_adc(info, PE5P_ADCCHAN_VBAT, &vbat);
	if (ret < 0) {
		PE5P_ERR("get vbat fail(%d)\n", ret);
		goto err;
	}

	idvchg_lmt = pe5p_get_idvchg_lmt(info);
	if (idvchg_lmt < data->idvchg_term) {
		PE5P_INFO("idvchg_lmt(%d) < idvchg_term(%d)\n", idvchg_lmt,
			  data->idvchg_term);
		goto err;
	}

	/* VBAT reaches CV level */
	if (vbat >= data->vbat_cv) {
		if (data->ita_measure < data->idvchg_term) {
			if (data->is_dvchg_en[PE5P_HVDVCHG_SLAVE]) {
				ret = pe5p_check_slave_dvchg_off(info);
				if (ret < 0) {
					PE5P_INFO("slave off fail(%d)\n", ret);
					goto err;
				}
				idvchg_lmt = pe5p_get_idvchg_lmt(info);
				goto cc_cv;
			}
			PE5P_INFO("finish PE5.0 charging, vbat(%d), ita(%d)\n",
				  vbat, data->ita_measure);
			goto err;
		}
cc_cv:
		ita = min(data->ita_setting - desc->idvchg_ss_step, idvchg_lmt);
		data->state = PE5P_ALGO_CC_CV;
		goto out_set_cap;
	}

	/* ITA reaches CC level */
	if (data->ita_measure >= idvchg_lmt ||
	    data->ita_setting >= idvchg_lmt) {
		/*
		 * Turn on switching charger only if divider charger
		 * is charging by it's maximum setting current
		 */
		ita_lmt = pe5p_get_ita_lmt(info);
		if (vbat < desc->swchg_off_vbat && desc->swchg_aicr > 0 &&
		    desc->swchg_ichg > 0 &&
		    (ita_lmt > data->idvchg_cc + desc->swchg_aicr_ss_init)) {
			ret = pe5p_set_swchg_cap(info,
						 desc->swchg_aicr_ss_init);
			if (ret < 0) {
				PE5P_ERR("set swchg cap fail(%d)\n", ret);
				goto err;
			}
			ret = pe5p_enable_swchg_charging(info, true);
			if (ret < 0) {
				PE5P_ERR("en swchg fail(%d)\n", ret);
				goto err;
			}
			ita = idvchg_lmt + desc->swchg_aicr_ss_init;
			data->aicr_init_lmt = ita_lmt - data->idvchg_cc;
			data->aicr_lmt = data->aicr_init_lmt;
			data->state = PE5P_ALGO_SS_SWCHG;
			goto out_set_cap;
		}
		data->state = PE5P_ALGO_CC_CV;
		ita = idvchg_lmt;
		goto out_set_cap;
	}

	/* Increase ita according to vbat level */
	if (vbat < desc->idvchg_ss_step1_vbat)
		ita = data->ita_setting + desc->idvchg_ss_step;
	else if (vbat < desc->idvchg_ss_step2_vbat)
		ita = data->ita_setting + desc->idvchg_ss_step1;
	else
		ita = data->ita_setting + desc->idvchg_ss_step2;
	ita = min(ita, idvchg_lmt);

out_set_cap:
	ret = pe5p_set_ta_cap_cc_by_cali_vta(info, ita);
	if (ret < 0) {
		PE5P_ERR("set ta cap fail(%d)\n", ret);
		sinfo.hardreset_ta = true;
		goto err;
	}
	return 0;
err:
	return pe5p_stop(info, &sinfo);
}

static int pe5p_algo_ss_dvchg_with_ta_cv(struct pe5p_algo_info *info)
{
	int ret, vbat;
	ktime_t start_time, end_time;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;
	struct pe5p_ta_auth_data *auth_data = &data->ta_auth_data;
	u32 idvchg_lmt, vta, ita, delta_time;
	u32 ita_gap_per_vstep = data->ita_gap_per_vstep > 0 ?
				data->ita_gap_per_vstep :
				auth_data->ita_gap_per_vstep;
	struct pe5p_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

repeat:
	PE5P_DBG("++\n");
	vta = data->vta_setting;
	start_time = ktime_get();
	ret = pe5p_get_adc(info, PE5P_ADCCHAN_VBAT, &vbat);
	if (ret < 0) {
		PE5P_ERR("get vbat fail(%d)\n", ret);
		goto out;
	}
	ret = pe5p_get_ta_cap_by_supportive(info, &data->vta_measure,
					    &data->ita_measure);
	if (ret < 0) {
		PE5P_ERR("get ta cap fail(%d)\n", ret);
		sinfo.hardreset_ta = auth_data->support_meas_cap;
		goto out;
	}

	/* Turn on slave dvchg if idvchg_lmt >= 2 * idvchg_term */
	ita = data->idvchg_term * 2;
	if (data->is_dvchg_exist[PE5P_HVDVCHG_SLAVE] &&
	    !data->tried_dual_dvchg && !data->is_dvchg_en[PE5P_HVDVCHG_SLAVE] &&
	    (data->ita_measure >= ita)) {
		PE5P_INFO("try dual hvdvchg\n");
		data->tried_dual_dvchg = true;
		data->idvchg_term = 2 * desc->idvchg_term;
		data->idvchg_cc = desc->ita_level_dual[PE5P_RCABLE_NORMAL] -
				  desc->swchg_aicr;
		ret = pe5p_select_ita_lmt_by_r(info, true);
		if (ret < 0) {
			PE5P_ERR("select dual hvdvchg ita lmt fail(%d)\n", ret);
			goto single_dvchg_select_ita;
		}
		ret = pe5p_enable_dvchg_charging(info, PE5P_HVDVCHG_MASTER,
						 false);
		if (ret < 0) {
			PE5P_ERR("disable master hvdvchg fail(%d)\n", ret);
			goto single_dvchg_restart;
		}
		data->ignore_ibusucpf = true;
		ret = pe5p_set_dvchg_protection(info, true);
		if (ret < 0) {
			PE5P_ERR("set dual hvdvchg protection fail(%d)\n", ret);
			goto single_dvchg_restart;
		}
		ret = pe5p_set_operating_mode(info->alg, HVDVCHG2, 1);
		if (ret)
			goto single_dvchg_restart;
		ret = pe5p_enable_dvchg_charging(info, PE5P_HVDVCHG_SLAVE,
						 true);
		if (ret < 0) {
			PE5P_ERR("en slave hvdvchg fail(%d)\n", ret);
			goto single_dvchg_restart;
		}
		ret = pe5p_set_operating_mode(info->alg, HVDVCHG1, 1);
		if (ret)
			goto single_dvchg_restart;
		ret = pe5p_enable_dvchg_charging(info, PE5P_HVDVCHG_MASTER,
						 true);
		if (ret < 0) {
			PE5P_ERR("en master hvdvchg fail(%d)\n", ret);
			goto single_dvchg_restart;
		}
		goto ss_dvchg;
single_dvchg_restart:
		ret = pe5p_earily_restart(info);
		if (ret < 0) {
			PE5P_ERR("earily restart fail(%d)\n", ret);
			goto out;
		}
		return 0;
single_dvchg_select_ita:
		data->idvchg_term = desc->idvchg_term;
		data->idvchg_cc = desc->ita_level[PE5P_RCABLE_NORMAL] -
				  desc->swchg_aicr;
		ret = pe5p_select_ita_lmt_by_r(info, false);
		if (ret < 0) {
			PE5P_ERR("select dvchg ita lmt fail(%d)\n", ret);
			goto out;
		}
	}

ss_dvchg:
	ita = data->ita_setting;
	idvchg_lmt = pe5p_get_idvchg_lmt(info);
	if (idvchg_lmt < data->idvchg_term) {
		PE5P_INFO("idvchg_lmt(%d) < idvchg_term(%d)\n", idvchg_lmt,
			 data->idvchg_term);
		goto out;
	}

	/* VBAT reaches CV level */
	if (vbat >= data->vbat_cv) {
		if (data->ita_measure < data->idvchg_term) {
			if (data->is_dvchg_en[PE5P_HVDVCHG_SLAVE]) {
				ret = pe5p_check_slave_dvchg_off(info);
				if (ret < 0) {
					PE5P_INFO("slave off fail(%d)\n", ret);
					goto out;
				}
				idvchg_lmt = pe5p_get_idvchg_lmt(info);
				goto cc_cv;
			}
			PE5P_INFO("finish PE5.0 charging, vbat(%d), ita(%d)\n",
				  vbat, data->ita_measure);
			goto out;
		}
cc_cv:
		vta -= auth_data->vta_step;
		ita -= ita_gap_per_vstep;
		data->state = PE5P_ALGO_CC_CV;
		goto out_set_cap;
	}

	/* IBUS reaches CC level */
	if (data->ita_measure + ita_gap_per_vstep > idvchg_lmt ||
	    vta == auth_data->vcap_max)
		data->state = PE5P_ALGO_CC_CV;
	else {
		vta += auth_data->vta_step;
		vta = min_t(u32, vta, auth_data->vcap_max);
		ita += ita_gap_per_vstep;
		ita = min(ita, idvchg_lmt);
	}

out_set_cap:
	ret = pe5p_set_ta_cap_cv(info, vta, ita);
	if (ret < 0) {
		PE5P_ERR("set ta cap fail(%d)\n", ret);
		sinfo.hardreset_ta = true;
		goto out;
	}
	if (data->state == PE5P_ALGO_SS_DVCHG) {
		end_time = ktime_get();
		delta_time = ktime_ms_delta(end_time, start_time);
		PE5P_DBG("delta time %dms\n", delta_time);
		if (delta_time < desc->ta_cv_ss_repeat_tmin)
			msleep(desc->ta_cv_ss_repeat_tmin - delta_time);
		goto repeat;
	}
	return 0;
out:
	return pe5p_stop(info, &sinfo);
}

/* Soft start of divider charger */
static int pe5p_algo_ss_dvchg(struct pe5p_algo_info *info)
{
	struct pe5p_algo_data *data = info->data;
	struct pe5p_ta_auth_data *auth_data = &data->ta_auth_data;

	return (auth_data->support_cc && !data->force_ta_cv) ?
		pe5p_algo_ss_dvchg_with_ta_cc(info) :
		pe5p_algo_ss_dvchg_with_ta_cv(info);
}

static int pe5p_check_swchg_off(struct pe5p_algo_info *info,
				struct pe5p_stop_info *sinfo)
{
	int ret;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;
	u32 aicr = data->aicr_setting, ita, vbat;

	ret = pe5p_get_adc(info, PE5P_ADCCHAN_VBAT, &vbat);
	if (ret < 0) {
		PE5P_ERR("get vbat fail(%d)\n", ret);
		return ret;
	}
	if (!data->is_swchg_en || (vbat < desc->swchg_off_vbat &&
	    desc->tswchg_curlmt[data->tswchg_level] <= 0 &&
	    aicr <= data->aicr_lmt))
		return 0;
	PE5P_INFO("vbat(%d),swchg_off_vbat(%d),aicr_lmt(%d)\n", vbat,
		 desc->swchg_off_vbat, data->aicr_lmt);
	/* Calculate AICR */
	if (vbat >= desc->swchg_off_vbat)
		aicr -= desc->swchg_aicr_ss_step;
	aicr = min(aicr, data->aicr_lmt);
	/* Calculate ITA */
	if (aicr >= desc->swchg_aicr_ss_init)
		ita = data->ita_setting - (data->aicr_setting - aicr);
	else
		ita = data->ita_setting - data->aicr_setting;
	ret = pe5p_set_ta_cap_cc_by_cali_vta(info, ita);
	if (ret < 0) {
		PE5P_ERR("set ta cap fail(%d)\n", ret);
		sinfo->hardreset_ta = true;
		return ret;
	}
	return (aicr >= desc->swchg_aicr_ss_init) ?
		pe5p_set_swchg_cap(info, aicr) :
		pe5p_enable_swchg_charging(info, false);
}

static int pe5p_update_aicr_lmt(struct pe5p_algo_info *info)
{
	struct pe5p_algo_desc *desc = info->desc;
	struct pe5p_algo_data *data = info->data;
	u32 ita_lmt;

	if (!data->is_swchg_en)
		return 0;
	ita_lmt = pe5p_get_ita_lmt(info);
	/* Turn off swchg and use dvchg only */
	if (ita_lmt < data->idvchg_cc + desc->swchg_aicr_ss_init) {
		data->aicr_lmt = 0;
		return -EINVAL;
	}
	data->aicr_lmt = min(data->aicr_lmt, ita_lmt - data->idvchg_cc);
	if (data->aicr_lmt < desc->swchg_aicr_ss_init) {
		data->aicr_lmt = 0;
		return -EINVAL;
	}
	return 0;
}

static int pe5p_algo_ss_swchg(struct pe5p_algo_info *info)
{
	int ret;
	struct pe5p_algo_desc *desc = info->desc;
	struct pe5p_algo_data *data = info->data;
	u32 ita = data->ita_setting, aicr = 0, vbat;
	struct pe5p_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	PE5P_DBG("++\n");

	if (pe5p_update_aicr_lmt(info) < 0)
		goto out;
	/* Set new AICR & TA cap */
	aicr = min(data->aicr_setting + desc->swchg_aicr_ss_step,
		   data->aicr_lmt);
	ita += (aicr - data->aicr_setting);
	ret = pe5p_set_swchg_cap(info, aicr);
	if (ret < 0) {
		PE5P_ERR("set swchg cap fail(%d)\n", ret);
		goto err;
	}
	ret = pe5p_set_ta_cap_cc_by_cali_vta(info, ita);
	if (ret < 0) {
		PE5P_ERR("set ta cap fail(%d)\n", ret);
		sinfo.hardreset_ta = true;
		goto err;
	}
out:
	ret = pe5p_get_adc(info, PE5P_ADCCHAN_VBAT, &vbat);
	if (ret < 0) {
		PE5P_ERR("get vbat fail(%d)\n", ret);
		goto err;
	}
	ret = pe5p_check_swchg_off(info, &sinfo);
	if (ret < 0) {
		PE5P_ERR("check swchg off fail(%d)\n", ret);
		goto err;
	}
	if (!data->is_swchg_en || vbat >= desc->swchg_off_vbat ||
	    aicr >= data->aicr_lmt)
		data->state = PE5P_ALGO_CC_CV;
	return 0;
err:
	return pe5p_stop(info, &sinfo);
}

static int pe5p_algo_cc_cv_with_ta_cc(struct pe5p_algo_info *info)
{
	int ret, vbat = 0;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;
	u32 ita = data->ita_setting, ita_lmt;
	struct pe5p_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	PE5P_DBG("++\n");

	ret = pe5p_check_force_ta_cv(info, &sinfo);
	if (ret < 0) {
		PE5P_ERR("check force ta cv fail(%d)\n", ret);
		goto err;
	}
	if (data->force_ta_cv) {
		PE5P_INFO("force switching to ta cv mode\n");
		return 0;
	}
	/* Check swchg */
	pe5p_update_aicr_lmt(info);
	ret = pe5p_check_swchg_off(info, &sinfo);
	if (ret < 0) {
		PE5P_ERR("check swchg off fail(%d)\n", ret);
		goto err;
	}

	ret = pe5p_get_adc(info, PE5P_ADCCHAN_VBAT, &vbat);
	if (ret < 0)
		PE5P_ERR("get vbat fail(%d)\n", ret);

	ret = pe5p_get_ta_cap(info);
	if (ret < 0) {
		PE5P_ERR("get ta cap fail(%d)\n", ret);
		sinfo.hardreset_ta = true;
		goto err;
	}

	if (data->ita_measure < data->idvchg_term) {
		if (data->is_dvchg_en[PE5P_HVDVCHG_SLAVE]) {
			ret = pe5p_check_slave_dvchg_off(info);
			if (ret < 0) {
				PE5P_INFO("slave off fail(%d)\n", ret);
				goto err;
			}
			goto cc_cv;
		}
		PE5P_INFO("finish PE5.0 charging\n");
		goto err;
	}
cc_cv:
	ita_lmt = pe5p_get_ita_lmt(info);
	/* Consider AICR is decreased */
	ita_lmt = min(ita_lmt, data->is_swchg_en ?
		      (data->idvchg_cc + data->aicr_setting) : data->idvchg_cc);
	if (ita_lmt < data->idvchg_term) {
		PE5P_INFO("ita_lmt(%d) < idvchg_term(%d)\n", ita_lmt,
			 data->idvchg_term);
		goto err;
	}

	if (vbat >= data->vbat_cv) {
		ita = data->ita_setting - desc->idvchg_step;
		data->is_vbat_over_cv = true;
	} else if (vbat < desc->idvchg_ss_step1_vbat && ita < ita_lmt) {
		PE5P_INFO("++ita(set,lmt,add)=(%d,%d,%d)\n", ita, ita_lmt,
			  desc->idvchg_ss_step);
		ita = data->ita_setting + desc->idvchg_ss_step;
	} else if (vbat < desc->idvchg_ss_step2_vbat && ita < ita_lmt) {
		PE5P_INFO("++ita(set,lmt,add)=(%d,%d,%d)\n", ita, ita_lmt,
			  desc->idvchg_ss_step1);
		ita = data->ita_setting + desc->idvchg_ss_step1;
	} else if (!data->is_vbat_over_cv &&
		   vbat <= data->cv_lower_bound && ita < ita_lmt) {
		PE5P_INFO("++ita(set,lmt,add)=(%d,%d,%d)\n", ita, ita_lmt,
			  desc->idvchg_step);
		ita = data->ita_setting + desc->idvchg_step;
	} else if (data->is_vbat_over_cv)
		data->is_vbat_over_cv = false;

	ita = min(ita, ita_lmt);
	ret = pe5p_set_ta_cap_cc_by_cali_vta(info, ita);
	if (ret < 0) {
		PE5P_ERR("set_ta_cap fail(%d)\n", ret);
		sinfo.hardreset_ta = true;
		goto err;
	}
	return 0;
err:
	return pe5p_stop(info, &sinfo);
}

static int pe5p_algo_cc_cv_with_ta_cv(struct pe5p_algo_info *info)
{
	int ret, vbat, vsys = 0;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_ta_auth_data *auth_data = &data->ta_auth_data;
	u32 idvchg_lmt, vta = data->vta_setting, ita = data->ita_setting;
	u32 ita_gap_per_vstep = data->ita_gap_per_vstep > 0 ?
				data->ita_gap_per_vstep :
				auth_data->ita_gap_per_vstep;
	u32 vta_measure, ita_measure, suspect_ta_cc = false;
	struct pe5p_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	PE5P_DBG("++\n");

	ret = pe5p_get_adc(info, PE5P_ADCCHAN_VBAT, &vbat);
	if (ret < 0) {
		PE5P_ERR("get vbat fail(%d)\n", ret);
		goto out;
	}

	ret = pe5p_hal_get_adc(info->alg, CHG1, PE5P_ADCCHAN_VSYS,
				   &vsys);
	if (ret < 0) {
		PE5P_ERR("get vsys fail(%d)\n", ret);
		goto out;
	}

	ret = pe5p_get_ta_cap_by_supportive(info, &data->vta_measure,
					    &data->ita_measure);
	if (ret < 0) {
		PE5P_ERR("get ta cap fail(%d)\n", ret);
		sinfo.hardreset_ta = auth_data->support_meas_cap;
		goto out;
	}
	if (data->ita_measure <= data->idvchg_term) {
		if (data->is_dvchg_en[PE5P_HVDVCHG_SLAVE]) {
			ret = pe5p_check_slave_dvchg_off(info);
			if (ret < 0) {
				PE5P_INFO("slave off fail(%d)\n", ret);
				goto out;
			}
			goto cc_cv;
		}
		PE5P_INFO("finish PE5.0 charging\n");
		goto out;
	}
cc_cv:
	idvchg_lmt = pe5p_get_idvchg_lmt(info);
	if (idvchg_lmt < data->idvchg_term) {
		PE5P_INFO("idvchg_lmt(%d) < idvchg_term(%d)\n", idvchg_lmt,
			  data->idvchg_term);
		goto out;
	}

	if (vbat >= data->vbat_cv) {
		PE5P_INFO("--vbat >= vbat_cv, %d > %d\n", vbat, data->vbat_cv);
		vta -= auth_data->vta_step;
		ita -= ita_gap_per_vstep;
		data->is_vbat_over_cv = true;
	} else if (data->ita_measure > idvchg_lmt  ||
		   vsys >= PE5P_VSYS_UPPER_BOUND) {
		vta -= auth_data->vta_step;
		ita -= ita_gap_per_vstep;
		ita = max(ita, idvchg_lmt);
		PE5P_INFO("--vta, ita(meas,lmt)=(%d,%d)\n", data->ita_measure,
			  idvchg_lmt);
	} else if (!data->is_vbat_over_cv && vbat <= data->cv_lower_bound &&
		   data->ita_measure <= (idvchg_lmt - ita_gap_per_vstep) &&
		   vta < auth_data->vcap_max && !data->suspect_ta_cc &&
		   vsys < (PE5P_VSYS_UPPER_BOUND - PE5P_VSYS_UPPER_BOUND_GAP)) {
		vta += auth_data->vta_step;
		vta = min_t(u32, vta, auth_data->vcap_max);
		ita += ita_gap_per_vstep;
		ita = min(ita, idvchg_lmt);
		if (ita == data->ita_setting)
			suspect_ta_cc = true;
		PE5P_INFO("++vta, ita(meas,lmt)=(%d,%d)\n", data->ita_measure,
			  idvchg_lmt);
	} else if (data->is_vbat_over_cv)
		data->is_vbat_over_cv = false;

	ret = pe5p_set_ta_cap_cv(info, vta, ita);
	if (ret < 0) {
		PE5P_ERR("set_ta_cap fail(%d)\n", ret);
		sinfo.hardreset_ta = true;
		goto out;
	}
	ret = pe5p_get_ta_cap_by_supportive(info, &vta_measure, &ita_measure);
	if (ret < 0) {
		PE5P_ERR("get ta cap fail(%d)\n", ret);
		sinfo.hardreset_ta = auth_data->support_meas_cap;
		goto out;
	}
	data->suspect_ta_cc = (suspect_ta_cc &&
			       data->ita_measure == ita_measure);
	return 0;
out:
	return pe5p_stop(info, &sinfo);
}

static int pe5p_algo_cc_cv(struct pe5p_algo_info *info)
{
	struct pe5p_algo_data *data = info->data;
	struct pe5p_ta_auth_data *auth_data = &data->ta_auth_data;

	return (auth_data->support_cc && !data->force_ta_cv) ?
		pe5p_algo_cc_cv_with_ta_cc(info) :
		pe5p_algo_cc_cv_with_ta_cv(info);
}

/*
 * Check TA's status
 * Get status from TA and check temperature, OCP, OTP, and OVP, etc...
 *
 * return true if TA is normal and false if it is abnormal
 */
static bool pe5p_check_ta_status(struct pe5p_algo_info *info,
				 struct pe5p_stop_info *sinfo)
{
	int ret;
	struct pe5p_ta_status status;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;
	struct pe5p_ta_auth_data *auth_data = &data->ta_auth_data;

	if (!auth_data->support_status)
		return desc->allow_not_check_ta_status;
	ret = pe5p_hal_get_ta_status(info->alg, &status);
	if (ret < 0) {
		PE5P_ERR("get ta status fail(%d)\n", ret);
		goto err;
	}

	PE5P_INFO("temp = %d, (OVP,OCP,OTP) = (%d,%d,%d)\n",
		  status.temperature, status.ovp, status.ocp, status.otp);
	if (status.ocp || status.otp || status.ovp)
		goto err;
	return true;
err:
	sinfo->hardreset_ta = true;
	return false;
}

static bool pe5p_check_dvchg_ibusocp(struct pe5p_algo_info *info,
				     struct pe5p_stop_info *sinfo)
{
	int ret, ibus, acc = 0;
	struct pe5p_algo_data *data = info->data;
	u32 ibusocp;

	if (!data->is_dvchg_en[PE5P_HVDVCHG_MASTER])
		return true;
	ibusocp = pe5p_get_dvchg_ibusocp(info, data->ita_setting);
	pe5p_hal_get_adc_accuracy(info->alg, to_chgidx(PE5P_BUCK_BSTCHG),
				  PE5P_ADCCHAN_IBUS, &acc);
	ret = pe5p_hal_get_adc(info->alg, to_chgidx(PE5P_BUCK_BSTCHG),
			       PE5P_ADCCHAN_IBUS, &ibus);
	if (ret < 0) {
		PE5P_ERR("get ibus fail(%d)\n", ret);
		return false;
	}
	PE5P_INFO("(%s)ibus(%d+-%dmA), ibusocp(%dmA)\n",
		 pe5p_dvchg_role_name[PE5P_BUCK_BSTCHG], ibus, acc, ibusocp);
	if (ibus > acc)
		ibus -= acc;
	// if (ibus > ibusocp) {
	if (ibus > ibusocp + 150) {
		// PE5P_ERR("(%s)ibus(%dmA) > ibusocp(%dmA)\n",
		PE5P_ERR("(%s)ibus(%dmA) > ibusocp(%dmA) + 150\n",
			 pe5p_dvchg_role_name[PE5P_BUCK_BSTCHG], ibus, ibusocp);
		return false;
	}
	return true;
}

static bool pe5p_check_ta_ibusocp(struct pe5p_algo_info *info,
				  struct pe5p_stop_info *sinfo)
{
	int ret;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_ta_auth_data *auth_data = &data->ta_auth_data;
	u32 itaocp;

	if (!auth_data->support_meas_cap)
		return true;
	ret = pe5p_get_ta_cap(info);
	if (ret < 0) {
		PE5P_ERR("get ta cap fail(%d)\n", ret);
		goto err;
	}
	itaocp = pe5p_get_itaocp(info);
	PE5P_INFO("ita(%dmA), itaocp(%dmA)\n", data->ita_measure, itaocp);
	if (data->ita_measure > itaocp) {
		PE5P_ERR("ita(%dmA) > itaocp(%dmA)\n", data->ita_measure,
			 itaocp);
		/* double confirm using dvchg */
		if (!pe5p_check_dvchg_ibusocp(info, sinfo))
			goto err;
	}
	return true;

err:
	sinfo->hardreset_ta = true;
	return false;
}

/*
 * Check VBUS voltage of divider charger
 * return false if VBUS is over voltage otherwise return true
 */
static bool pe5p_check_dvchg_vbusovp(struct pe5p_algo_info *info,
				     struct pe5p_stop_info *sinfo)
{
	int ret, vbus;
	struct pe5p_algo_data *data = info->data;
	u32 vbusovp;

	vbusovp = pe5p_get_dvchg_vbusovp(info, data->ita_setting);
	ret = pe5p_get_adc(info, PE5P_ADCCHAN_VBUS, &vbus);
	if (ret < 0) {
		PE5P_ERR("get vbus fail(%d)\n", ret);
		return false;
	}
	PE5P_INFO("vbus(%dmV), vbusovp(%dmV)\n", vbus, vbusovp);
	if (vbus > vbusovp) {
		PE5P_ERR("vbus(%dmV) > vbusovp(%dmV)\n", vbus, vbusovp);
		return false;
	}

	return true;
}

static bool pe5p_check_vbatovp(struct pe5p_algo_info *info,
			       struct pe5p_stop_info *sinfo)
{
	int ret, vbat;
	u32 vbatovp;

	vbatovp = pe5p_get_vbatovp(info);
	ret = pe5p_get_adc(info, PE5P_ADCCHAN_VBAT, &vbat);
	if (ret < 0) {
		PE5P_ERR("get vbat fail(%d)\n", ret);
		return false;
	}
	PE5P_INFO("vbat(%dmV), vbatovp(%dmV)\n", vbat, vbatovp);
	if (vbat > vbatovp) {
		PE5P_ERR("vbat(%dmV) > vbatovp(%dmV)\n", vbat, vbatovp);
		return false;
	}
	return true;
}

static bool pe5p_check_ibatocp(struct pe5p_algo_info *info,
			       struct pe5p_stop_info *sinfo)
{
	int ret, ibat;
	struct pe5p_algo_data *data = info->data;
	u32 ibatocp;

	if (!data->is_dvchg_en[PE5P_HVDVCHG_MASTER])
		return true;
	ibatocp = pe5p_get_ibatocp(info, data->ita_setting);
	ret = pe5p_hal_get_adc(info->alg, CHG1,
			       PE5P_ADCCHAN_IBUS, &ibat);
	if (ret < 0) {
		PE5P_ERR("get ibat fail(%d)\n", ret);
		return false;
	}
	PE5P_INFO("ibat(%dmA), ibatocp(%dmA)\n", ibat, ibatocp);
	if (ibat > ibatocp) {
		PE5P_ERR("ibat(%dmA) > ibatocp(%dmA)\n", ibat, ibatocp);
		return false;
	}
	return true;
}

struct pe5p_thermal_data {
	const char *name;
	int temp;
	enum pe5p_thermal_level *temp_level;
	int *temp_level_def;
	int *curlmt;
	int recovery_area;
};

static bool pe5p_check_thermal_level(struct pe5p_algo_info *info,
				     struct pe5p_thermal_data *tdata)
{
	if (tdata->temp >= tdata->temp_level_def[PE5P_THERMAL_VERY_HOT]) {
		if (tdata->curlmt[PE5P_THERMAL_VERY_HOT] == 0)
			return true;
		PE5P_ERR("%s(%d) is over max(%d)\n", tdata->name, tdata->temp,
			tdata->temp_level_def[PE5P_THERMAL_VERY_HOT]);
		return false;
	}
	if (tdata->temp <= tdata->temp_level_def[PE5P_THERMAL_VERY_COLD]) {
		if (tdata->curlmt[PE5P_THERMAL_VERY_COLD] == 0)
			return true;
		PE5P_ERR("%s(%d) is under min(%d)\n", tdata->name, tdata->temp,
			tdata->temp_level_def[PE5P_THERMAL_VERY_COLD]);
		return false;
	}
	switch (*tdata->temp_level) {
	case PE5P_THERMAL_COLD:
		if (tdata->temp >= (tdata->temp_level_def[PE5P_THERMAL_COLD] +
				    tdata->recovery_area))
			*tdata->temp_level = PE5P_THERMAL_VERY_COOL;
		break;
	case PE5P_THERMAL_VERY_COOL:
		if (tdata->temp >=
		    (tdata->temp_level_def[PE5P_THERMAL_VERY_COOL] +
		     tdata->recovery_area))
			*tdata->temp_level = PE5P_THERMAL_COOL;
		else if (tdata->temp <=
			 tdata->temp_level_def[PE5P_THERMAL_COLD] &&
			 tdata->curlmt[PE5P_THERMAL_COLD] > 0)
			*tdata->temp_level = PE5P_THERMAL_COLD;
		break;
	case PE5P_THERMAL_COOL:
		if (tdata->temp >= (tdata->temp_level_def[PE5P_THERMAL_COOL] +
				    tdata->recovery_area))
			*tdata->temp_level = PE5P_THERMAL_NORMAL;
		else if (tdata->temp <=
			 tdata->temp_level_def[PE5P_THERMAL_VERY_COOL] &&
			 tdata->curlmt[PE5P_THERMAL_VERY_COOL] > 0)
			*tdata->temp_level = PE5P_THERMAL_VERY_COOL;
		break;
	case PE5P_THERMAL_NORMAL:
		if (tdata->temp >= tdata->temp_level_def[PE5P_THERMAL_WARM] &&
		    tdata->curlmt[PE5P_THERMAL_WARM] > 0)
			*tdata->temp_level = PE5P_THERMAL_WARM;
		else if (tdata->temp <=
			 tdata->temp_level_def[PE5P_THERMAL_COOL] &&
			 tdata->curlmt[PE5P_THERMAL_COOL] > 0)
			*tdata->temp_level = PE5P_THERMAL_COOL;
		break;
	case PE5P_THERMAL_WARM:
		if (tdata->temp <= (tdata->temp_level_def[PE5P_THERMAL_WARM] -
				    tdata->recovery_area))
			*tdata->temp_level = PE5P_THERMAL_NORMAL;
		else if (tdata->temp >=
			 tdata->temp_level_def[PE5P_THERMAL_VERY_WARM] &&
			 tdata->curlmt[PE5P_THERMAL_VERY_WARM] > 0)
			*tdata->temp_level = PE5P_THERMAL_VERY_WARM;
		break;
	case PE5P_THERMAL_VERY_WARM:
		if (tdata->temp <=
		    (tdata->temp_level_def[PE5P_THERMAL_VERY_WARM] -
		     tdata->recovery_area))
			*tdata->temp_level = PE5P_THERMAL_WARM;
		else if (tdata->temp >=
			 tdata->temp_level_def[PE5P_THERMAL_HOT] &&
			 tdata->curlmt[PE5P_THERMAL_HOT] > 0)
			*tdata->temp_level = PE5P_THERMAL_HOT;
		break;
	case PE5P_THERMAL_HOT:
		if (tdata->temp <= (tdata->temp_level_def[PE5P_THERMAL_HOT] -
				    tdata->recovery_area))
			*tdata->temp_level = PE5P_THERMAL_VERY_WARM;
		break;
	default:
		PE5P_ERR("NO SUCH STATE\n");
		return false;
	}
	PE5P_INFO("%s(%d,%d)\n", tdata->name, tdata->temp, *tdata->temp_level);
	return true;
}

/*
 * Check and adjust battery's temperature level
 * return false if battery's temperature is over maximum or under minimum
 * otherwise return true
 */
static bool pe5p_check_tbat_level(struct pe5p_algo_info *info,
				  struct pe5p_stop_info *sinfo)
{
	int ret, tbat;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;
	struct pe5p_thermal_data tdata = {
		.name = "tbat",
		.temp_level_def = desc->tbat_level_def,
		.curlmt = desc->tbat_curlmt,
		.temp_level = &data->tbat_level,
		.recovery_area = desc->tbat_recovery_area,
	};

	ret = pe5p_get_adc(info, PE5P_ADCCHAN_TBAT, &tbat);
	if (ret < 0) {
		PE5P_ERR("get tbat fail(%d)\n", ret);
		return false;
	}
	tdata.temp = tbat;
	return pe5p_check_thermal_level(info, &tdata);
}

/*
 * Check and adjust TA's temperature level
 * return false if TA's temperature is over maximum
 * otherwise return true
 */
static bool pe5p_check_tta_level(struct pe5p_algo_info *info,
				 struct pe5p_stop_info *sinfo)
{
	int ret;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;
	struct pe5p_ta_auth_data *auth_data = &data->ta_auth_data;
	struct pe5p_ta_status status;
	struct pe5p_thermal_data tdata = {
		.name = "tta",
		.temp_level_def = desc->tta_level_def,
		.curlmt = desc->tta_curlmt,
		.temp_level = &data->tta_level,
		.recovery_area = desc->tta_recovery_area,
	};

	if (!auth_data->support_status)
		return desc->allow_not_check_ta_status;

	ret = pe5p_hal_get_ta_status(info->alg, &status);
	if (ret < 0) {
		PE5P_ERR("get tta fail(%d)\n", ret);
		sinfo->hardreset_ta = true;
		return false;
	}

	tdata.temp = status.temperature;
	return pe5p_check_thermal_level(info, &tdata);
}

/*
 * Check and adjust switching charger's temperature level
 * return false if switching charger's temperature is over maximum
 * otherwise return true
 */
static bool pe5p_check_tswchg_level(struct pe5p_algo_info *info,
				    struct pe5p_stop_info *sinfo)
{
	int ret, tswchg;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;
	struct pe5p_thermal_data tdata = {
		.name = "tswchg",
		.temp_level_def = desc->tswchg_level_def,
		.curlmt = desc->tswchg_curlmt,
		.temp_level = &data->tswchg_level,
		.recovery_area = desc->tswchg_recovery_area,
	};

	if (!data->is_swchg_en)
		return true;

	ret = pe5p_hal_get_adc(info->alg, CHG1,
			       PE5P_ADCCHAN_TCHG, &tswchg);
	if (ret < 0) {
		PE5P_ERR("get tswchg fail(%d)\n", ret);
		return false;
	}
	tdata.temp = tswchg;
	if (!pe5p_check_thermal_level(info, &tdata))
		return false;
	data->aicr_lmt = min(data->aicr_lmt, data->aicr_init_lmt -
			     desc->tswchg_curlmt[data->tswchg_level]);
	return true;
}

static bool
(*pe5p_safety_check_fn[])(struct pe5p_algo_info *info,
			  struct pe5p_stop_info *sinfo) = {
	pe5p_check_ta_status,
	pe5p_check_ta_ibusocp,
	pe5p_check_dvchg_vbusovp,
	pe5p_check_vbatovp,
	pe5p_check_ibatocp,
	pe5p_check_tbat_level,
	pe5p_check_tta_level,
	pe5p_check_tswchg_level,
};

static bool pe5p_algo_safety_check(struct pe5p_algo_info *info)
{
	unsigned int i;
	struct pe5p_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	PE5P_DBG("++\n");
	for (i = 0; i < ARRAY_SIZE(pe5p_safety_check_fn); i++) {
		if (!pe5p_safety_check_fn[i](info, &sinfo))
			goto err;
	}
	return true;

err:
	pe5p_stop(info, &sinfo);
	return false;
}

static bool pe5p_is_ta_rdy(struct pe5p_algo_info *info)
{
	int ret;
	struct pe5p_algo_desc *desc = info->desc;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_ta_auth_data *auth_data = &data->ta_auth_data;

	if (!data->ta_ready) {
		auth_data->vcap_min = desc->vta_cap_min;
		auth_data->vcap_max = desc->vta_cap_max;
		auth_data->icap_min = desc->ita_cap_min;

		ret = pe5p_hal_authenticate_ta(info->alg, auth_data);
		if (ret < 0)
			return false;
		if (auth_data->vcap_max > 19000)
			data->ta_ready = true;
		else
			data->ta_ready = false;

		PE5P_ERR("[%s - line:%d]vcap_min:%d, vcap_max:%d, icap_min:%d, ta_ready:%d\n",
			__func__, __LINE__, auth_data->vcap_min, auth_data->vcap_max,
			auth_data->icap_min, data->ta_ready);
		return data->ta_ready;
	}
	return true;
}

static inline void pe5p_wakeup_algo_thread(struct pe5p_algo_data *data)
{
	PE5P_DBG("++\n");
	atomic_set(&data->wakeup_thread, 1);
	wake_up_interruptible(&data->wq);
}

static enum alarmtimer_restart
pe5p_algo_timer_cb(struct alarm *alarm, ktime_t now)
{
	struct pe5p_algo_data *data =
		container_of(alarm, struct pe5p_algo_data, timer);

	PE5P_DBG("++\n");
	pe5p_wakeup_algo_thread(data);
	return ALARMTIMER_NORESTART;
}

/*
 * Check charging time of pe5.0 algorithm
 * return false if timeout otherwise return true
 */
static bool pe5p_algo_check_charging_time(struct pe5p_algo_info *info)
{
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;
	ktime_t etime, time_diff;
	struct timespec64 dtime;
	struct pe5p_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	etime = ktime_get_boottime();
	time_diff = ktime_sub(etime, data->stime);
	dtime = ktime_to_timespec64(time_diff);
	if (dtime.tv_sec >= desc->chg_time_max) {
		PE5P_ERR("PE5.0 algo timeout(%d, %d)\n", (int)dtime.tv_sec,
			 desc->chg_time_max);
		pe5p_stop(info, &sinfo);
		return false;
	}
	return true;
}

static inline int __pe5p_plugout_reset(struct pe5p_algo_info *info,
				       struct pe5p_stop_info *sinfo)
{
	struct pe5p_algo_data *data = info->data;

	PE5P_DBG("++\n");
	data->ta_ready = false;
	data->run_once = false;
	return pe5p_stop(info, sinfo);
}

static int pe5p_notify_hardreset_hdlr(struct pe5p_algo_info *info)
{
	struct pe5p_stop_info sinfo = {
		.reset_ta = false,
		.hardreset_ta = false,
	};

	PE5P_INFO("++\n");
	return __pe5p_plugout_reset(info, &sinfo);
}

static int pe5p_notify_detach_hdlr(struct pe5p_algo_info *info)
{
	struct pe5p_stop_info sinfo = {
		.reset_ta = false,
		.hardreset_ta = false,
	};

	PE5P_INFO("++\n");
	return __pe5p_plugout_reset(info, &sinfo);
}

static int pe5p_notify_hwerr_hdlr(struct pe5p_algo_info *info)
{
	struct pe5p_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	PE5P_INFO("++\n");
	return pe5p_stop(info, &sinfo);
}

static int pe5p_notify_ibusucpf_hdlr(struct pe5p_algo_info *info)
{
	int ret, ibus;
	struct pe5p_algo_data *data = info->data;

	if (data->ignore_ibusucpf) {
		PE5P_INFO("ignore ibusucpf\n");
		data->ignore_ibusucpf = false;
		return 0;
	}
	if (!data->is_dvchg_en[PE5P_HVDVCHG_MASTER]) {
		PE5P_INFO("master dvchg is off\n");
		return 0;
	}
	/* Last chance */
	ret = pe5p_hal_get_adc(info->alg, DVCHG1, PE5P_ADCCHAN_IBUS, &ibus);
	if (ret < 0) {
		PE5P_ERR("get dvchg ibus fail(%d)\n", ret);
		goto out;
	}
	if (ibus < PE5P_IBUSUCPF_RECHECK) {
		PE5P_ERR("ibus(%d) < recheck(%d)\n", ibus,
			 PE5P_IBUSUCPF_RECHECK);
		goto out;
	}
	PE5P_INFO("recheck ibus and it is not ucp\n");
	return 0;
out:
	return pe5p_notify_hwerr_hdlr(info);
}

static int pe5p_notify_vbatovp_alarm_hdlr(struct pe5p_algo_info *info)
{
	int ret;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	if (data->state == PE5P_ALGO_STOP)
		return 0;
	PE5P_INFO("++\n");
	ret = pe5p_hal_reset_vbatovp_alarm(info->alg, DVCHG1);
	if (ret < 0) {
		PE5P_ERR("reset vbatovp alarm fail(%d)\n", ret);
		return pe5p_stop(info, &sinfo);
	}
	return 0;
}

static int pe5p_notify_vbusovp_alarm_hdlr(struct pe5p_algo_info *info)
{
	int ret;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	if (data->state == PE5P_ALGO_STOP)
		return 0;
	PE5P_INFO("++\n");
	ret = pe5p_hal_reset_vbusovp_alarm(info->alg, DVCHG1);
	if (ret < 0) {
		PE5P_ERR("reset vbusovp alarm fail(%d)\n", ret);
		return pe5p_stop(info, &sinfo);
	}
	return 0;
}

static int
(*pe5p_notify_pre_hdlr[EVT_MAX])(struct pe5p_algo_info *info) = {
	[EVT_DETACH] = pe5p_notify_detach_hdlr,
	[EVT_HARDRESET] = pe5p_notify_hardreset_hdlr,
	[EVT_VBUSOVP] = pe5p_notify_hwerr_hdlr,
	[EVT_IBUSOCP] = pe5p_notify_hwerr_hdlr,
	[EVT_IBUSUCP_FALL] = pe5p_notify_ibusucpf_hdlr,
	[EVT_VBATOVP] = pe5p_notify_hwerr_hdlr,
	[EVT_IBATOCP] = pe5p_notify_hwerr_hdlr,
	[EVT_VOUTOVP] = pe5p_notify_hwerr_hdlr,
	[EVT_VDROVP] = pe5p_notify_hwerr_hdlr,
	[EVT_VBATOVP_ALARM] = pe5p_notify_vbatovp_alarm_hdlr,
};

static int
(*pe5p_notify_post_hdlr[EVT_MAX])(struct pe5p_algo_info *info) = {
	[EVT_DETACH] = pe5p_notify_detach_hdlr,
	[EVT_HARDRESET] = pe5p_notify_hardreset_hdlr,
	[EVT_VBUSOVP] = pe5p_notify_hwerr_hdlr,
	[EVT_IBUSOCP] = pe5p_notify_hwerr_hdlr,
	[EVT_IBUSUCP_FALL] = pe5p_notify_ibusucpf_hdlr,
	[EVT_VBATOVP] = pe5p_notify_hwerr_hdlr,
	[EVT_IBATOCP] = pe5p_notify_hwerr_hdlr,
	[EVT_VOUTOVP] = pe5p_notify_hwerr_hdlr,
	[EVT_VDROVP] = pe5p_notify_hwerr_hdlr,
	[EVT_VBATOVP_ALARM] = pe5p_notify_vbatovp_alarm_hdlr,
	[EVT_VBUSOVP_ALARM] = pe5p_notify_vbusovp_alarm_hdlr,
};

static int pe5p_pre_handle_notify_evt(struct pe5p_algo_info *info)
{
	unsigned int i;
	struct pe5p_algo_data *data = info->data;

	mutex_lock(&data->notify_lock);
	PE5P_DBG("0x%08X\n", data->notify);
	for (i = 0; i < EVT_MAX; i++) {
		if ((data->notify & BIT(i)) && pe5p_notify_pre_hdlr[i]) {
			data->notify &= ~BIT(i);
			mutex_unlock(&data->notify_lock);
			pe5p_notify_pre_hdlr[i](info);
			mutex_lock(&data->notify_lock);
		}
	}
	mutex_unlock(&data->notify_lock);
	return 0;
}

static int pe5p_post_handle_notify_evt(struct pe5p_algo_info *info)
{
	unsigned int i;
	struct pe5p_algo_data *data = info->data;

	mutex_lock(&data->notify_lock);
	PE5P_DBG("0x%08X\n", data->notify);
	for (i = 0; i < EVT_MAX; i++) {
		if ((data->notify & BIT(i)) && pe5p_notify_post_hdlr[i]) {
			data->notify &= ~BIT(i);
			mutex_unlock(&data->notify_lock);
			pe5p_notify_post_hdlr[i](info);
			mutex_lock(&data->notify_lock);
		}
	}
	mutex_unlock(&data->notify_lock);
	return 0;
}

static int pe5p_dump_charging_info(struct pe5p_algo_info *info)
{
	int ret = 0;
	int vbus = 0, ibus = 0, ibus_swchg = 0, vbat = 0, ibat = 0,
	vsys = 0, tbat = 0;
	int soc = 0;
	struct pe5p_algo_data *data = info->data;

	/* vbus */
	ret = pe5p_get_adc(info, PE5P_ADCCHAN_VBUS, &vbus);
	if (ret < 0)
		PE5P_ERR("get vbus fail(%d)\n", ret);
	/* ibus */
	ret = pe5p_get_adc(info, PE5P_ADCCHAN_IBUS, &ibus);

	if (data->is_swchg_en) {
		ret = pe5p_hal_get_adc(info->alg, CHG1, PE5P_ADCCHAN_IBUS,
				       &ibus_swchg);
		if (ret < 0)
			PE5P_ERR("get swchg ibus fail\n");
	}
	/* vbat */
	ret = pe5p_get_adc(info, PE5P_ADCCHAN_VBAT, &vbat);
	if (ret < 0)
		PE5P_ERR("get vbat fail\n");
	/* ibat */
	ret = pe5p_get_adc(info, PE5P_ADCCHAN_IBAT, &ibat);
	if (ret < 0)
		PE5P_ERR("get ibat fail\n");

	ret = pe5p_get_ta_cap_by_supportive(info, &data->vta_measure,
					     &data->ita_measure);
	if (ret < 0)
		PE5P_ERR("get ta measure cap fail(%d)\n", ret);

	ret = pe5p_hal_get_adc(info->alg, CHG1, PE5P_ADCCHAN_VSYS,
				   &vsys);
	if (ret < 0)
		PE5P_ERR("get vsys from swchg fail\n");

	ret = pe5p_get_adc(info, PE5P_ADCCHAN_TBAT, &tbat);

	ret = pe5p_hal_get_soc(info->alg, &soc);
	if (ret < 0)
		PE5P_ERR("get soc fail\n");

	PE5P_INFO("vbus,ibus,vbat,ibat=%d,%d,%d,%d\n", vbus, ibus, vbat, ibat);
	PE5P_INFO("vta,ita(set,meas)=(%d,%d),(%d,%d),force_cv=%d\n",
		 data->vta_setting, data->vta_measure, data->ita_setting,
		 data->ita_measure, data->force_ta_cv);
	pe5p_hal_dump_registers(info->alg, HVDVCHG1);
	pe5p_hal_dump_registers(info->alg, HVDVCHG2);

	return 0;
}

static int pe5p_algo_threadfn(void *param)
{
	struct pe5p_algo_info *info = param;
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;
	struct pe5p_ta_auth_data *auth_data = &data->ta_auth_data;
	u32 sec, ms, polling_interval;
	ktime_t ktime;
	struct pe5p_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	while (!kthread_should_stop()) {
		wait_event_interruptible(data->wq,
					 atomic_read(&data->wakeup_thread));
		pm_stay_awake(info->dev);
		if (atomic_read(&data->stop_thread)) {
			pm_relax(info->dev);
			break;
		}
		atomic_set(&data->wakeup_thread, 0);
		mutex_lock(&data->lock);
		PE5P_INFO("state = %s\n", pe5p_algo_state_name[data->state]);
		if (atomic_read(&data->stop_algo))
			pe5p_stop(info, &sinfo);
		pe5p_pre_handle_notify_evt(info);
		if (data->state != PE5P_ALGO_STOP) {
			pe5p_algo_check_charging_time(info);
			pe5p_calculate_vbat_ircmp(info);
			pe5p_select_vbat_cv(info);
			pe5p_dump_charging_info(info);
		}
		switch (data->state) {
		case PE5P_ALGO_INIT:
			pe5p_algo_init(info);
			break;
		case PE5P_ALGO_MEASURE_R:
			pe5p_algo_measure_r(info);
			break;
		case PE5P_ALGO_SS_SWCHG:
			pe5p_algo_ss_swchg(info);
			break;
		case PE5P_ALGO_SS_DVCHG:
			pe5p_algo_ss_dvchg(info);
			break;
		case PE5P_ALGO_CC_CV:
			pe5p_algo_cc_cv(info);
			break;
		case PE5P_ALGO_STOP:
			PE5P_INFO("PE5.0 STOP\n");
			break;
		default:
			PE5P_ERR("NO SUCH STATE\n");
			break;
		}
		pe5p_post_handle_notify_evt(info);
		if (data->state != PE5P_ALGO_STOP) {
			if (!pe5p_algo_safety_check(info))
				goto cont;
			pe5p_dump_charging_info(info);
			if (data->state == PE5P_ALGO_CC_CV &&
			    auth_data->support_cc && !data->force_ta_cv)
				polling_interval = desc->polling_interval;
			else
				polling_interval =
					PE5P_INIT_POLLING_INTERVAL;
			sec = polling_interval / 1000;
			ms = polling_interval % 1000;
			ktime = ktime_set(sec, MS_TO_NS(ms));
			alarm_start_relative(&data->timer, ktime);
		}
cont:
		mutex_unlock(&data->lock);
		pm_relax(info->dev);
	}
	return 0;
}

/* =================================================================== */
/* PE5.0 Algo OPS                                                        */
/* =================================================================== */

static int pe5p_init_algo(struct chg_alg_device *alg)
{
	int ret = 0;
	struct pe5p_algo_info *info = chg_alg_dev_get_drvdata(alg);
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;

	mutex_lock(&data->lock);
	PE5P_DBG("++\n");

	if (data->inited) {
		PE5P_INFO("already inited\n");
		goto out;
	}
	if (pe5p_hal_init_hardware(info->alg, desc->support_ta,
				   desc->support_ta_cnt)) {
		PE5P_ERR("%s: init hw fail\n", __func__);
		goto out;
	}
	data->inited = true;
	PE5P_INFO("successfully\n");
out:
	mutex_unlock(&data->lock);
	return ret;
}

static bool pe5p_is_algo_running(struct chg_alg_device *alg)
{
	struct pe5p_algo_info *info = chg_alg_dev_get_drvdata(alg);
	struct pe5p_algo_data *data = info->data;
	bool running = true;

	mutex_lock(&data->lock);

	if (!data->inited) {
		running = false;
		goto out_unlock;
	}
	running = !(data->state == PE5P_ALGO_STOP);
	PE5P_DBG("running = %d\n", running);
out_unlock:
	mutex_unlock(&data->lock);

	return running;
}

static int pe5p_is_algo_ready(struct chg_alg_device *alg)
{
	int ret = 0;
	int soc = 0;
	struct pe5p_algo_info *info = chg_alg_dev_get_drvdata(alg);
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;

	if (algo_waiver_test)
		return ALG_WAIVER;

	if (pe5p_is_algo_running(info->alg))
		return ALG_RUNNING;

	mutex_lock(&data->lock);
	PE5P_DBG("++\n");
	if (!data->inited) {
		ret = ALG_INIT_FAIL;
		goto out;
	}

	PE5P_DBG("run once(%d)\n", data->run_once);
	if (data->run_once) {
		if (!(data->notify & PE5P_RESET_NOTIFY)) {
			ret = ALG_NOT_READY;
			goto out;
		}
		mutex_lock(&data->notify_lock);
		PE5P_INFO("run once but detach/hardreset happened\n");
		data->notify &= ~PE5P_RESET_NOTIFY;
		data->run_once = false;
		data->ta_ready = false;
		mutex_unlock(&data->notify_lock);
	}

	ret = pe5p_hal_get_soc(info->alg, &soc);
	if (ret < 0) {
		PE5P_ERR("get SOC fail(%d)\n", ret);
		ret = ALG_INIT_FAIL;
		goto out;
	}
	if (soc < desc->start_soc_min || soc > desc->start_soc_max) {
		if (soc > 0) {
			PE5P_INFO("soc(%d) not in range(%d~%d)\n", soc,
				  desc->start_soc_min, desc->start_soc_max);
			ret = ALG_WAIVER;
			goto out;
		}
		if (soc == -1 && data->ref_vbat > data->vbat_threshold) {
			PE5P_INFO("soc(%d) not in range(%d~%d)\n", soc,
				  desc->start_soc_min, desc->start_soc_max);
			ret = ALG_WAIVER;
			goto out;
		}
	}

	if (!pe5p_is_ta_rdy(info)) {
		ret = pe5p_hal_is_adapter_ready(alg);
		if (!data->ta_ready)
			ret = ALG_TA_NOT_SUPPORT;
		goto out;
	}
	ret = ALG_READY;
out:
	mutex_unlock(&data->lock);
	return ret;
}

static int pe5p_start_algo(struct chg_alg_device *alg)
{
	int ret = 0, vbat;
	struct pe5p_algo_info *info = chg_alg_dev_get_drvdata(alg);
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;

	if (pe5p_is_algo_running(alg))
		return ALG_RUNNING;

	ret = pe5p_get_adc(info, PE5P_ADCCHAN_VBAT, &vbat);
	if (ret < 0) {
		PE5P_ERR("get vbus fail(%d)\n", ret);
		return ret;
	}
	PE5P_INFO("vbat = %d\n", vbat);

	if (vbat < desc->start_vbat_min) {
		ret = pe5p_hal_set_aicr(info->alg, CHG1, 3000);
		if (ret < 0) {
			PE5P_ERR("set aicr fail(%d)\n", ret);
			return ret;
		}
		ret = pe5p_hal_set_ichg(info->alg, CHG1, 3000);
		if (ret < 0) {
			PE5P_ERR("set ichg fail(%d)\n", ret);
			return ret;
		}
		ret = pe5p_hal_enable_charging(info->alg, CHG1, true);
		if (ret < 0) {
			PE5P_ERR("enable buck boost chg fail(%d)\n", ret);
			return ret;
		}
		return pe5p_set_ta_cap_cv(info, 12000, 3000);
	}

	mutex_lock(&data->lock);
	PE5P_DBG("++\n");
	if (!data->inited || !data->ta_ready) {
		ret = ALG_INIT_FAIL;
		goto out;
	}
	pe5p_hal_enable_sw_vbusovp(alg, false);
	ret = pe5p_start(info);
	if (ret < 0) {
		pe5p_hal_set_ta_cap(info->alg, PE5P_VTA_INIT,
					PE5P_ITA_INIT);
		pe5p_enable_ta_charging(info, false, PE5P_VTA_INIT,
					PE5P_ITA_INIT);
		pe5p_hal_enable_sw_vbusovp(alg, true);
		PE5P_ERR("start PE5.0 algo fail\n");
		ret = ALG_INIT_FAIL;
	}
out:
	mutex_unlock(&data->lock);
	return ret;
}

static int pe5p_plugout_reset(struct chg_alg_device *alg)
{
	int ret = 0;
	struct pe5p_algo_info *info = chg_alg_dev_get_drvdata(alg);
	struct pe5p_algo_data *data = info->data;
	struct pe5p_stop_info sinfo = {
		.reset_ta = false,
		.hardreset_ta = false,
	};

	mutex_lock(&data->lock);
	PE5P_DBG("++\n");
	if (!data->inited)
		goto out;
	ret = __pe5p_plugout_reset(info, &sinfo);
out:
	mutex_unlock(&data->lock);
	return ret;
}

static int pe5p_stop_algo(struct chg_alg_device *alg)
{
	int ret = 0;
	struct pe5p_algo_info *info = chg_alg_dev_get_drvdata(alg);
	struct pe5p_algo_data *data = info->data;
	struct pe5p_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	atomic_set(&data->stop_algo, 1);
	mutex_lock(&data->lock);
	if (!data->inited)
		goto out;
	ret = pe5p_stop(info, &sinfo);
out:
	mutex_unlock(&data->lock);
	return ret;
}

static int pe5p_notifier_call(struct chg_alg_device *alg,
			      struct chg_alg_notify *notify)
{
	int ret = 0;
	struct pe5p_algo_info *info = chg_alg_dev_get_drvdata(alg);
	struct pe5p_algo_data *data = info->data;

	mutex_lock(&data->notify_lock);
	if (data->state == PE5P_ALGO_STOP) {
		if ((notify->evt == EVT_DETACH ||
		     notify->evt == EVT_HARDRESET) && data->run_once) {
			PE5P_INFO("detach/hardreset && run once after stop\n");
			data->notify |= BIT(notify->evt);
		}
		goto out;
	}
	PE5P_INFO("%s\n", chg_alg_notify_evt_tostring(notify->evt));
	switch (notify->evt) {
	case EVT_DETACH:
	case EVT_HARDRESET:
	case EVT_VBUSOVP:
	case EVT_IBUSOCP:
	case EVT_IBUSUCP_FALL:
	case EVT_VBATOVP:
	case EVT_IBATOCP:
	case EVT_VOUTOVP:
	case EVT_VDROVP:
	case EVT_VBATOVP_ALARM:
	case EVT_VBUSOVP_ALARM:
		data->notify |= BIT(notify->evt);
		break;
	default:
		ret = -EINVAL;
		goto out;
	}
	pe5p_wakeup_algo_thread(data);
out:
	mutex_unlock(&data->notify_lock);
	return ret;
}

static int pe5p_set_current_limit(struct chg_alg_device *alg,
				  struct chg_limit_setting *setting)
{
	struct pe5p_algo_info *info = chg_alg_dev_get_drvdata(alg);
	struct pe5p_algo_data *data = info->data;
	struct pe5p_algo_desc *desc = info->desc;
	int cv = micro_to_milli(setting->cv);
	int ic = micro_to_milli(setting->input_current_limit_dvchg1);

	mutex_lock(&data->ext_lock);
	if (cv < desc->vbat_cv)
		cv = desc->vbat_cv;

	if (data->cv_limit != cv || data->input_current_limit != ic) {
		data->cv_limit = cv;
		data->input_current_limit = ic;
		PE5P_INFO("ic = %d, cv = %d\n", ic, cv);
		pe5p_wakeup_algo_thread(data);
	}
	mutex_unlock(&data->ext_lock);
	return 0;
}

int pe5p_set_prop(struct chg_alg_device *alg,
		enum chg_alg_props s, int value)
{
	struct pe5p_algo_info *info = chg_alg_dev_get_drvdata(alg);
	struct pe5p_algo_data *data = info->data;

	pr_notice("%s %d %d\n", __func__, s, value);

	switch (s) {
	case ALG_LOG_LEVEL:
		log_level = value;
		break;
	case ALG_REF_VBAT:
		data->ref_vbat = value;
		break;
	default:
		break;
	}

	return 0;
}

static struct chg_alg_ops pe5p_ops = {
	.init_algo = pe5p_init_algo,
	.is_algo_ready = pe5p_is_algo_ready,
	.start_algo = pe5p_start_algo,
	.is_algo_running = pe5p_is_algo_running,
	.plugout_reset = pe5p_plugout_reset,
	.stop_algo = pe5p_stop_algo,
	.notifier_call = pe5p_notifier_call,
	.set_current_limit = pe5p_set_current_limit,
	.set_prop = pe5p_set_prop,
};

#define PE5P_DT_VALPROP_ARR(name, var_name, sz) \
	{name, offsetof(struct pe5p_algo_desc, var_name), sz}

#define PE5P_DT_VALPROP(name, var_name) \
	PE5P_DT_VALPROP_ARR(name, var_name, 1)

struct pe5p_dtprop {
	const char *name;
	size_t offset;
	size_t sz;
};

static inline void pe5p_parse_dt_u32(struct device_node *np, void *desc,
				     const struct pe5p_dtprop *props,
				     int prop_cnt)
{
	unsigned int i;

	for (i = 0; i < prop_cnt; i++) {
		if (unlikely(!props[i].name))
			continue;
		of_property_read_u32(np, props[i].name, desc + props[i].offset);
	}
}

static inline void pe5p_parse_dt_u32_arr(struct device_node *np, void *desc,
					 const struct pe5p_dtprop *props,
					 int prop_cnt)
{
	unsigned int i;

	for (i = 0; i < prop_cnt; i++) {
		if (unlikely(!props[i].name))
			continue;
		of_property_read_u32_array(np, props[i].name,
					   desc + props[i].offset, props[i].sz);
	}
}

static inline int __of_property_read_s32_array(const struct device_node *np,
					       const char *propname,
					       s32 *out_values, size_t sz)
{
	return of_property_read_u32_array(np, propname, (u32 *)out_values, sz);
}

static inline void pe5p_parse_dt_s32_arr(struct device_node *np, void *desc,
					 const struct pe5p_dtprop *props,
					 int prop_cnt)
{
	unsigned int i;

	for (i = 0; i < prop_cnt; i++) {
		if (unlikely(!props[i].name))
			continue;
		__of_property_read_s32_array(np, props[i].name,
					     desc + props[i].offset,
					     props[i].sz);
	}
}

static const struct pe5p_dtprop pe5p_dtprops_u32[] = {
	PE5P_DT_VALPROP("polling-interval", polling_interval),
	PE5P_DT_VALPROP("ta-cv-ss-repeat-tmin", ta_cv_ss_repeat_tmin),
	PE5P_DT_VALPROP("vbat-cv", vbat_cv),
	PE5P_DT_VALPROP("start-soc-min", start_soc_min),
	PE5P_DT_VALPROP("start-soc-max", start_soc_max),
	PE5P_DT_VALPROP("start-vbat-min", start_vbat_min),
	PE5P_DT_VALPROP("start-vbat-max", start_vbat_max),
	PE5P_DT_VALPROP("idvchg-term", idvchg_term),
	PE5P_DT_VALPROP("idvchg-step", idvchg_step),
	PE5P_DT_VALPROP("idvchg-ss-init", idvchg_ss_init),
	PE5P_DT_VALPROP("idvchg-ss-step", idvchg_ss_step),
	PE5P_DT_VALPROP("idvchg-ss-step1", idvchg_ss_step1),
	PE5P_DT_VALPROP("idvchg-ss-step2", idvchg_ss_step2),
	PE5P_DT_VALPROP("idvchg-ss-step1-vbat", idvchg_ss_step1_vbat),
	PE5P_DT_VALPROP("idvchg-ss-step2-vbat", idvchg_ss_step2_vbat),
	PE5P_DT_VALPROP("ta-blanking", ta_blanking),
	PE5P_DT_VALPROP("swchg-aicr", swchg_aicr),
	PE5P_DT_VALPROP("swchg-ichg", swchg_ichg),
	PE5P_DT_VALPROP("swchg-aicr-ss-init", swchg_aicr_ss_init),
	PE5P_DT_VALPROP("swchg-aicr-ss-step", swchg_aicr_ss_step),
	PE5P_DT_VALPROP("swchg-off-vbat", swchg_off_vbat),
	PE5P_DT_VALPROP("force-ta-cv-vbat", force_ta_cv_vbat),
	PE5P_DT_VALPROP("chg-time-max", chg_time_max),
	PE5P_DT_VALPROP("tta-recovery-area", tta_recovery_area),
	PE5P_DT_VALPROP("tbat-recovery-area", tbat_recovery_area),
	PE5P_DT_VALPROP("tdvchg-recovery-area", tdvchg_recovery_area),
	PE5P_DT_VALPROP("tswchg-recovery-area", tswchg_recovery_area),
	PE5P_DT_VALPROP("ifod-threshold", ifod_threshold),
	PE5P_DT_VALPROP("rsw-min", rsw_min),
	PE5P_DT_VALPROP("ircmp-rbat", ircmp_rbat),
	PE5P_DT_VALPROP("ircmp-vclamp", ircmp_vclamp),
	PE5P_DT_VALPROP("vta-cap-min", vta_cap_min),
	PE5P_DT_VALPROP("vta-cap-max", vta_cap_max),
	PE5P_DT_VALPROP("ita-cap-min", ita_cap_min),
};

static const struct pe5p_dtprop pe5p_dtprops_u32_array[] = {
	PE5P_DT_VALPROP_ARR("ita-level", ita_level, PE5P_RCABLE_MAX),
	PE5P_DT_VALPROP_ARR("rcable-level", rcable_level, PE5P_RCABLE_MAX),
	PE5P_DT_VALPROP_ARR("ita-level-dual", ita_level_dual, PE5P_RCABLE_MAX),
	PE5P_DT_VALPROP_ARR("rcable-level-dual", rcable_level_dual, PE5P_RCABLE_MAX),
};

static const struct pe5p_dtprop pe5p_dtprops_s32_array[] = {
	PE5P_DT_VALPROP_ARR("tta-level-def", tta_level_def, PE5P_THERMAL_MAX),
	PE5P_DT_VALPROP_ARR("tta-curlmt", tta_curlmt, PE5P_THERMAL_MAX),
	PE5P_DT_VALPROP_ARR("tbat-level-def", tbat_level_def, PE5P_THERMAL_MAX),
	PE5P_DT_VALPROP_ARR("tbat-curlmt", tbat_curlmt, PE5P_THERMAL_MAX),
	PE5P_DT_VALPROP_ARR("tdvchg-level-def", tdvchg_level_def, PE5P_THERMAL_MAX),
	PE5P_DT_VALPROP_ARR("tdvchg-curlmt", tdvchg_curlmt, PE5P_THERMAL_MAX),
	PE5P_DT_VALPROP_ARR("tswchg-level_def", tswchg_level_def, PE5P_THERMAL_MAX),
	PE5P_DT_VALPROP_ARR("tswchg-curlmt", tswchg_curlmt, PE5P_THERMAL_MAX),
};

static int pe5p_parse_dt(struct pe5p_algo_info *info)
{
	int i;
	struct pe5p_algo_desc *desc;
	struct pe5p_algo_data *data;
	struct device_node *np = info->dev->of_node;
	u32 val;
	int nondash_flag = 0;
	int ret = 0;

	desc = devm_kzalloc(info->dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;
	info->desc = desc;
	data = info->data;
	memcpy(desc, &algo_desc_defval, sizeof(*desc));

	ret = of_property_count_strings(np, "support-ta");
	if (ret < 0) {
		nondash_flag = 1;
		ret = of_property_count_strings(np, "support_ta");
		if (ret < 0)
			return ret;
	}

	desc->support_ta_cnt = ret;
	desc->support_ta = devm_kzalloc(info->dev, ret * sizeof(char *),
					GFP_KERNEL);
	if (!desc->support_ta)
		return -ENOMEM;
	for (i = 0; i < desc->support_ta_cnt; i++) {
		if (nondash_flag)
			ret = of_property_read_string_index(np, "support_ta", i,
							    &desc->support_ta[i]);
		else
			ret = of_property_read_string_index(np, "support-ta", i,
							    &desc->support_ta[i]);
		if (ret < 0)
			return ret;
		PE5P_INFO("support ta(%s)\n", desc->support_ta[i]);
	}

	desc->allow_not_check_ta_status =
		of_property_read_bool(np, "allow_not_check_ta_status")
			|| of_property_read_bool(np, "allow-not-check-ta-status");
	pe5p_parse_dt_u32(np, (void *)desc, pe5p_dtprops_u32,
			  ARRAY_SIZE(pe5p_dtprops_u32));
	pe5p_parse_dt_u32_arr(np, (void *)desc, pe5p_dtprops_u32_array,
			      ARRAY_SIZE(pe5p_dtprops_u32_array));
	pe5p_parse_dt_s32_arr(np, (void *)desc, pe5p_dtprops_s32_array,
			      ARRAY_SIZE(pe5p_dtprops_s32_array));
	if (desc->swchg_aicr == 0 || desc->swchg_ichg == 0) {
		desc->swchg_aicr = 0;
		desc->swchg_ichg = 0;
	}

	if (of_property_read_u32(np, "vbat_threshold", &val) >= 0)
		data->vbat_threshold = val;
	else if (of_property_read_u32(np, "vbat-threshold", &val) >= 0)
		data->vbat_threshold = val;
	else {
		pr_notice("turn off vbat_threshold checking:%d\n",
			DISABLE_VBAT_THRESHOLD);
		data->vbat_threshold = DISABLE_VBAT_THRESHOLD;
	}

	return 0;
}

static int pe5p_probe(struct platform_device *pdev)
{
	int ret;
	struct pe5p_algo_info *info;
	struct pe5p_algo_data *data;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	info->data = data;
	info->dev = &pdev->dev;
	platform_set_drvdata(pdev, info);

	ret = pe5p_parse_dt(info);
	if (ret < 0) {
		PE5P_ERR("%s parse dt fail(%d)\n", __func__, ret);
		return ret;
	}

	mutex_init(&data->notify_lock);
	mutex_init(&data->lock);
	mutex_init(&data->ext_lock);
	init_waitqueue_head(&data->wq);
	atomic_set(&data->wakeup_thread, 0);
	atomic_set(&data->stop_thread, 0);
	data->state = PE5P_ALGO_STOP;
	alarm_init(&data->timer, ALARM_REALTIME, pe5p_algo_timer_cb);
	data->task = kthread_run(pe5p_algo_threadfn, info, "pe5p_algo_task");
	if (IS_ERR(data->task)) {
		ret = PTR_ERR(data->task);
		PE5P_ERR("%s run task fail(%d)\n", __func__, ret);
		goto err;
	}
	device_init_wakeup(info->dev, true);

	info->alg = chg_alg_device_register("pe5p", info->dev, info, &pe5p_ops,
					    NULL);
	if (IS_ERR_OR_NULL(info->alg)) {
		PE5P_ERR("%s reg pe5p algo fail(%d)\n", __func__, ret);
		ret = PTR_ERR(info->alg);
		goto err;
	}
	chg_alg_dev_set_drvdata(info->alg, info);

	dev_info(info->dev, "%s successfully\n", __func__);
	return 0;
err:
	mutex_destroy(&data->ext_lock);
	mutex_destroy(&data->lock);
	mutex_destroy(&data->notify_lock);
	chg_alg_device_unregister(info->alg);
	return ret;
}

static int pe5p_remove(struct platform_device *pdev)
{
	struct pe5p_algo_info *info = platform_get_drvdata(pdev);
	struct pe5p_algo_data *data;

	if (info) {
		data = info->data;
		atomic_set(&data->stop_thread, 1);
		pe5p_wakeup_algo_thread(data);
		kthread_stop(data->task);
		mutex_destroy(&data->ext_lock);
		mutex_destroy(&data->lock);
		mutex_destroy(&data->notify_lock);
		chg_alg_device_unregister(info->alg);
	}

	return 0;
}

static int __maybe_unused pe5p_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct pe5p_algo_info *info = platform_get_drvdata(pdev);
	struct pe5p_algo_data *data = info->data;

	mutex_lock(&data->lock);
	return 0;
}

static int __maybe_unused pe5p_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct pe5p_algo_info *info = platform_get_drvdata(pdev);
	struct pe5p_algo_data *data = info->data;

	mutex_unlock(&data->lock);
	return 0;
}

static SIMPLE_DEV_PM_OPS(pe5p_pm_ops, pe5p_suspend, pe5p_resume);

static const struct of_device_id mtk_pe5p_of_match[] = {
	{ .compatible = "mediatek,charger,pe5p", },
	{},
};
MODULE_DEVICE_TABLE(of, mtk_pe5p_of_match);

static struct platform_driver pe5p_platdrv = {
	.probe = pe5p_probe,
	.remove = pe5p_remove,
	.driver = {
		.name = "pe5p",
		.owner = THIS_MODULE,
		.pm = &pe5p_pm_ops,
		.of_match_table = mtk_pe5p_of_match,
	},
};

static int __init pe5p_init(void)
{
	return platform_driver_register(&pe5p_platdrv);
}

static void __exit pe5p_exit(void)
{
	platform_driver_unregister(&pe5p_platdrv);
}
late_initcall(pe5p_init);
module_exit(pe5p_exit);

MODULE_AUTHOR("ChiYuan Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("MTK Pump Express 5 Plus Algorithm");
MODULE_LICENSE("GPL");
