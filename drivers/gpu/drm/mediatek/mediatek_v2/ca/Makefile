#
#HDCP crypto ca
#

ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
include $(DEVICE_MODULES_PATH)/drivers/tee/gud/Makefile.include
endif
ifeq ($(CONFIG_MICROTRUST_TEE_SUPPORT),y)
include $(DEVICE_MODULES_PATH)/drivers/tee/teei/Makefile.include
endif

obj-y += tlcDpHdcp.o

#ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),m)
#include $(srctree)/drivers/tee/gud/Makefile.include
#endif

#obj-m += tlcDpHdcp.o
#ccflags-y += -DHDCP_SETKEY_FROM_KERNEL
