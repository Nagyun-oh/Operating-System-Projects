obj-m += os_ftracehooking.o 

KDIR := /usr/src/linux-5.4.282
PWD := $(CURDIR) 

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

