KERN_DIR := /home/zhanghangying/linux/kernel/linux-xlnx-xlnx_rebase_v5.4_2020.2

obj-m := scanner.o

all:
	make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -C $(KERN_DIR) M=`pwd` modules

clean:
	make -C $(KERN_DIR) M=`pwd` clean
