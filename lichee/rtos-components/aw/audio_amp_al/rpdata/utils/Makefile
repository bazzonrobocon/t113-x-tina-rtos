obj-$(CONFIG_COMPONENTS_AW_RPDATA_AUDIO_CTRL_ASR) += rpdata_audio_ctrl_asr.o

CFLAGS +=  -I components/common/aw/audio_amp_al/rpdata
CFLAGS +=  -I components/common/aw/audio_amp_al/rpdata/utils
CFLAGS +=  -I components/common/aw/audio_amp_al/rpdata/include
CFLAGS +=  -I components/common/aw/audio_amp_al/audio_process/asr/rpdata
CFLAGS +=  -I components/common/aw/audio_amp_al/log

ifeq ($(CONFIG_COMPONENTS_PROCESS_ASR),y)
CFLAGS +=  -I components/common/aw/process_module/asr/include
endif

ifeq ($(CONFIG_COMPONENTS_OMX_SYSTEM),y)
CFLAGS +=  -I components/common/aw/OpenMax/
CFLAGS +=  -I components/common/aw/OpenMax/include
endif

ifeq ($(CONFIG_COMPONENTS_RINGBUFFER),y)
CFLAGS +=  -I components/common/aw/ringbuffer/include
endif

ifeq ($(CONFIG_COMPONENTS_RPDATA),y)
CFLAGS +=  -I components/common/aw/rpdata/include
endif
