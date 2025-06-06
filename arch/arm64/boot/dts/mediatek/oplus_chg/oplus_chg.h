//ifdef OPLUS_FEATURE_CHG_BASIC

#define OPLUS_CHG_IC_BUCK		0
#define OPLUS_CHG_IC_BOOST		1
#define OPLUS_CHG_IC_BUCK_BOOST		2
#define OPLUS_CHG_IC_CP_DIV2		3
#define OPLUS_CHG_IC_CP_MUL2		4
#define OPLUS_CHG_IC_CP_TW2		5
#define OPLUS_CHG_IC_RX			6
#define OPLUS_CHG_IC_VIRTUAL_RX		7
#define OPLUS_CHG_IC_VIRTUAL_BUCK	8
#define OPLUS_CHG_IC_VIRTUAL_CP		9
#define OPLUS_CHG_IC_VIRTUAL_USB	10
#define OPLUS_CHG_IC_TYPEC		11
#define OPLUS_CHG_IC_GAUGE		12
#define OPLUS_CHG_IC_VIRTUAL_GAUGE	13
#define OPLUS_CHG_IC_ASIC		14
#define OPLUS_CHG_IC_VIRTUAL_ASIC	15
#define OPLUS_CHG_IC_VPHY		16
#define OPLUS_CHG_IC_VIRTUAL_VPHY	17
#define OPLUS_CHG_IC_SWITCH		18
#define OPLUS_CHG_IC_VIRTUAL_SWITCH	19
#define OPLUS_CHG_IC_CP			20
#define OPLUS_CHG_IC_VIRTUAL_UFCS	21
#define OPLUS_CHG_IC_MISC		22
#define OPLUS_CHG_IC_PPS		23
#define OPLUS_CHG_IC_VIRTUAL_PPS	24
#define OPLUS_CHG_IC_UFCS		25

#define OPLUS_CHG_IC_CONNECT_PARALLEL	0
#define OPLUS_CHG_IC_CONNECT_SERIAL	1

/* virq ID */
#define OPLUS_IC_VIRQ_ERR		0
#define OPLUS_IC_VIRQ_CC_DETECT		1
#define OPLUS_IC_VIRQ_PLUGIN		2
#define OPLUS_IC_VIRQ_CC_CHANGED	3
#define OPLUS_IC_VIRQ_VOOC_DATA		4
#define OPLUS_IC_VIRQ_SUSPEND_CHECK	5
#define OPLUS_IC_VIRQ_CHG_TYPE_CHANGE	6
#define OPLUS_IC_VIRQ_OFFLINE		7
#define OPLUS_IC_VIRQ_RESUME		8
#define OPLUS_IC_VIRQ_SVID		9
#define OPLUS_IC_VIRQ_OTG_ENABLE	10
#define OPLUS_IC_VIRQ_VOLTAGE_CHANGED	11
#define OPLUS_IC_VIRQ_CURRENT_CHANGED	12
#define OPLUS_IC_VIRQ_BC12_COMPLETED	13
#define OPLUS_IC_VIRQ_DATA_ROLE_CHANGED	14
#define OPLUS_IC_VIRQ_ONLINE		15
#define OPLUS_IC_VIRQ_TYPEC_STATE	16
#define OPLUS_IC_VIRQ_HARD_RESET	17
#define OPLUS_IC_VIRQ_POWER_CHANGED	18
#define OPLUS_IC_VIRQ_PRESENT		19
#define OPLUS_IC_VIRQ_EVENT_CHANGED	20

/* common */
#define OPLUS_IC_FUNC_NUM_COMMON_START			0
#define OPLUS_IC_FUNC_EXIT				(OPLUS_IC_FUNC_NUM_COMMON_START + 0)
#define OPLUS_IC_FUNC_INIT				(OPLUS_IC_FUNC_NUM_COMMON_START + 1)
#define OPLUS_IC_FUNC_REG_DUMP				(OPLUS_IC_FUNC_NUM_COMMON_START + 2)
#define OPLUS_IC_FUNC_SMT_TEST				(OPLUS_IC_FUNC_NUM_COMMON_START + 3)
#define OPLUS_IC_FUNC_CHIP_ENABLE			(OPLUS_IC_FUNC_NUM_COMMON_START + 4)
#define OPLUS_IC_FUNC_CHIP_IS_ENABLE			(OPLUS_IC_FUNC_NUM_COMMON_START + 5)

