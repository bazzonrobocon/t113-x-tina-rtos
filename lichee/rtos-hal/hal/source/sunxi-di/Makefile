ifeq ($(CONFIG_DRIVERS_DI_V1XX),y)
sub_dir = drv_div1xx
obj-y += drv_div1xx/di110.o
endif

ifeq ($(CONFIG_DRIVERS_DI_V2X),y)
sub_dir = drv_div2x
endif

ifeq ($(CONFIG_DRIVERS_DI_V3XX),y)
sub_dir = drv_div3xx
obj-y += drv_div3xx/lowlevel_v3x/di300.o
obj-y += drv_div3xx/lowlevel_v3x/di300_alg.o
endif

ifneq ($(sub_dir),)
obj-y += common/di_utils.o
obj-y += $(sub_dir)/di_client.o
obj-y += $(sub_dir)/di_fops.o
obj-y += $(sub_dir)/di_driver.o
endif

