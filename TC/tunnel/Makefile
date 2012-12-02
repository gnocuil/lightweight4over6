obj-m += lw4over6.o
lw4over6-objs += mapping.o tunnel.o 

all: ioctl
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f ioctl

ioctl: tunnel.h main.c
	gcc -o ioctl main.c
	