/* wireless rx */
#define OPLUS_IC_FUNC_NUM_RX_START			100
#define OPLUS_IC_FUNC_RX_SET_ENABLE			(OPLUS_IC_FUNC_NUM_RX_START + 0)
#define OPLUS_IC_FUNC_RX_IS_ENABLE			(OPLUS_IC_FUNC_NUM_RX_START + 1)
#define OPLUS_IC_FUNC_RX_IS_CONNECTED			(OPLUS_IC_FUNC_NUM_RX_START + 2)
#define OPLUS_IC_FUNC_RX_GET_VOUT			(OPLUS_IC_FUNC_NUM_RX_START + 3)
#define OPLUS_IC_FUNC_RX_SET_VOUT			(OPLUS_IC_FUNC_NUM_RX_START + 4)
#define OPLUS_IC_FUNC_RX_GET_VRECT			(OPLUS_IC_FUNC_NUM_RX_START + 5)
#define OPLUS_IC_FUNC_RX_GET_IOUT			(OPLUS_IC_FUNC_NUM_RX_START + 6)
#define OPLUS_IC_FUNC_RX_GET_TRX_VOL			(OPLUS_IC_FUNC_NUM_RX_START + 7)
#define OPLUS_IC_FUNC_RX_GET_TRX_CURR			(OPLUS_IC_FUNC_NUM_RX_START + 8)
#define OPLUS_IC_FUNC_RX_GET_CEP_COUNT			(OPLUS_IC_FUNC_NUM_RX_START + 9)
#define OPLUS_IC_FUNC_RX_GET_CEP_VAL			(OPLUS_IC_FUNC_NUM_RX_START + 10)
#define OPLUS_IC_FUNC_RX_GET_WORK_FREQ			(OPLUS_IC_FUNC_NUM_RX_START + 11)
#define OPLUS_IC_FUNC_RX_GET_RX_MODE			(OPLUS_IC_FUNC_NUM_RX_START + 12)
#define OPLUS_IC_FUNC_RX_SET_RX_MODE			(OPLUS_IC_FUNC_NUM_RX_START + 13)
#define OPLUS_IC_FUNC_RX_SET_DCDC_ENABLE		(OPLUS_IC_FUNC_NUM_RX_START + 14)
#define OPLUS_IC_FUNC_RX_SET_TRX_ENABLE			(OPLUS_IC_FUNC_NUM_RX_START + 15)
#define OPLUS_IC_FUNC_RX_SET_TRX_START			(OPLUS_IC_FUNC_NUM_RX_START + 16)
#define OPLUS_IC_FUNC_RX_GET_TRX_STATUS			(OPLUS_IC_FUNC_NUM_RX_START + 17)
#define OPLUS_IC_FUNC_RX_GET_TRX_ERR			(OPLUS_IC_FUNC_NUM_RX_START + 18)
#define OPLUS_IC_FUNC_RX_GET_HEADROOM			(OPLUS_IC_FUNC_NUM_RX_START + 19)
#define OPLUS_IC_FUNC_RX_SET_HEADROOM			(OPLUS_IC_FUNC_NUM_RX_START + 20)
#define OPLUS_IC_FUNC_RX_SEND_MATCH_Q			(OPLUS_IC_FUNC_NUM_RX_START + 21)
#define OPLUS_IC_FUNC_RX_SET_FOD_PARM			(OPLUS_IC_FUNC_NUM_RX_START + 22)
#define OPLUS_IC_FUNC_RX_SEND_MSG			(OPLUS_IC_FUNC_NUM_RX_START + 23)
#define OPLUS_IC_FUNC_RX_REG_MSG_CALLBACK		(OPLUS_IC_FUNC_NUM_RX_START + 24)
#define OPLUS_IC_FUNC_RX_GET_RX_VERSION			(OPLUS_IC_FUNC_NUM_RX_START + 25)
#define OPLUS_IC_FUNC_RX_GET_TRX_VERSION		(OPLUS_IC_FUNC_NUM_RX_START + 26)
#define OPLUS_IC_FUNC_RX_UPGRADE_FW_BY_BUF		(OPLUS_IC_FUNC_NUM_RX_START + 27)
#define OPLUS_IC_FUNC_RX_UPGRADE_FW_BY_IMG		(OPLUS_IC_FUNC_NUM_RX_START + 28)
#define OPLUS_IC_FUNC_RX_CHECK_CONNECT			(OPLUS_IC_FUNC_NUM_RX_START + 29)
#define OPLUS_IC_FUNC_RX_GET_EVENT_CODE			(OPLUS_IC_FUNC_NUM_RX_START + 30)
#define OPLUS_IC_FUNC_RX_GET_BRIDGE_MODE		(OPLUS_IC_FUNC_NUM_RX_START + 31)
#define OPLUS_IC_FUNC_RX_DIS_INSERT			(OPLUS_IC_FUNC_NUM_RX_START + 32)
#define OPLUS_IC_FUNC_RX_STANDBY_CONFIG			(OPLUS_IC_FUNC_NUM_RX_START + 33)
#define OPLUS_IC_FUNC_RX_SET_COMU			(OPLUS_IC_FUNC_NUM_RX_START + 34)

/* buck/boost */
#define OPLUS_IC_FUNC_NUM_BUCK_START			200
#define OPLUS_IC_FUNC_BUCK_INPUT_PRESENT		(OPLUS_IC_FUNC_NUM_BUCK_START + 0)
#define OPLUS_IC_FUNC_BUCK_INPUT_SUSPEND		(OPLUS_IC_FUNC_NUM_BUCK_START + 1)
#define OPLUS_IC_FUNC_BUCK_INPUT_IS_SUSPEND		(OPLUS_IC_FUNC_NUM_BUCK_START + 2)
#define OPLUS_IC_FUNC_BUCK_OUTPUT_SUSPEND		(OPLUS_IC_FUNC_NUM_BUCK_START + 3)
#define OPLUS_IC_FUNC_BUCK_OUTPUT_IS_SUSPEND		(OPLUS_IC_FUNC_NUM_BUCK_START + 4)
#define OPLUS_IC_FUNC_BUCK_SET_ICL			(OPLUS_IC_FUNC_NUM_BUCK_START + 5)
#define OPLUS_IC_FUNC_BUCK_GET_ICL			(OPLUS_IC_FUNC_NUM_BUCK_START + 6)
#define OPLUS_IC_FUNC_BUCK_SET_FCC			(OPLUS_IC_FUNC_NUM_BUCK_START + 7)
#define OPLUS_IC_FUNC_BUCK_SET_FV			(OPLUS_IC_FUNC_NUM_BUCK_START + 8)
#define OPLUS_IC_FUNC_BUCK_SET_ITERM			(OPLUS_IC_FUNC_NUM_BUCK_START + 9)
#define OPLUS_IC_FUNC_BUCK_SET_RECHG_VOL		(OPLUS_IC_FUNC_NUM_BUCK_START + 10)
#define OPLUS_IC_FUNC_BUCK_GET_INPUT_CURR		(OPLUS_IC_FUNC_NUM_BUCK_START + 11)
#define OPLUS_IC_FUNC_BUCK_GET_INPUT_VOL		(OPLUS_IC_FUNC_NUM_BUCK_START + 12)
#define OPLUS_IC_FUNC_BUCK_AICL_ENABLE			(OPLUS_IC_FUNC_NUM_BUCK_START + 13)
#define OPLUS_IC_FUNC_BUCK_AICL_RERUN			(OPLUS_IC_FUNC_NUM_BUCK_START + 14)
#define OPLUS_IC_FUNC_BUCK_AICL_RESET			(OPLUS_IC_FUNC_NUM_BUCK_START + 15)
#define OPLUS_IC_FUNC_BUCK_GET_CC_ORIENTATION		(OPLUS_IC_FUNC_NUM_BUCK_START + 16)
#define OPLUS_IC_FUNC_BUCK_GET_HW_DETECT		(OPLUS_IC_FUNC_NUM_BUCK_START + 17)
#define OPLUS_IC_FUNC_BUCK_GET_CHARGER_TYPE		(OPLUS_IC_FUNC_NUM_BUCK_START + 18)
#define OPLUS_IC_FUNC_BUCK_RERUN_BC12			(OPLUS_IC_FUNC_NUM_BUCK_START + 19)
#define OPLUS_IC_FUNC_BUCK_QC_DETECT_ENABLE		(OPLUS_IC_FUNC_NUM_BUCK_START + 20)
#define OPLUS_IC_FUNC_BUCK_SHIPMODE_ENABLE		(OPLUS_IC_FUNC_NUM_BUCK_START + 21)
#define OPLUS_IC_FUNC_BUCK_SET_QC_CONFIG		(OPLUS_IC_FUNC_NUM_BUCK_START + 22)
#define OPLUS_IC_FUNC_BUCK_SET_PD_CONFIG		(OPLUS_IC_FUNC_NUM_BUCK_START + 23)
#define OPLUS_IC_FUNC_BUCK_GET_VBUS_COLLAPSE_STATUS	(OPLUS_IC_FUNC_NUM_BUCK_START + 24)
#define OPLUS_IC_FUNC_BUCK_CURR_DROP			(OPLUS_IC_FUNC_NUM_BUCK_START + 25)
#define OPLUS_IC_FUNC_BUCK_WDT_ENABLE			(OPLUS_IC_FUNC_NUM_BUCK_START + 26)
#define OPLUS_IC_FUNC_BUCK_KICK_WDT			(OPLUS_IC_FUNC_NUM_BUCK_START + 27)
#define OPLUS_IC_FUNC_BUCK_BC12_COMPLETED		(OPLUS_IC_FUNC_NUM_BUCK_START + 28)
#define OPLUS_IC_FUNC_BUCK_SET_AICL_POINT		(OPLUS_IC_FUNC_NUM_BUCK_START + 29)
#define OPLUS_IC_FUNC_BUCK_SET_VINDPM			(OPLUS_IC_FUNC_NUM_BUCK_START + 30)
#define OPLUS_IC_FUNC_BUCK_HARDWARE_INIT		(OPLUS_IC_FUNC_NUM_BUCK_START + 31)
#define OPLUS_IC_FUNC_BUCK_GET_TYPEC_STATE		(OPLUS_IC_FUNC_NUM_BUCK_START + 32)
#define OPLUS_IC_FUNC_BUCK_GET_FV			(OPLUS_IC_FUNC_NUM_BUCK_START + 33)
#define OPLUS_IC_FUNC_BUCK_WLS_INPUT_SUSPEND		(OPLUS_IC_FUNC_NUM_BUCK_START + 34)
#define OPLUS_IC_FUNC_BUCK_SET_WLS_ICL			(OPLUS_IC_FUNC_NUM_BUCK_START + 35)
#define OPLUS_IC_FUNC_BUCK_GET_WLS_ICL			(OPLUS_IC_FUNC_NUM_BUCK_START + 36)
#define OPLUS_IC_FUNC_BUCK_GET_WLS_INPUT_CURR		(OPLUS_IC_FUNC_NUM_BUCK_START + 37)
#define OPLUS_IC_FUNC_BUCK_GET_WLS_INPUT_VOL		(OPLUS_IC_FUNC_NUM_BUCK_START + 38)
#define OPLUS_IC_FUNC_BUCK_WLS_AICL_ENABLE		(OPLUS_IC_FUNC_NUM_BUCK_START + 39)
#define OPLUS_IC_FUNC_BUCK_WLS_AICL_RERUN		(OPLUS_IC_FUNC_NUM_BUCK_START + 40)
#define OPLUS_IC_FUNC_BUCK_DIS_INSERT_DETECT		(OPLUS_IC_FUNC_NUM_BUCK_START + 41)

