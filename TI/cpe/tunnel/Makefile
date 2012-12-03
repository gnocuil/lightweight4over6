obj-m += lw4over6.o

all: lw4over6.c ioctl
	echo $(PWD)
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

ioctl : ioctl.c
	gcc -o ioctl ioctl.c

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f ioctl


