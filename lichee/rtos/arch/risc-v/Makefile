obj-y += common/exception.o
obj-y += common/clocksource.o
obj-$(CONFIG_DEBUG_BACKTRACE) += common/backtrace.o
obj-$(CONFIG_ARCH_RISCV_PMP) += common/pmp.o

obj-$(CONFIG_ARCH_RISCV_C906) += c906/

obj-$(CONFIG_ARCH_SUN8IW20) += sun8iw20p1/

CFLAGS-$(CONFIG_ARCH_RISCV_C906) += -I $(BASE)/arch/risc-v/c906/

CFLAGS-$(CONFIG_COMPONENTS_BOOT_REASON) += -I $(BASE)/components/common/aw/boot_reason/
CFLAGS-$(CONFIG_COMPONENTS_KALLSYMS) += -I $(BASE)/components/common/aw/kallsyms/include/
