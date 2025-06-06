/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef __VCP_HELPER_H__
#define __VCP_HELPER_H__

#include <linux/arm-smccc.h>
#include <linux/notifier.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include "vcp_reg.h"
#include "vcp_feature_define.h"
#include "vcp_status.h"
#include "vcp.h"

#define ROUNDUP(a, b)		        (((a) + ((b)-1)) & ~((b)-1))

/* vcp dram shared memory */
#define DRAM_VDEC_VSI_BUF_LEN		(480 * 1024)
#define DRAM_VENC_VSI_BUF_LEN		(32 * 1024)
#define DRAM_LOG_BUF_LEN		(1 * 1024 * 1024)

/* vcp config reg. definition*/
#define VCP_TCM_SIZE		(vcpreg.total_tcmsize)
#define VCP_A_TCM_SIZE		(vcpreg.vcp_tcmsize)
#define VCP_TCM			(vcpreg.sram)
#define VCP_REGION_INFO_OFFSET	0x4
#define VCP_RTOS_START		(0x800)

#define VCP_DRAM_MAPSIZE	(0x100000)
#define VCP_CLK_FMETER

/* vcp dvfs return status flag */
#define SET_PLL_FAIL		(1)
#define SET_PMIC_VOLT_FAIL	(2)

#define mt_reg_sync_writel(v, a) \
	do {    \
		__raw_writel((v), (void __force __iomem *)((a)));   \
		mb();  /*make sure register access in order */ \
	} while (0)

/* This structre need to sync with VCP-side */
struct VCP_IRQ_AST_INFO {
	unsigned int vcp_irq_ast_time;
	unsigned int vcp_irq_ast_pc_s;
	unsigned int vcp_irq_ast_pc_e;
	unsigned int vcp_irq_ast_lr_s;
	unsigned int vcp_irq_ast_lr_e;
	unsigned int vcp_irq_ast_irqd;
};

/* reset ID */
#define VCP_ALL_ENABLE	0x00
#define VCP_ALL_REBOOT	0x01
#define VCP_ALL_SUSPEND	0x10

#define VCP_PACK_IOVA(addr)     ((uint32_t)((addr) | (((addr) >> 32) & 0xF)))
#define VCP_UNPACK_IOVA(addr)   \
	((uint64_t)(addr & 0xFFFFFFF0) | (((uint64_t)(addr) & 0xF) << 32))

/* vcp semaphore definition*/
enum SEMAPHORE_FLAG {
	SEMAPHORE_CLK_CFG_5 = 0,
	SEMAPHORE_PTP,
	SEMAPHORE_I2C0,
	SEMAPHORE_I2C1,
	SEMAPHORE_TOUCH,
	SEMAPHORE_APDMA,
	SEMAPHORE_SENSOR,
	SEMAPHORE_VCP_A_AWAKE,
	SEMAPHORE_VCP_B_AWAKE,
	NR_FLAG = 9,
};

/* vcp reset status */
enum VCP_RESET_STATUS {
	RESET_STATUS_STOP = 0,
	RESET_STATUS_START = 1,
};

/* vcp reset status */
enum VCP_RESET_TYPE {
	RESET_TYPE_WDT = 0,
	RESET_TYPE_AWAKE = 1,
	RESET_TYPE_CMD = 2,
	RESET_TYPE_TIMEOUT = 3,
};

enum mtk_tinysys_vcp_kernel_op {
	MTK_TINYSYS_VCP_KERNEL_OP_DUMP_START = 0,
	MTK_TINYSYS_VCP_KERNEL_OP_DUMP_POLLING,
	MTK_TINYSYS_VCP_KERNEL_OP_RESET_SET,
	MTK_TINYSYS_VCP_KERNEL_OP_RESET_RELEASE,
	MTK_TINYSYS_VCP_KERNEL_OP_SET_SRAMLOGBUF_INFO,
	MTK_TINYSYS_VCP_KERNEL_OP_NUM,
};

struct slp_ctrl_data {
	uint32_t feature;
	uint32_t cmd;
};

enum {
	SLP_WAKE_LOCK = 0,
	SLP_WAKE_UNLOCK,
	SLP_STATUS_DBG,
};

