
APP = led_output

obj-m:= spidev.o sensor_driver.o

ARCH=x86

CC=/opt/iot-devkit/1.7.2/sysroots/x86_64-pokysdk-linux/usr/bin/i586-poky-linux/i586-poky-linux-gcc

CROSS_COMPILE=/opt/iot-devkit/1.7.2/sysroots/x86_64-pokysdk-linux/usr/bin/i586-poky-linux/i586-poky-linux-

SROOT=/opt/iot-devkit/1.7.2/sysroots/i586-poky-linux

ifeq ($(TEST_TARGET), Galileo)
	CC = $(CROSS_COMPILE)gcc
	MAKE = make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE)
	KDIR = $(SROOT)/usr/src/kernel/
else
	CC = gcc
	MAKE = make
	KDIR = /lib/modules/$(shell uname -r)/build
endif


all :
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	$(CC) -o $(APP) main.c -pthread -lm

clean:
	rm -f *.ko
	rm -f *.o
	rm -f Module.symvers
	rm -f modules.order
	rm -f *.mod.c
	rm -rf .tmp_versions
	rm -f *.mod.c
	rm -f *.mod.o
	rm -f \.*.cmd
	rm -f Module.markers
	rm -f $(APP) 
