###
# Copyright (C) 2018 Fourier Semiconductor Inc. All rights reserved.
#

subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/common

#EXTRA_CFLAGS += -DDEBUG -DFSM_DEBUG -DFSM_DEBUG_I2C
EXTRA_CFLAGS += -Wall -Werror
# MODFLAGS = -fno-pic
# CFLAGS_MODULE = $(MODFLAGS)
# AFLAGS_MODULE = $(MODFLAGS)

EXTRA_CFLAGS += -DCONFIG_FSM_MISC
EXTRA_CFLAGS += -DCONFIG_FSM_SYSFS
EXTRA_CFLAGS += -DCONFIG_FSM_CODEC
EXTRA_CFLAGS += -DCONFIG_FSM_FIRMWARE
EXTRA_CFLAGS += -DCONFIG_FSM_I2C
EXTRA_CFLAGS += -DCONFIG_FSM_STEREO
# EXTRA_CFLAGS += -DCONFIG_FSM_STUB
EXTRA_CFLAGS += -DCONFIG_FSM_FS1801

snd-soc-fsm-objs := fsm_core.o
snd-soc-fsm-objs += fs1801.o
snd-soc-fsm-objs += fsm_i2c.o

obj-$(CONFIG_SND_SOC_FS16XX) += snd-soc-fsm.o