struct vcp_regs {
	void __iomem *sram;
	void __iomem *cfg;
	void __iomem *l1cctrl;
	void __iomem *cfg_core0;
	void __iomem *cfg_intc;
	void __iomem *cfg_pwr;
	void __iomem *cfg_core1;
	void __iomem *cfg_sec;
	void __iomem *cfg_mmu;
	void __iomem *bus_debug;
	void __iomem *bus_tracker;
	void __iomem *bus_prot;
	void __iomem *cfg_sec_gpr;
	void __iomem *infra_sram;
	void __iomem *infra_cfg;
	void __iomem *infra_cfg_core0;
	void __iomem *infra_cfg_intc;
	void __iomem *infra_cfg_pwr;
	void __iomem *infra_cfg_core1;
	void __iomem *infra_cfg_sec;
	void __iomem *infra_bus_debug;
	void __iomem *infra_bus_tracker;
	void __iomem *infra_bus_prot;
	void __iomem *infra_cfg_sec_gpr;
	void __iomem *vcp_pwr_ack;
	int irq0;
	int irq1;
	unsigned int total_tcmsize;
	unsigned int cfgregsize;
	unsigned int vcp_tcmsize;
	unsigned int core_nums;
	unsigned int twohart;
	unsigned int fmeter_ck;
	unsigned int fmeter_type;
	unsigned int secure_dump;
	unsigned int bus_debug_num_ports;
};

struct vcp_ipi_profile {
	unsigned int type;
	unsigned int ipi_time_h;
	unsigned int ipi_time_l;
};

/* vcp work struct definition*/
struct vcp_work_struct {
	struct work_struct work;
	unsigned int flags;
	unsigned int id;
};

struct vcp_reserve_mblock {
	enum vcp_reserve_mem_id_t num;
	u64 start_phys;
	u64 start_virt;
	u32 size;
};

struct vcp_region_info_st {
	uint32_t ap_loader_start;
	uint32_t ap_loader_size;
	uint32_t ap_firmware_start;
	uint32_t ap_firmware_size;
	uint32_t ap_dram_start;
	uint32_t ap_dram_size;
	uint32_t ap_dram_backup_start;
	/*	This is the size of the structure.
	 *	It can act as a version number if entries can only be
	 *	added to (not deleted from) the structure.
	 *	It should be the first entry of the structure, but for
	 *	compatibility reason, it is appended here.
	 */
	uint32_t struct_size;
	uint32_t vcp_log_thru_ap_uart;
	uint32_t TaskContext_ptr;
	uint32_t vcpctl;
	uint32_t regdump_start;
	uint32_t regdump_size;
	uint32_t ap_params_start;
	uint32_t sramlog_buf_offset;
	uint32_t sramlog_end_idx_offset;
	uint32_t sramlog_buf_maxlen;
	uint32_t ap_loader_start_PA;
	uint32_t coredump_offset;
	uint32_t coredump_dram_offset;
};

struct vcp_mminfra_on_off_st {
	mminfra_pwr_ptr mminfra_on;
	mminfra_pwr_ptr mminfra_off;
	int mminfra_ref;
};

/* vcp helper varriable */
extern bool driver_init_done;
extern bool is_suspending;
extern unsigned int infra_vcp_support;

extern struct vcp_regs vcpreg;
extern const struct file_operations vcp_A_log_file_ops;
#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_DEBUG_SUPPORT)
/* vcp device attribute */
extern struct device_attribute dev_attr_vcp_A_mobile_log_UT;
extern struct device_attribute dev_attr_vcp_A_logger_wakeup_AP;
extern const struct file_operations vcp_A_log_file_ops;

extern struct vcp_regs vcpreg;
extern struct device_attribute dev_attr_vcp_mobile_log;
extern struct device_attribute dev_attr_vcp_A_get_last_log;
extern struct device_attribute dev_attr_vcp_A_status;
extern struct device_attribute dev_attr_log_filter;
#endif  //  CONFIG_MTK_TINYSYS_VCP_DEBUG_SUPPORT
extern struct bin_attribute bin_attr_vcp_dump;
extern char *halt_user;

/* vcp loggger */
extern int vcp_logger_init(phys_addr_t start, phys_addr_t limit);
extern void vcp_logger_uninit(void);
extern int vcp_logger_wakeup_handler(unsigned int id,
	void *prdata, void *data, unsigned int len);

extern void vcp_logger_stop(void);
extern void vcp_logger_cleanup(void);
extern unsigned int vcp_dbg_log;

/* vcp exception */
int vcp_excep_init(void);
void vcp_ram_dump_init(void);
void vcp_excep_cleanup(void);
void vcp_wait_core_stop_timeout(int mmup_enable);

