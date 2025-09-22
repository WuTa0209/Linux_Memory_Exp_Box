obj-m += try_to_shrink_lruvec_kprobe.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
alloc_test:
	gcc alloc.c -o alloc
	sudo systemd-run --user --scope -p MemoryMax=300M ./alloc