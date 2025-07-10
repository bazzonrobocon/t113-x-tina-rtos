obj-y += head_s.o
obj-y += plic.o
obj-y += cache.o
obj-y += spinlock.o
obj-y += clic.o
obj-y += interrupt.o

obj-$(CONFIG_ARCH_SUN8IW20) += platform/platform_sun8iw20.o
CFLAGS-$(CONFIG_ARCH_SUN8IW20) += -I $(BASE)/arch/risc-v/sun8iw20p1/

CFLAGS-$(CONFIG_ARCH_SUN20IW2) += -I $(BASE)/arch/risc-v/sun20iw2p1/