/* charge pump */
#define OPLUS_IC_FUNC_NUM_CP_START			300
#define OPLUS_IC_FUNC_CP_ENABLE				(OPLUS_IC_FUNC_NUM_CP_START + 0)
#define OPLUS_IC_FUNC_CP_HW_INTI			(OPLUS_IC_FUNC_NUM_CP_START + 1)
#define OPLUS_IC_FUNC_CP_SET_WORK_MODE			(OPLUS_IC_FUNC_NUM_CP_START + 2)
#define OPLUS_IC_FUNC_CP_GET_WORK_MODE			(OPLUS_IC_FUNC_NUM_CP_START + 3)
#define OPLUS_IC_FUNC_CP_CHECK_WORK_MODE_SUPPORT	(OPLUS_IC_FUNC_NUM_CP_START + 4)
#define OPLUS_IC_FUNC_CP_SET_IIN			(OPLUS_IC_FUNC_NUM_CP_START + 5)
#define OPLUS_IC_FUNC_CP_GET_VIN			(OPLUS_IC_FUNC_NUM_CP_START + 6)
#define OPLUS_IC_FUNC_CP_GET_IIN			(OPLUS_IC_FUNC_NUM_CP_START + 7)
#define OPLUS_IC_FUNC_CP_GET_VOUT			(OPLUS_IC_FUNC_NUM_CP_START + 8)
#define OPLUS_IC_FUNC_CP_GET_IOUT			(OPLUS_IC_FUNC_NUM_CP_START + 9)
#define OPLUS_IC_FUNC_CP_GET_VAC			(OPLUS_IC_FUNC_NUM_CP_START + 10)
#define OPLUS_IC_FUNC_CP_SET_WORK_START			(OPLUS_IC_FUNC_NUM_CP_START + 11)
#define OPLUS_IC_FUNC_CP_GET_WORK_STATUS		(OPLUS_IC_FUNC_NUM_CP_START + 12)
#define OPLUS_IC_FUNC_CP_SET_ADC_ENABLE			(OPLUS_IC_FUNC_NUM_CP_START + 13)
#define OPLUS_IC_FUNC_CP_WATCHDOG_ENABLE		(OPLUS_IC_FUNC_NUM_CP_START + 14)

