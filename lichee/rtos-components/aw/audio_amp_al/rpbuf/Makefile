obj-$(CONFIG_COMPONENTS_AW_AUDIO_RPBUF) += rpbuf_common_interface.o

obj-$(CONFIG_COMPONENTS_AW_AUDIO_RPBUF_UTILS) += utils/

CFLAGS +=  -I components/common/aw/audio_amp_al/rpbuf
CFLAGS +=  -I components/common/aw/audio_amp_al/log

ifeq ($(CONFIG_COMPONENTS_RPBUF),y)
CFLAGS +=  -I components/common/aw/rpbuf/include
endif
