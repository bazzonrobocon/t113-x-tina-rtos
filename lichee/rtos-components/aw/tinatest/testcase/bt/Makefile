ifeq ($(CONFIG_AW_COMPONENT_ZBLUE), y)

obj-$(CONFIG_TESTCASE_BTON) += tt-onbt.o
obj-$(CONFIG_TESTCASE_BTOFF) += tt-offbt.o
obj-$(CONFIG_TESTCASE_BTSCAN) += tt-scanbt.o

HCIOPTION = H4
CFLAGS += -DCONFIG_ARM -DZTEST_UNITTEST -D$(HCIOPTION)
CFLAGS += -I drivers/drv/wireless/xradio/os/include

endif