/* gauge */
#define OPLUS_IC_FUNC_NUM_GAUGE_START			400
#define OPLUS_IC_FUNC_GAUGE_UPDATE			(OPLUS_IC_FUNC_NUM_GAUGE_START + 0)
#define OPLUS_IC_FUNC_GAUGE_GET_BATT_VOL		(OPLUS_IC_FUNC_NUM_GAUGE_START + 1)
#define OPLUS_IC_FUNC_GAUGE_GET_BATT_MAX		(OPLUS_IC_FUNC_NUM_GAUGE_START + 2)
#define OPLUS_IC_FUNC_GAUGE_GET_BATT_MIN		(OPLUS_IC_FUNC_NUM_GAUGE_START + 3)
#define OPLUS_IC_FUNC_GAUGE_GET_BATT_CURR		(OPLUS_IC_FUNC_NUM_GAUGE_START + 4)
#define OPLUS_IC_FUNC_GAUGE_GET_BATT_TEMP		(OPLUS_IC_FUNC_NUM_GAUGE_START + 5)
#define OPLUS_IC_FUNC_GAUGE_GET_BATT_SOC		(OPLUS_IC_FUNC_NUM_GAUGE_START + 6)
#define OPLUS_IC_FUNC_GAUGE_GET_BATT_FCC		(OPLUS_IC_FUNC_NUM_GAUGE_START + 7)
#define OPLUS_IC_FUNC_GAUGE_GET_BATT_CC			(OPLUS_IC_FUNC_NUM_GAUGE_START + 8)
#define OPLUS_IC_FUNC_GAUGE_GET_BATT_RM			(OPLUS_IC_FUNC_NUM_GAUGE_START + 9)
#define OPLUS_IC_FUNC_GAUGE_GET_BATT_SOH		(OPLUS_IC_FUNC_NUM_GAUGE_START + 10)
#define OPLUS_IC_FUNC_GAUGE_GET_BATT_AUTH		(OPLUS_IC_FUNC_NUM_GAUGE_START + 11)
#define OPLUS_IC_FUNC_GAUGE_GET_BATT_HMAC		(OPLUS_IC_FUNC_NUM_GAUGE_START + 12)
#define OPLUS_IC_FUNC_GAUGE_SET_BATT_FULL		(OPLUS_IC_FUNC_NUM_GAUGE_START + 13)
#define OPLUS_IC_FUNC_GAUGE_GET_BATT_FC			(OPLUS_IC_FUNC_NUM_GAUGE_START + 14)
#define OPLUS_IC_FUNC_GAUGE_GET_BATT_QM			(OPLUS_IC_FUNC_NUM_GAUGE_START + 15)
#define OPLUS_IC_FUNC_GAUGE_GET_BATT_PD			(OPLUS_IC_FUNC_NUM_GAUGE_START + 16)
#define OPLUS_IC_FUNC_GAUGE_GET_BATT_RCU		(OPLUS_IC_FUNC_NUM_GAUGE_START + 17)
#define OPLUS_IC_FUNC_GAUGE_GET_BATT_RCF		(OPLUS_IC_FUNC_NUM_GAUGE_START + 18)
#define OPLUS_IC_FUNC_GAUGE_GET_BATT_FCU		(OPLUS_IC_FUNC_NUM_GAUGE_START + 19)
#define OPLUS_IC_FUNC_GAUGE_GET_BATT_FCF		(OPLUS_IC_FUNC_NUM_GAUGE_START + 20)
#define OPLUS_IC_FUNC_GAUGE_GET_BATT_SOU		(OPLUS_IC_FUNC_NUM_GAUGE_START + 21)
#define OPLUS_IC_FUNC_GAUGE_GET_BATT_DO0		(OPLUS_IC_FUNC_NUM_GAUGE_START + 22)
#define OPLUS_IC_FUNC_GAUGE_GET_BATT_DOE		(OPLUS_IC_FUNC_NUM_GAUGE_START + 23)
#define OPLUS_IC_FUNC_GAUGE_GET_BATT_TRM		(OPLUS_IC_FUNC_NUM_GAUGE_START + 24)
#define OPLUS_IC_FUNC_GAUGE_GET_BATT_PC			(OPLUS_IC_FUNC_NUM_GAUGE_START + 25)
#define OPLUS_IC_FUNC_GAUGE_GET_BATT_QS			(OPLUS_IC_FUNC_NUM_GAUGE_START + 26)
#define OPLUS_IC_FUNC_GAUGE_UPDATE_DOD0			(OPLUS_IC_FUNC_NUM_GAUGE_START + 27)
#define OPLUS_IC_FUNC_GAUGE_UPDATE_SOC_SMOOTH		(OPLUS_IC_FUNC_NUM_GAUGE_START + 28)
#define OPLUS_IC_FUNC_GAUGE_GET_CB_STATUS		(OPLUS_IC_FUNC_NUM_GAUGE_START + 29)
#define OPLUS_IC_FUNC_GAUGE_GET_PASSEDCHG		(OPLUS_IC_FUNC_NUM_GAUGE_START + 30)
#define OPLUS_IC_FUNC_GAUGE_SET_LOCK			(OPLUS_IC_FUNC_NUM_GAUGE_START + 31)
#define OPLUS_IC_FUNC_GAUGE_IS_LOCKED			(OPLUS_IC_FUNC_NUM_GAUGE_START + 32)
#define OPLUS_IC_FUNC_GAUGE_GET_BATT_NUM		(OPLUS_IC_FUNC_NUM_GAUGE_START + 33)
#define OPLUS_IC_FUNC_GAUGE_GET_DEVICE_TYPE		(OPLUS_IC_FUNC_NUM_GAUGE_START + 34)
#define OPLUS_IC_FUNC_GAUGE_GET_DEVICE_TYPE_FOR_VOOC	(OPLUS_IC_FUNC_NUM_GAUGE_START + 35)
#define OPLUS_IC_FUNC_GAUGE_GET_BATT_EXIST		(OPLUS_IC_FUNC_NUM_GAUGE_START + 36)
#define OPLUS_IC_FUNC_GAUGE_GET_BATT_CAP		(OPLUS_IC_FUNC_NUM_GAUGE_START + 37)
#define OPLUS_IC_FUNC_GAUGE_IS_SUSPEND			(OPLUS_IC_FUNC_NUM_GAUGE_START + 38)
#define OPLUS_IC_FUNC_GAUGE_GET_BCC_PARMS		(OPLUS_IC_FUNC_NUM_GAUGE_START + 39)
#define OPLUS_IC_FUNC_GAUGE_FASTCHG_UPDATE_BCC_PARMS	(OPLUS_IC_FUNC_NUM_GAUGE_START + 40)
#define OPLUS_IC_FUNC_GAUGE_GET_PREV_BCC_PARMS		(OPLUS_IC_FUNC_NUM_GAUGE_START + 41)
#define OPLUS_IC_FUNC_GAUGE_SET_BCC_PARMS		(OPLUS_IC_FUNC_NUM_GAUGE_START + 42)
#define OPLUS_IC_FUNC_GAUGE_SET_PROTECT_CHECK		(OPLUS_IC_FUNC_NUM_GAUGE_START + 43)
#define OPLUS_IC_FUNC_GAUGE_GET_AFI_UPDATE_DONE		(OPLUS_IC_FUNC_NUM_GAUGE_START + 44)
#define OPLUS_IC_FUNC_GAUGE_CHECK_RESET			(OPLUS_IC_FUNC_NUM_GAUGE_START + 45)
#define OPLUS_IC_FUNC_GAUGE_SET_RESET			(OPLUS_IC_FUNC_NUM_GAUGE_START + 46)
#define OPLUS_IC_FUNC_GAUGE_SET_BATTERY_CURVE		(OPLUS_IC_FUNC_NUM_GAUGE_START + 47)
#define OPLUS_IC_FUNC_GAUGE_GET_SUBBOARD_TEMP		(OPLUS_IC_FUNC_NUM_GAUGE_START + 48)
#define OPLUS_IC_FUNC_GAUGE_GET_DEVICE_TYPE_FOR_BCC	(OPLUS_IC_FUNC_NUM_GAUGE_START + 49)
#define OPLUS_IC_FUNC_GAUGE_GET_DOD0			(OPLUS_IC_FUNC_NUM_GAUGE_START + 50)
#define OPLUS_IC_FUNC_GAUGE_GET_DOD0_PASSED_Q		(OPLUS_IC_FUNC_NUM_GAUGE_START + 51)
#define OPLUS_IC_FUNC_GAUGE_GET_QMAX			(OPLUS_IC_FUNC_NUM_GAUGE_START + 52)
#define OPLUS_IC_FUNC_GAUGE_GET_QMAX_PASSED_Q		(OPLUS_IC_FUNC_NUM_GAUGE_START + 53)
#define OPLUS_IC_FUNC_GAUGE_GET_DEEP_DISCHG_COUNT	(OPLUS_IC_FUNC_NUM_GAUGE_START + 54)
#define OPLUS_IC_FUNC_GAUGE_SET_DEEP_DISCHG_COUNT	(OPLUS_IC_FUNC_NUM_GAUGE_START + 55)
#define OPLUS_IC_FUNC_GAUGE_SET_DEEP_TERM_VOLT		(OPLUS_IC_FUNC_NUM_GAUGE_START + 56)
#define OPLUS_IC_FUNC_GAUGE_GET_BATTID_INFO		(OPLUS_IC_FUNC_NUM_GAUGE_START + 57)
#define OPLUS_IC_FUNC_GAUGE_GET_REG_INFO		(OPLUS_IC_FUNC_NUM_GAUGE_START + 58)
#define OPLUS_IC_FUNC_GAUGE_GET_CALIB_TIME		(OPLUS_IC_FUNC_NUM_GAUGE_START + 59)
#define OPLUS_IC_FUNC_GAUGE_GET_BATT_SN			(OPLUS_IC_FUNC_NUM_GAUGE_START + 60)
#define OPLUS_IC_FUNC_GAUGE_GET_BATTID_MATCH_INFO	(OPLUS_IC_FUNC_NUM_GAUGE_START + 61)
#define OPLUS_IC_FUNC_GAUGE_GET_DEEP_TERM_VOLT		(OPLUS_IC_FUNC_NUM_GAUGE_START + 62)
#define OPLUS_IC_FUNC_GAUGE_SET_READ_MODE		(OPLUS_IC_FUNC_NUM_GAUGE_START + 63)
#define OPLUS_IC_FUNC_GAUGE_SET_SILI_SPARE_POWER	(OPLUS_IC_FUNC_NUM_GAUGE_START + 64)
#define OPLUS_IC_FUNC_GAUGE_GET_SILI_SIMULATE_TERM_VOLT	(OPLUS_IC_FUNC_NUM_GAUGE_START + 65)
#define OPLUS_IC_FUNC_GAUGE_GET_SILI_IC_ALG_TERM_VOLT	(OPLUS_IC_FUNC_NUM_GAUGE_START + 66)
#define OPLUS_IC_FUNC_GAUGE_SET_SILI_IC_ALG_CFG		(OPLUS_IC_FUNC_NUM_GAUGE_START + 67)
#define OPLUS_IC_FUNC_GAUGE_GET_SILI_IC_ALG_DSG_ENABLE	(OPLUS_IC_FUNC_NUM_GAUGE_START + 68)
#define OPLUS_IC_FUNC_GAUGE_SET_SILI_IC_ALG_TERM_VOLT	(OPLUS_IC_FUNC_NUM_GAUGE_START + 69)
#define OPLUS_IC_FUNC_GAUGE_GET_SILI_ALG_APPLICATION_INFO	(OPLUS_IC_FUNC_NUM_GAUGE_START + 70)
#define OPLUS_IC_FUNC_GAUGE_GET_SILI_LIFETIME_STATUS	(OPLUS_IC_FUNC_NUM_GAUGE_START + 71)
#define OPLUS_IC_FUNC_GAUGE_GET_SILI_LIFETIME_INFO	(OPLUS_IC_FUNC_NUM_GAUGE_START + 72)
#define OPLUS_IC_FUNC_GAUGE_GET_MANU_DATE		(OPLUS_IC_FUNC_NUM_GAUGE_START + 73)
#define OPLUS_IC_FUNC_GAUGE_GET_FIRST_USAGE_DATE	(OPLUS_IC_FUNC_NUM_GAUGE_START + 74)
#define OPLUS_IC_FUNC_GAUGE_SET_FIRST_USAGE_DATE	(OPLUS_IC_FUNC_NUM_GAUGE_START + 75)
#define OPLUS_IC_FUNC_GAUGE_GET_UI_CC			(OPLUS_IC_FUNC_NUM_GAUGE_START + 76)
#define OPLUS_IC_FUNC_GAUGE_SET_UI_CC			(OPLUS_IC_FUNC_NUM_GAUGE_START + 77)
#define OPLUS_IC_FUNC_GAUGE_GET_UI_SOH			(OPLUS_IC_FUNC_NUM_GAUGE_START + 78)
#define OPLUS_IC_FUNC_GAUGE_SET_UI_SOH			(OPLUS_IC_FUNC_NUM_GAUGE_START + 79)
#define OPLUS_IC_FUNC_GAUGE_GET_USED_FLAG		(OPLUS_IC_FUNC_NUM_GAUGE_START + 80)
#define OPLUS_IC_FUNC_GAUGE_SET_USED_FLAG		(OPLUS_IC_FUNC_NUM_GAUGE_START + 81)
#define OPLUS_IC_FUNC_GAUGE_SET_CALIB_TIME		(OPLUS_IC_FUNC_NUM_GAUGE_START + 82)
#define OPLUS_IC_FUNC_GAUGE_GET_CHEM_ID			(OPLUS_IC_FUNC_NUM_GAUGE_START + 83)
#define OPLUS_IC_FUNC_GAUGE_SET_LAST_CC			(OPLUS_IC_FUNC_NUM_GAUGE_START + 84)
#define OPLUS_IC_FUNC_GAUGE_GET_LAST_CC			(OPLUS_IC_FUNC_NUM_GAUGE_START + 85)

