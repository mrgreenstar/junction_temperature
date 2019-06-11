obj-m += src/junction_temp.o
subdir-ccflags-y += -I$(srctree)/drivers/fpga/stcmtk/grif
kernel-path = ~/linux-fslc

all:
	make -C $(kernel-path) M=$(PWD) modules