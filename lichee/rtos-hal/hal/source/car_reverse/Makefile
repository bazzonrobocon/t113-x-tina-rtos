INC_PATH := -I drivers/rtos-hal/include/source/hal/vin
INC_PATH += -I $(BASE)/components/common/thirdparty/openamp/include
INC_PATH += -I $(BASE)/components/common/thirdparty/rtt_memheap
INC_PATH += -I drivers/rtos-hal/hal/source/g2d_rcq/
INC_PATH += -I drivers/rtos-hal/hal/source/car_reverse/include/libcedarc
INC_PATH += -I $(BASE)/components/common/aw/multimedia/lib_riscv

obj-y += preview_display.o fastboot_com.o

obj-$(CONFIG_DRIVERS_VIDEO_EARLY) += fastboot_video.o video_buffer_pool.o

obj-$(CONFIG_VIDEO_EARLY_RV_DECODE_SUPPORT) += rv_decode.o rawStreamParser.o

obj-$(CONFIG_DRIVERS_CAR_REVERSE) += car_reverse.o video_source.o buffer_pool.o \
										preview_enhancer.o preview_rotator.o \
										car_reverse_amp.o car_reverse_test.o

subdir-ccflags-y += $(INC_PATH)
