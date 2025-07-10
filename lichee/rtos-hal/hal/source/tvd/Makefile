INC_PATH := -I drivers/rtos-hal/include/source/hal/vin
INC_PATH += -I $(BASE)/components/common/thirdparty/openamp/include
INC_PATH += -I $(BASE)/components/common/thirdparty/rtt_memheap

obj-y += tvd.o
obj-y += bsp_tvd.o
obj-y += tvd_rpmsg.o
subdir-ccflags-y += $(INC_PATH)