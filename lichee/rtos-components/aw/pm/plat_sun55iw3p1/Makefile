obj-y += main.o
obj-y += pm_manager.o
obj-y += pm_rpc.o

obj-$(CONFIG_ARCH_RISCV_E906) += e906/sleep.o
obj-$(CONFIG_ARCH_RISCV_E906) += e906/platops.o
obj-$(CONFIG_ARCH_RISCV_E906) += e906/pm_systeminit.o

obj-$(CONFIG_ARCH_DSP) += hifi4/sleep.o
obj-$(CONFIG_ARCH_DSP) += hifi4/platops.o
obj-$(CONFIG_ARCH_DSP) += hifi4/pm_systeminit.o

CFLAGS += -I components/common/aw/pm/plat_sun55iw3p1/
CFLAGS += -I components/common/aw/pm/plat_sun55iw3p1/include/

ifeq ($(CONFIG_ARCH_RISCV_E906), y)
CFLAGS += -I components/common/aw/pm/plat_sun55iw3p1/e906/
endif

ifeq ($(CONFIG_ARCH_DSP), y)
CFLAGS += -I components/common/aw/pm/plat_sun55iw3p1/hifi4/
CFLAGS += -I components/common/aw/pm/include/
endif

