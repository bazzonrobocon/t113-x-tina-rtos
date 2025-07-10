obj-$(CONFIG_COMPONENTS_AW_AUDIO_RPDATA) += rpdata_audio_interface.o

obj-$(CONFIG_COMPONENTS_AW_AUDIO_RPDATA_UTILS) += utils/

CFLAGS +=  -I components/common/aw/audio_amp_al/rpdata
CFLAGS +=  -I components/common/aw/audio_amp_al/log

ifeq ($(CONFIG_COMPONENTS_RPDATA),y)
CFLAGS +=  -I components/common/aw/rpdata/include
endif
