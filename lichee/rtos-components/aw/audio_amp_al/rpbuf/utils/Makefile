obj-$(CONFIG_COMPONENTS_AW_RPBUF_AUDIO_CTRL_AEC) += rpbuf_audio_ctrl_aec.o
obj-$(CONFIG_COMPONENTS_AW_RPBUF_AUDIO_CTRL_EQ) += rpbuf_audio_ctrl_eq.o
obj-$(CONFIG_COMPONENTS_AW_RPBUF_AUDIO_CTRL_MODULE_PROCESS) += rpbuf_audio_ctrl_module_process.o

CFLAGS +=  -I components/common/aw/audio_amp_al/rpbuf
CFLAGS +=  -I components/common/aw/audio_amp_al/rpbuf/utils
CFLAGS +=  -I components/common/aw/audio_amp_al/rpbuf/include
CFLAGS +=  -I components/common/aw/audio_amp_al/audio_process/aec/rpbuf
CFLAGS +=  -I components/common/aw/audio_amp_al/audio_process/eq/rpbuf
CFLAGS +=  -I components/common/aw/audio_amp_al/audio_process/module_process/rpbuf
CFLAGS +=  -I components/common/aw/audio_amp_al/log

ifeq ($(CONFIG_COMPONENT_NF_AEC),y)
CFLAGS +=  -I components/common/thirdparty/algo_process_module/aec/include/hifi4_nf
endif

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

ifeq ($(CONFIG_COMPONENTS_AW_RPBUF_AUDIO_CTRL_MODULE_PROCESS),y)
CFLAGS +=  -I components/common/aw/audio_amp_al/audio_process/module_process
endif
