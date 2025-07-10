obj-$(CONFIG_COMPONENTS_AW_AUDIO_RPBUF_OMX_AEC) += rpbuf_aec_process.o
obj-$(CONFIG_COMPONENTS_AW_AUDIO_RPBUF_OMX_AEC_DOWNLINK) += rpbuf_aec_downlink_process.o

CFLAGS +=  -I components/common/aw/audio_amp_al/rpbuf/
CFLAGS +=  -I components/common/aw/audio_amp_al/rpbuf/utils
CFLAGS +=  -I components/common/aw/audio_amp_al/rpbuf/include
CFLAGS +=  -I components/common/aw/audio_amp_al/audio_process/aec/rpbuf
CFLAGS +=  -I components/common/aw/audio_amp_al/log

ifeq ($(CONFIG_COMPONENT_NF_AEC),y)
CFLAGS +=  -I components/common/thirdparty/algo_process_module/aec/include/hifi4_nf
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
