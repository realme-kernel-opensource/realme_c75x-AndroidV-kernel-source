#
# Makefile for trusty components
#

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/trusty/include
ifneq ($(findstring $(CONFIG_TRUSTONIC_TEE_SUPPORT), y m),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/tee/gud/$(CONFIG_TRUSTONIC_TEE_VERSION)/MobiCoreDriver/public/
endif

obj-$(CONFIG_TRUSTY) += ise-trusty.o
ise-trusty-$(CONFIG_TRUSTY) += trusty.o trusty-mem.o trusty-shm.o
ise-trusty-$(CONFIG_TRUSTONIC_TEE_SUPPORT) += trusty-dci.o

obj-$(CONFIG_TRUSTY_LOG) += ise-trusty-log.o
ise-trusty-log-$(CONFIG_TRUSTY_LOG) += trusty-log.o

obj-$(CONFIG_TRUSTY_VIRTIO_IPC) += ise-trusty-ipc.o
ise-trusty-ipc-$(CONFIG_TRUSTY_VIRTIO_IPC) += trusty-ipc.o

obj-$(CONFIG_TRUSTY_VIRTIO) += ise-trusty-virtio.o
ise-trusty-virtio-$(CONFIG_TRUSTY_VIRTIO) += trusty-virtio.o