/* misc */
#define OPLUS_IC_FUNC_NUM_MISC_START			500
#define OPLUS_IC_FUNC_GET_CHARGER_CYCLE			(OPLUS_IC_FUNC_NUM_MISC_START + 0)
#define OPLUS_IC_FUNC_OTG_BOOST_ENABLE			(OPLUS_IC_FUNC_NUM_MISC_START + 1)
#define OPLUS_IC_FUNC_SET_OTG_BOOST_VOL			(OPLUS_IC_FUNC_NUM_MISC_START + 2)
#define OPLUS_IC_FUNC_SET_OTG_BOOST_CURR_LIMIT		(OPLUS_IC_FUNC_NUM_MISC_START + 3)
#define OPLUS_IC_FUNC_WLS_BOOST_ENABLE			(OPLUS_IC_FUNC_NUM_MISC_START + 4)
#define OPLUS_IC_FUNC_SET_WLS_BOOST_VOL			(OPLUS_IC_FUNC_NUM_MISC_START + 5)
#define OPLUS_IC_FUNC_SET_WLS_BOOST_CURR_LIMIT		(OPLUS_IC_FUNC_NUM_MISC_START + 6)
#define OPLUS_IC_FUNC_GET_SHUTDOWN_SOC			(OPLUS_IC_FUNC_NUM_MISC_START + 7)
#define OPLUS_IC_FUNC_BACKUP_SOC			(OPLUS_IC_FUNC_NUM_MISC_START + 8)
#define OPLUS_IC_FUNC_GET_USB_TEMP			(OPLUS_IC_FUNC_NUM_MISC_START + 9)
#define OPLUS_IC_FUNC_GET_USB_TEMP_VOLT			(OPLUS_IC_FUNC_NUM_MISC_START + 10)
#define OPLUS_IC_FUNC_USB_TEMP_CHECK_IS_SUPPORT		(OPLUS_IC_FUNC_NUM_MISC_START + 11)
#define OPLUS_IC_FUNC_GET_TYPEC_MODE			(OPLUS_IC_FUNC_NUM_MISC_START + 12)
#define OPLUS_IC_FUNC_SET_TYPEC_MODE			(OPLUS_IC_FUNC_NUM_MISC_START + 13)
#define OPLUS_IC_FUNC_SET_USB_DISCHG_ENABLE		(OPLUS_IC_FUNC_NUM_MISC_START + 14)
#define OPLUS_IC_FUNC_GET_USB_DISCHG_STATUS		(OPLUS_IC_FUNC_NUM_MISC_START + 15)
#define OPLUS_IC_FUNC_SET_OTG_SWITCH_STATUS		(OPLUS_IC_FUNC_NUM_MISC_START + 16)
#define OPLUS_IC_FUNC_GET_OTG_SWITCH_STATUS		(OPLUS_IC_FUNC_NUM_MISC_START + 17)
#define OPLUS_IC_FUNC_GET_OTG_ONLINE_STATUS		(OPLUS_IC_FUNC_NUM_MISC_START + 18)
#define OPLUS_IC_FUNC_CC_DETECT_HAPPENED		(OPLUS_IC_FUNC_NUM_MISC_START + 19)
#define OPLUS_IC_FUNC_GET_OTG_ENABLE			(OPLUS_IC_FUNC_NUM_MISC_START + 20)
#define OPLUS_IC_FUNC_GET_CHARGER_VOL_MAX		(OPLUS_IC_FUNC_NUM_MISC_START + 21)
#define OPLUS_IC_FUNC_GET_CHARGER_VOL_MIN		(OPLUS_IC_FUNC_NUM_MISC_START + 22)
#define OPLUS_IC_FUNC_GET_CHARGER_CURR_MAX		(OPLUS_IC_FUNC_NUM_MISC_START + 23)
#define OPLUS_IC_FUNC_DISABLE_VBUS			(OPLUS_IC_FUNC_NUM_MISC_START + 24)
#define OPLUS_IC_FUNC_IS_OPLUS_SVID			(OPLUS_IC_FUNC_NUM_MISC_START + 25)
#define OPLUS_IC_FUNC_GET_DATA_ROLE			(OPLUS_IC_FUNC_NUM_MISC_START + 26)
#define OPLUS_IC_FUNC_BUCK_GET_USB_BTB_TEMP		(OPLUS_IC_FUNC_NUM_MISC_START + 27)
#define OPLUS_IC_FUNC_BUCK_GET_BATT_BTB_TEMP		(OPLUS_IC_FUNC_NUM_MISC_START + 28)
#define OPLUS_IC_FUNC_SET_DPDM_SWITCH_MODE		(OPLUS_IC_FUNC_NUM_MISC_START + 29)
#define OPLUS_IC_FUNC_GET_DPDM_SWITCH_MODE		(OPLUS_IC_FUNC_NUM_MISC_START + 30)
#define OPLUS_IC_FUNC_GET_TYPEC_ROLE			(OPLUS_IC_FUNC_NUM_MISC_START + 31)

