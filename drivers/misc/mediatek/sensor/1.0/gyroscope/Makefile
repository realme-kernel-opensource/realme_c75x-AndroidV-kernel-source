# In case the platform does NOT support this type of sensors
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sensor/1.0/hwmon/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sensor/1.0/include
obj-$(CONFIG_MTK_SENSOR_SUPPORT)   +=  gyro_common.o
gyro_common-$(CONFIG_MTK_SENSOR_SUPPORT) := gyroscope.o gyro_factory.o

obj-$(CONFIG_MTK_GYROHUB) += gyrohub/
#obj-$(CONFIG_MTK_BMI160GY_I2C) += bmi160gy-i2c/
ifeq ($(CONFIG_MTK_ITG1010),y)
obj-$(CONFIG_MTK_ITG1010) += ITG1010/
endif
#obj-$(CONFIG_MTK_MPU6050GY) += mpu6050gy/
