obj-m += lw4over6.o
curdir := $(PWD)$(subdir)

all: lw4over6.c ioctl
	make -C /lib/modules/$(shell uname -r)/build M=$(curdir) modules

ioctl : ioctl.c
	gcc -o ioctl ioctl.c

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(curdir) clean
	rm -f ioctl


