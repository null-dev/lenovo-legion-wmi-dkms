obj-m += lenovo-legion-wmi.o
module-name := lenovo-legion-wmi

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(module-name) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(module-name) clean