/* voocphy */
#define OPLUS_IC_FUNC_NUM_VOOCPHY_START			600
#define OPLUS_IC_FUNC_VOOCPHY_ENABLE			(OPLUS_IC_FUNC_NUM_VOOCPHY_START + 0)
#define OPLUS_IC_FUNC_VOOCPHY_RESET_AGAIN		(OPLUS_IC_FUNC_NUM_VOOCPHY_START + 1)
#define OPLUS_IC_FUNC_VOOCPHY_SET_CURR_LEVEL		(OPLUS_IC_FUNC_NUM_VOOCPHY_START + 2)
#define OPLUS_IC_FUNC_VOOCPHY_SET_MATCH_TEMP		(OPLUS_IC_FUNC_NUM_VOOCPHY_START + 3)
#define OPLUS_IC_FUNC_VOOCPHY_SET_PDQC_CONFIG		(OPLUS_IC_FUNC_NUM_VOOCPHY_START + 4)
#define OPLUS_IC_FUNC_VOOCPHY_GET_CP_VBAT		(OPLUS_IC_FUNC_NUM_VOOCPHY_START + 5)
#define OPLUS_IC_FUNC_VOOCPHY_SET_CHG_AUTO_MODE		(OPLUS_IC_FUNC_NUM_VOOCPHY_START + 6)
#define OPLUS_IC_FUNC_VOOCPHY_GET_BCC_MAX_CURR		(OPLUS_IC_FUNC_NUM_VOOCPHY_START + 7)
#define OPLUS_IC_FUNC_VOOCPHY_GET_BCC_MIN_CURR		(OPLUS_IC_FUNC_NUM_VOOCPHY_START + 8)
#define OPLUS_IC_FUNC_VOOCPHY_GET_BCC_EXIT_CURR			(OPLUS_IC_FUNC_NUM_VOOCPHY_START + 9)
#define OPLUS_IC_FUNC_VOOCPHY_GET_FASTCHG_ING		(OPLUS_IC_FUNC_NUM_VOOCPHY_START + 10)
#define OPLUS_IC_FUNC_VOOCPHY_GET_BCC_TEMP_RANGE		(OPLUS_IC_FUNC_NUM_VOOCPHY_START + 11)
#define OPLUS_IC_FUNC_VOOCPHY_SET_BCC_CURR		(OPLUS_IC_FUNC_NUM_VOOCPHY_START + 12)
#define OPLUS_IC_FUNC_VOOCPHY_GET_CURVE_CURR			(OPLUS_IC_FUNC_NUM_VOOCPHY_START + 13)
#define OPLUS_IC_FUNC_VOOCPHY_GET_CURVE_CURR_T		(OPLUS_IC_FUNC_NUM_VOOCPHY_START + 14)
#define OPLUS_IC_FUNC_VOOCPHY_SET_UCP_TIME		(OPLUS_IC_FUNC_NUM_VOOCPHY_START + 15)
#define OPLUS_IC_FUNC_VOOCPHY_SET_UCP_TIME_T		(OPLUS_IC_FUNC_NUM_VOOCPHY_START + 16)

