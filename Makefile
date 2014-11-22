ccflags-y += -std=gnu99
ccflags-y += -Wdeclaration-after-statement
ccflags-y += -lwiringPi
obj-m := us_service.o
KDIR :=/lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
