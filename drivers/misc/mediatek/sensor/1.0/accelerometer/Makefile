# In case the platform does NOT support this type of sensors
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sensor/1.0/hwmon/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sensor/1.0/include

obj-$(CONFIG_MTK_SENSOR_SUPPORT)   +=  accel_common.o
accel_common-$(CONFIG_MTK_SENSOR_SUPPORT) := accel.o accel_factory.o

obj-$(CONFIG_MTK_ACCELHUB) += accelhub/
#obj-$(CONFIG_MTK_BMI160_I2C)   += bmi160-i2c/
#obj-$(CONFIG_MTK_LIS3DH_NEW) += lis3dh/
obj-$(CONFIG_MTK_MC3410_I2C) += mc3410-i2c/
#obj-$(CONFIG_MTK_MC3433) += mc3433/
#obj-$(CONFIG_MTK_MPU6050G) += mpu6050g/
