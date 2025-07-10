obj-$(CONFIG_COMPONENTS_AW_AUDIO_RPBUF_OMX_EQ) += rpbuf_eq_process.o

CFLAGS +=  -I components/common/aw/audio_amp_al/rpbuf/
CFLAGS +=  -I components/common/aw/audio_amp_al/rpbuf/utils
CFLAGS +=  -I components/common/aw/audio_amp_al/rpbuf/include
CFLAGS +=  -I components/common/aw/audio_amp_al/audio_process/eq/rpbuf
CFLAGS +=  -I components/common/aw/audio_amp_al/log

ifeq ($(CONFIG_COMPONENTS_PROCESS_AW_EQ),y)
CFLAGS +=  -I components/common/aw/process_module/aw_eq/include
endif

ifeq ($(CONFIG_COMPONENTS_OMX_SYSTEM),y)
CFLAGS +=  -I components/common/aw/OpenMax/
CFLAGS +=  -I components/common/aw/OpenMax/include
endif

ifeq ($(CONFIG_COMPONENTS_RINGBUFFER),y)
CFLAGS +=  -I components/common/aw/ringbuffer/include
endif

ifeq ($(CONFIG_COMPONENTS_RPBUF),y)
CFLAGS +=  -I components/common/aw/rpbuf/include
endif