/* ufcs */
#define OPLUS_IC_FUNC_NUM_UFCS_START			900
#define OPLUS_IC_FUNC_UFCS_HANDSHAKE			(OPLUS_IC_FUNC_NUM_UFCS_START + 0)
#define OPLUS_IC_FUNC_UFCS_PDO_SET			(OPLUS_IC_FUNC_NUM_UFCS_START + 1)
#define OPLUS_IC_FUNC_UFCS_HARD_RESET			(OPLUS_IC_FUNC_NUM_UFCS_START + 2)
#define OPLUS_IC_FUNC_UFCS_EXIT				(OPLUS_IC_FUNC_NUM_UFCS_START + 3)
#define OPLUS_IC_FUNC_UFCS_CONFIG_WD			(OPLUS_IC_FUNC_NUM_UFCS_START + 4)
#define OPLUS_IC_FUNC_UFCS_GET_DEV_INFO			(OPLUS_IC_FUNC_NUM_UFCS_START + 5)
#define OPLUS_IC_FUNC_UFCS_GET_ERR_INFO			(OPLUS_IC_FUNC_NUM_UFCS_START + 6)
#define OPLUS_IC_FUNC_UFCS_GET_SRC_INFO			(OPLUS_IC_FUNC_NUM_UFCS_START + 7)
#define OPLUS_IC_FUNC_UFCS_GET_CABLE_INFO		(OPLUS_IC_FUNC_NUM_UFCS_START + 8)
#define OPLUS_IC_FUNC_UFCS_GET_PDO_INFO			(OPLUS_IC_FUNC_NUM_UFCS_START + 9)
#define OPLUS_IC_FUNC_UFCS_VERIFY_ADAPTER		(OPLUS_IC_FUNC_NUM_UFCS_START + 10)
#define OPLUS_IC_FUNC_UFCS_GET_POWER_CHANGE_INFO	(OPLUS_IC_FUNC_NUM_UFCS_START + 11)
#define OPLUS_IC_FUNC_UFCS_GET_EMARK_INFO		(OPLUS_IC_FUNC_NUM_UFCS_START + 12)
#define OPLUS_IC_FUNC_UFCS_GET_POWER_INFO_EXT		(OPLUS_IC_FUNC_NUM_UFCS_START + 13)
#define OPLUS_IC_FUNC_UFCS_IS_TEST_MODE			(OPLUS_IC_FUNC_NUM_UFCS_START + 14)

#define VOOC_VERSION_DEFAULT		0x0
#define VOOC_VERSION_1_0		0x1
#define VOOC_VERSION_2_0		0x2
#define VOOC_VERSION_3_0		0x3
#define VOOC_VERSION_4_0		0x4
#define VOOC_VERSION_5_0		0x5 /* optimize into fastchging time */

#define CURR_LIMIT_VOOC_3_6A_SVOOC_2_5A	0x01
#define CURR_LIMIT_VOOC_2_5A_SVOOC_2_0A	0x02
#define CURR_LIMIT_VOOC_3_0A_SVOOC_3_0A	0x03
#define CURR_LIMIT_VOOC_4_0A_SVOOC_4_0A	0x04
#define CURR_LIMIT_VOOC_5_0A_SVOOC_5_0A	0x05
#define CURR_LIMIT_VOOC_6_0A_SVOOC_6_5A	0x06

#define CURR_LIMIT_7BIT_1_0A		0x01
#define CURR_LIMIT_7BIT_1_5A		0x02
#define CURR_LIMIT_7BIT_2_0A		0x03
#define CURR_LIMIT_7BIT_2_5A		0x04
#define CURR_LIMIT_7BIT_3_0A		0x05
#define CURR_LIMIT_7BIT_3_5A		0x06
#define CURR_LIMIT_7BIT_4_0A		0x07
#define CURR_LIMIT_7BIT_4_5A		0x08
#define CURR_LIMIT_7BIT_5_0A		0x09
#define CURR_LIMIT_7BIT_5_5A		0x0a
#define CURR_LIMIT_7BIT_6_0A		0x0b
#define CURR_LIMIT_7BIT_6_3A		0x0c
#define CURR_LIMIT_7BIT_6_5A		0x0d
#define CURR_LIMIT_7BIT_7_0A		0x0e
#define CURR_LIMIT_7BIT_7_5A		0x0f
#define CURR_LIMIT_7BIT_8_0A		0x10
#define CURR_LIMIT_7BIT_8_5A		0x11
#define CURR_LIMIT_7BIT_9_0A		0x12
#define CURR_LIMIT_7BIT_9_5A		0x13
#define CURR_LIMIT_7BIT_10_0A		0x14
#define CURR_LIMIT_7BIT_10_5A		0x15
#define CURR_LIMIT_7BIT_11_0A		0x16
#define CURR_LIMIT_7BIT_11_5A		0x17
#define CURR_LIMIT_7BIT_12_0A		0x18
#define CURR_LIMIT_7BIT_12_5A		0x19

