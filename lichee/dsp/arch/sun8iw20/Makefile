obj-y += init-sun8iw20.o
obj-y += cpufreq.o
obj-y += mmu.o

ifeq ($(CONFIG_COMPONENTS_AW_AUDIO_SYSTEM),y)
CFLAGS +=  -I components/common/aw/AudioSystem/include
endif
CFLAGS += -DLICHEE_IC=\"$(LICHEE_IC)\"