/* vcp irq */
extern irqreturn_t vcp_A_irq_handler(int irq, void *dev_id);
extern void vcp_A_irq_init(void);
extern void wait_vcp_ready_to_reboot(void);

/* vcp helper */
unsigned int is_vcp_ready_by_coreid(enum vcp_core_id id);
extern void vcp_schedule_work(struct vcp_work_struct *vcp_ws);
extern void vcp_schedule_logger_work(struct vcp_work_struct *vcp_ws);

extern void memcpy_to_vcp(void __iomem *trg,
		const void *src, int size);
extern void memcpy_from_vcp(void *trg, const void __iomem *src,
		int size);
extern int reset_vcp(int reset);
extern struct device *vcp_get_io_device(enum VCP_IOMMU_DEV io_num);

extern int vcp_turn_mminfra_on(void);
extern int vcp_turn_mminfra_off(void);

extern int vcp_check_resource(void);
void set_vcp_mpu(void);
extern unsigned int vcp_cmd(enum feature_id id, enum vcp_cmd_id cmd_id, char *user);
extern void trigger_vcp_halt(enum vcp_core_id id, char *user, bool vote_mminfra);

/* APIs for reserved memory */
extern void __iomem *vcp_get_sram_virt(void);
extern phys_addr_t vcp_get_reserve_mem_phys(enum vcp_reserve_mem_id_t id);
extern phys_addr_t vcp_get_reserve_mem_virt(enum vcp_reserve_mem_id_t id);
extern phys_addr_t vcp_get_reserve_mem_size(enum vcp_reserve_mem_id_t id);
extern phys_addr_t vcp_mem_base_phys;
extern phys_addr_t vcp_mem_base_virt;
extern phys_addr_t vcp_mem_size;

/* APIs for registering function of features */
extern int vcp_register_feature(enum feature_id id);
extern int vcp_deregister_feature(enum feature_id id);

/* APIs to lock vcp and make vcp awaken */
extern int vcp_awake_lock(void *_vcp_id);
extern int vcp_awake_unlock(void *_vcp_id);

extern unsigned int is_vcp_ready(enum feature_id id);
extern unsigned int get_vcp_generation(void);
extern atomic_t vcp_reset_status;
extern spinlock_t vcp_awake_spinlock;
extern struct mutex  vcp_pw_clk_mutex;
extern struct tasklet_struct vcp_A_irq0_tasklet;
extern struct tasklet_struct vcp_A_irq1_tasklet;

#if IS_ENABLED(CONFIG_MTK_GIC_V3_EXT)
extern u32 mt_irq_get_pending(unsigned int irq);
#endif
void dump_vcp_irq_status(void);
extern void mt_irq_dump_status(unsigned int irq);

extern struct mtk_ipi_device *get_ipidev(enum feature_id id);
extern int mmup_enable_count(void);
extern bool is_mmup_enable(void);
extern unsigned int is_vcp_suspending(void);
extern unsigned int is_vcp_ao(void);
extern int vcp_register_mminfra_cb(mminfra_pwr_ptr fpt_on, mminfra_pwr_ptr fpt_off,
	mminfra_dump_ptr mminfra_dump_func);

/*extern vcp notify*/
extern void vcp_send_reset_wq(enum VCP_RESET_TYPE type);
extern void vcp_extern_notify(enum VCP_NOTIFY_EVENT notify_status);
extern void vcp_A_register_notify(enum feature_id id, struct notifier_block *nb);
extern void vcp_A_unregister_notify(enum feature_id id, struct notifier_block *nb);

extern void vcp_status_set(unsigned int value);
extern void vcp_logger_init_set(unsigned int value);
extern unsigned int vcp_set_reset_status(void);
extern void vcp_enable_sram(void);
extern void vcp_reset_awake_counts(void);
extern void vcp_awake_init(void);
extern int vcp_enable_pm_clk(enum feature_id id);
extern int vcp_disable_pm_clk(enum feature_id id);

#if VCP_RECOVERY_SUPPORT
extern unsigned int vcp_reset_by_cmd;
extern struct vcp_region_info_st vcp_region_info_copy;
extern struct vcp_region_info_st *vcp_region_info;
extern void __iomem *vcp_ap_dram_virt;
extern void __iomem *vcp_loader_virt;
extern void __iomem *vcp_regdump_virt;
#endif

extern mminfra_dump_ptr mminfra_debug_dump;
extern bool vcp_ao;

#endif
