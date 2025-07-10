obj-$(CONFIG_COMPONENTS_AW_AUDIO_RPBUF_MODULE_PROCESS) += rpbuf_audio_module_process.o

CFLAGS +=  -I components/common/aw/audio_amp_al/rpbuf
CFLAGS +=  -I components/common/aw/audio_amp_al/rpbuf/utils
CFLAGS +=  -I components/common/aw/audio_amp_al/rpbuf/include
CFLAGS +=  -I components/common/aw/audio_amp_al/audio_process/module_process/rpbuf
CFLAGS +=  -I components/common/aw/audio_amp_al/log

ifeq ($(CONFIG_COMPONENTS_RINGBUFFER),y)
CFLAGS +=  -I components/common/aw/ringbuffer/include
endif

ifeq ($(CONFIG_COMPONENTS_RPBUF),y)
CFLAGS +=  -I components/common/aw/rpbuf/include
endif