#define CP_CURR_LIMIT_7BIT_2_0A		0x01
#define CP_CURR_LIMIT_7BIT_2_1A		0x02
#define CP_CURR_LIMIT_7BIT_2_4A		0x03
#define CP_CURR_LIMIT_7BIT_3_0A		0x04
#define CP_CURR_LIMIT_7BIT_3_4A		0x05
#define CP_CURR_LIMIT_7BIT_4_0A		0x06
#define CP_CURR_LIMIT_7BIT_4_4A		0x07
#define CP_CURR_LIMIT_7BIT_5_0A		0x08
#define CP_CURR_LIMIT_7BIT_5_4A		0x09
#define CP_CURR_LIMIT_7BIT_6_0A		0x0a
#define CP_CURR_LIMIT_7BIT_6_4A		0x0b
#define CP_CURR_LIMIT_7BIT_7_0A		0x0c
#define CP_CURR_LIMIT_7BIT_7_4A		0x0d
#define CP_CURR_LIMIT_7BIT_8_0A		0x0e
#define CP_CURR_LIMIT_7BIT_9_0A		0x0f
#define CP_CURR_LIMIT_7BIT_10_0A	0x10
#define CP_CURR_LIMIT_7BIT_11_0A	0x11
#define CP_CURR_LIMIT_7BIT_12_0A	0x12
#define CP_CURR_LIMIT_7BIT_12_6A	0x13
#define CP_CURR_LIMIT_7BIT_13_0A	0x14
#define CP_CURR_LIMIT_7BIT_14_0A	0x15
#define CP_CURR_LIMIT_7BIT_15_0A	0x16
#define CP_CURR_LIMIT_7BIT_16_0A	0x17
#define CP_CURR_LIMIT_7BIT_17_0A	0x18
#define CP_CURR_LIMIT_7BIT_18_0A	0x19
#define CP_CURR_LIMIT_7BIT_19_0A	0x1a
#define CP_CURR_LIMIT_7BIT_20_0A	0x1b

/* vooc current table type */
#define VOOC_CURR_TABLE_OLD_1_0		0
#define VOOC_CURR_TABLE_1_0		1
#define VOOC_CURR_TABLE_2_0		2
#define VOOC_CP_CURR_TABLE		3

/* VADC scale function index */
#define OPLUS_ADC_SCALE_DEFAULT				0x0
#define OPLUS_ADC_SCALE_THERM_100K_PULLUP		0x1
#define OPLUS_ADC_SCALE_PMIC_THERM			0x2
#define OPLUS_ADC_SCALE_XOTHERM				0x3
#define OPLUS_ADC_SCALE_PMI_CHG_TEMP			0x4
#define OPLUS_ADC_SCALE_HW_CALIB_DEFAULT		0x5
#define OPLUS_ADC_SCALE_HW_CALIB_THERM_100K_PULLUP	0x6
#define OPLUS_ADC_SCALE_HW_CALIB_XOTHERM		0x7
#define OPLUS_ADC_SCALE_HW_CALIB_PMIC_THERM		0x8
#define OPLUS_ADC_SCALE_HW_CALIB_CUR			0x9
#define OPLUS_ADC_SCALE_HW_CALIB_PM5_CHG_TEMP		0xA
#define OPLUS_ADC_SCALE_HW_CALIB_PM5_SMB_TEMP		0xB
#define OPLUS_ADC_SCALE_HW_CALIB_BATT_THERM_100K	0xC
#define OPLUS_ADC_SCALE_HW_CALIB_BATT_THERM_30K		0xD
#define OPLUS_ADC_SCALE_HW_CALIB_BATT_THERM_400K	0xE
#define OPLUS_ADC_SCALE_HW_CALIB_PM5_SMB1398_TEMP	0xF

/* strategy */
#define OPLUS_STRATEGY_USE_BATT_TEMP	0
#define OPLUS_STRATEGY_USE_SHELL_TEMP	1

#define AGING_FFC_NOT_SUPPORT		0
#define AGING_FFC_V1			1

/* charge protocol arbitration */
#define CHG_PROTOCOL_BC12		0
#define CHG_PROTOCOL_PD			1
#define CHG_PROTOCOL_PPS		2
#define CHG_PROTOCOL_VOOC		3
#define CHG_PROTOCOL_UFCS		4
#define CHG_PROTOCOL_QC			5

#define CP_WORK_MODE_UNKNOWN		0
#define CP_WORK_MODE_AUTO		1
#define CP_WORK_MODE_BYPASS		2
#define CP_WORK_MODE_2_TO_1		3
#define CP_WORK_MODE_3_TO_1		4
#define CP_WORK_MODE_4_TO_1		5
#define CP_WORK_MODE_1_TO_2		6
#define CP_WORK_MODE_1_TO_3		7
#define CP_WORK_MODE_1_TO_4		8

#define TRACK_NO_VOOCPHY			0
#define TRACK_ADSP_VOOCPHY			1
#define TRACK_AP_SINGLE_CP_VOOCPHY		2
#define TRACK_AP_DUAL_CP_VOOCPHY		3
#define TRACK_MCU_VOOCPHY			4

#define SILI_OCV_HYSTERESIS		0
#define SILI_OCV_AGING_OFFSET		1
#define SILI_DYNAMIC_DSG_CTRL		2
#define SILI_STATIC_DSG_CTRL		3
#define SILI_MONITOR_MODE		4

#define OPLUS_CHG_SPEC_VER_UNKNOW	0
#define OPLUS_CHG_SPEC_VER_V3P6		1
#define OPLUS_CHG_SPEC_VER_V3P7		2

/* ufcs current table type */
#define UFCS_CURR_BIDIRECT_TABLE		0
#define UFCS_CURR_CP_TABLE			1
//endif OPLUS_FEATURE_CHG_BASIC

//endif OPLUS_FEATURE_CHG_BASIC
