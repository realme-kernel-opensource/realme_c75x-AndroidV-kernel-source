obj-$(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) += sspm.o

sspm-objs += sspm_emi_mpu.o
sspm-objs += sspm_helper.o
sspm-objs += sspm_ipi_table.o
sspm-objs += sspm_ipi_timeout_cb.o
sspm-objs += sspm_logger_impl.o
sspm-objs += sspm_plt.o
sspm-objs += sspm_reservedmem.o
sspm-objs += sspm_sysfs.o
sspm-objs += sspm_timesync.o

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/v2
#ccflags-y += -DDEBUG

#SSPM_IPI_TEST=y
# obj-$(SSPM_IPI_TEST) += sspm_ipi_test.o
