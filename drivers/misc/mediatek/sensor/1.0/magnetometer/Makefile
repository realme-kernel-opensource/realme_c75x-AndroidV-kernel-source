# In case the platform does NOT support this type of sensors
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sensor/1.0/hwmon/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sensor/1.0/accelerometer/inc
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sensor/1.0/magnetometer/inc
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sensor/1.0/include
obj-$(CONFIG_MTK_SENSOR_SUPPORT)   +=  mag_common.o
mag_common-$(CONFIG_MTK_SENSOR_SUPPORT)   +=  mag.o mag_factory.o

obj-$(CONFIG_MTK_MAGHUB) += maghub/
#obj-$(CONFIG_MTK_AKM09915_NEW) += akm09915/
ifeq ($(CONFIG_MTK_AKM09918),y)
obj-$(CONFIG_MTK_AKM09918) += akm09918/
endif
