/*
* This confidential and proprietary software should be used
* under the licensing agreement from Allwinner Technology.

* Copyright (C) 2020-2030 Allwinner Technology Limited
* All rights reserved.

* Author:zhengwanyu <zhengwanyu@allwinnertech.com>

* The entire notice above must be reproduced on all authorised
* copies and copies may only be made to the extent permitted
* by a licensing agreement from Allwinner Technology Limited.
*/

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <stdlib.h>
#include <hal_mem.h>

#include "div3xx_hal.h"
#include "drm_fourcc.h"

struct div3xx_hal {
	int diFd;
	unsigned int diMode;

	struct di_fb tnrFb[3];
	unsigned int processCount;

	struct di_size srcSize;
	struct di_size dstSize;

	unsigned char debug;
};

enum packet_type {
	DEINTERLACE_START,
	DEINTERLACE_START_ACK,
	DEINTERLACE_STOP,
	DEINTERLACE_STOP_ACK,
};

static unsigned int getDrmFormat(unsigned int format)
{
	switch(format){
	case DI_FORMAT_YUV420_PLANNER:
		return DRM_FORMAT_YVU420;

	case DI_FORMAT_YUV420_NV21:
		return DRM_FORMAT_NV21;

	case DI_FORMAT_YUV420_NV12:
		return DRM_FORMAT_NV12;

	case DI_FORMAT_YUV422_PLANNER:
		return DRM_FORMAT_YVU422;

	case DI_FORMAT_YUV422_NV61:
		return DRM_FORMAT_NV61;

	case DI_FORMAT_YUV422_NV16:
		return DRM_FORMAT_NV16;
	}

	return 0;
}

#define DI_INF(...) \
	do {printf("[DI HAL]"); printf(__VA_ARGS__);} while(0);
#define DI_ERR(...) \
	do {printf("[DI ERROR]"); printf(__VA_ARGS__);} while(0);
#define DI_TRACE() \
	do {DI_INF("%s\n", __func__)} while(0);

#define MEMCLEAR(x) memset(x, 0, sizeof(*x))

extern int sunxi_di_control(int cmd, void *arg);
extern int sunxi_di_release(void);
extern int sunxi_di_open(void);
extern int rpmsg_deinterlace_remote_call(int type);
extern void preview_enhancer_rpmsg_init();

static int call_close_iommu()
{
	return rpmsg_deinterlace_remote_call(DEINTERLACE_START);
}

static int call_open_iommu()
{
	return rpmsg_deinterlace_remote_call(DEINTERLACE_STOP);
}

static void dumpInitDebugInfo(struct div3xx_hal *hal, void *para)
{
	if (!hal->debug)
		return;

	DI_INF("DI INIT DATA:\n");
	DI_INF("DI Mode:%d\n", hal->diMode);
	if (hal->diMode == DIV3X_MODE_OUT1_TNR) {
		struct init_para_out1_tnr *out1tnr
			= (struct init_para_out1_tnr *)para;
		DI_INF("size src:(%d, %d)  dst:(%d, %d)\n",
			out1tnr->src_w, out1tnr->src_h, out1tnr->dst_w, out1tnr->dst_h);
		DI_INF("crop:(%d, %d)~(%d, %d)\n",
			out1tnr->crop.left, out1tnr->crop.top,
			out1tnr->crop.right, out1tnr->crop.bottom);
		DI_INF("demo crop:(%d, %d)~(%d, %d)\n",
			out1tnr->democrop.left, out1tnr->democrop.top,
			out1tnr->democrop.right, out1tnr->democrop.bottom);
		DI_INF("tnrBuffer(fd, phyaddr):(%d, %lld) (%d, %lld)\n",
			out1tnr->tnrBuffer[0].fd, out1tnr->tnrBuffer[0].phyaddr,
			out1tnr->tnrBuffer[1].fd, out1tnr->tnrBuffer[1].phyaddr);
		DI_INF("tnr format:%s\n", getDiPixelFormatName(out1tnr->tnrFormat));
		DI_INF("fmt_en:%d\n", out1tnr->fmd_en);
	}

	DI_INF("\n\n\n");
}

static int diGetIPVersion(struct div3xx_hal *hal)
{
	int ret;
	struct di_version v;

	ret = sunxi_di_control(DI_IOC_GET_VERSION, &v);
	if (ret) {
		DI_ERR("DI_IOC_GET_VERSION failed\n");
		return ret;
	}

	DI_INF("di version[%d.%d.%d], hw_ip[0x%x]\n",
		v.version_major,
		v.version_minor,
		v.version_patchlevel,
		v.ip_version);

	return ret;
}

static int setDiFb(struct di_fb *fb,
			  unsigned int width, unsigned int height,
			  int fd, unsigned long long phyAddr, unsigned char field_type,
			  unsigned int format)
{
	unsigned int pixel_num;

	MEMCLEAR(fb);

	if (!phyAddr)
		return 0;

	pixel_num = width * height;

	fb->size.width = width;
	fb->size.height = height;
    fb->format = getDrmFormat(format);

    fb->dma_buf_fd = fd;
	fb->buf.y_addr = phyAddr;
	fb->buf.cb_addr = phyAddr + pixel_num;

    if (format == DI_FORMAT_YUV420_PLANNER)
		fb->buf.cr_addr = phyAddr +  pixel_num
			                + pixel_num / 4;
	else if (format == DI_FORMAT_YUV422_PLANNER)
		fb->buf.cr_addr = phyAddr +  pixel_num
						    + pixel_num / 2;


	//the stride of yuv420 and yuv422 is same with each other
	if (format == DI_IN_FORMAT_YUV422P || format == DI_IN_FORMAT_YUV420P) {
		fb->buf.ystride = width;//驱动中使用,Y分量一行的跨距
		fb->buf.cstride = width / 2;//驱动中使用，表示UV分量一行的跨距
	} else {
		fb->buf.ystride = width;
		fb->buf.cstride = width;
	}

	return 0;
}

static int diFbOut1Tnr(struct div3xx_hal *hal, struct fb_para_out1_tnr *para)
{
	struct di_process_fb_arg arg;
	MEMCLEAR(&arg);

	arg.client_number = 0;

	if (sunxi_di_control(DI_IOC_SET_TNR_MODE,
		(struct di_tnr_mode *)&para->tnrMode) < 0) {
		DI_ERR("DI_IOC_SET_TNR_MODE failed\n");
		return -1;
	}

	arg.is_interlace = 1;
	arg.top_field_first = para->topFieldFirst;

	if (hal->processCount == 0) {
		if (setDiFb(&arg.in_fb0,
			  hal->srcSize.width, hal->srcSize.height,
			  para->inFb[0].fd, para->inFb[0].phyaddr, para->inFb[0].field_type,
			  para->inFormat[0]) < 0) {
			DI_ERR("setDiFb failed\n");
			return -1;
		}

		if (para->inFb[0].field_type && para->inFbNf[0].field_type
		&& (para->inFb[0].field_type != para->inFbNf[0].field_type)
#ifdef CONF_DI_SINGEL_FILE
		//&& setDiFb(&arg.in_fb0_nf,
			  //hal->srcSize.width, hal->srcSize.height,
			  //para->inFbNf[0].fd, para->inFbNf[0].phyaddr, para->inFbNf[0].field_type,
			  //para->inFormat[0]) < 0
#endif
			) {
			DI_ERR("setDiFbNf0 failed\n");
		}
	} else {
		memcpy(&arg.in_fb0, &hal->tnrFb[(hal->processCount - 1) % 2],
					sizeof(struct di_fb));
#ifdef CONF_DI_SINGEL_FILE
		//memset(&arg.in_fb0_nf, 0, sizeof(struct di_fb));
#endif
	}


	if (setDiFb(&arg.in_fb1,
		  hal->srcSize.width, hal->srcSize.height,
		  para->inFb[1].fd, para->inFb[1].phyaddr, para->inFb[1].field_type,
		  para->inFormat[1]) < 0) {
		DI_ERR("setDiFb failed\n");
		return -1;
	}

	if (para->inFb[1].field_type && para->inFbNf[1].field_type
		&& (para->inFb[1].field_type != para->inFbNf[1].field_type)
#ifdef CONF_DI_SINGEL_FILE
		//&& setDiFb(&arg.in_fb1_nf,
			  //hal->srcSize.width, hal->srcSize.height,
			  //para->inFbNf[1].fd, para->inFbNf[1].phyaddr, para->inFbNf[1].field_type,
		  //para->inFormat[1]) < 0
#endif
		  ) {
		DI_ERR("setDiFbNf1 failed\n");
	}

	if (setDiFb(&arg.out_dit_fb0,
		  hal->dstSize.width, hal->dstSize.height,
		  para->outFb.fd, para->outFb.phyaddr, 0,
		  para->outFormat) < 0) {
		DI_ERR("setDiFb failed\n");
		return -1;
	}

	memcpy(&arg.out_tnr_fb0, &hal->tnrFb[hal->processCount % 2],
				sizeof(struct di_fb));
	if (sunxi_di_control(DI_IOC_PROCESS_FB, &arg) < 0) {
		DI_ERR("DI_IOC_PROCESS_FB failed");
		return -1;
	}

	return 0;
}

static int diInitSize(struct div3xx_hal *hal,
		      unsigned int src_w, unsigned int src_h,
		      unsigned int dst_w, unsigned int dst_h)
{
	hal->srcSize.width = src_w;
	hal->srcSize.height = src_h;
	hal->dstSize.width = dst_w;
	hal->dstSize.height = dst_h;

	return sunxi_di_control(DI_IOC_SET_VIDEO_SIZE, &hal->srcSize);
}

static int diInitCrop(struct div3xx_hal *hal, struct di_rect *rect)
{
	return sunxi_di_control(DI_IOC_SET_VIDEO_CROP, rect);
}

static int diInitDemocrop(struct div3xx_hal *hal, struct di_rect *rect)
{
	struct di_demo_crop_arg arg;

	MEMCLEAR(&arg);
	arg.dit_demo.left = 0;
	arg.dit_demo.top = 0;
	arg.dit_demo.right = hal->srcSize.width;
	arg.dit_demo.bottom = hal->srcSize.height;

	memcpy(&arg.tnr_demo, rect, sizeof(struct di_rect));

	return sunxi_di_control(DI_IOC_SET_DEMO_CROP, &arg);
}

static int diInitDitMode(struct div3xx_hal *hal,
	unsigned int intp_mode, unsigned int out_frame_mode)
{
	struct di_dit_mode ditmode;

	ditmode.intp_mode = intp_mode;
	ditmode.out_frame_mode = out_frame_mode;

	return sunxi_di_control(DI_IOC_SET_DIT_MODE, &ditmode);
}

static int diInitTnr(struct div3xx_hal *hal, struct di_tnr_mode *tnrMode)
{
	return sunxi_di_control(DI_IOC_SET_TNR_MODE, tnrMode);
}

static int diInitDisableTnr(struct div3xx_hal *hal)
{
	struct di_tnr_mode tnrMode;

	tnrMode.mode = DI_TNR_MODE_INVALID;
	tnrMode.level = 0;
	return sunxi_di_control(DI_IOC_SET_TNR_MODE, &tnrMode);
}

static int diInitFmd(struct div3xx_hal *hal, bool en)
{
	struct di_fmd_enable fmd_en;

	fmd_en.en = en;
	return sunxi_di_control(DI_IOC_SET_FMD_ENABLE, &fmd_en);
}

static int diCheckInitPara(struct div3xx_hal *hal)
{
	return sunxi_di_control(DI_IOC_CHECK_PARA, 0);
}

static int diReset(struct div3xx_hal *hal)
{
	return sunxi_di_control(DI_IOC_RESET, 0);
}

static int diFbOnlyTnr(struct div3xx_hal *hal, struct fb_para_only_tnr *para)
{
	struct di_process_fb_arg arg;
	MEMCLEAR(&arg);

	if (sunxi_di_control(DI_IOC_SET_TNR_MODE,
		(struct di_tnr_mode *)&para->tnrMode) < 0) {
		DI_ERR("DI_IOC_SET_TNR_MODE failed\n");
		return -1;
	}

	if (setDiFb(&arg.in_fb0,
		  hal->srcSize.width, hal->srcSize.height,
		  para->inFb[0].fd, para->inFb[0].phyaddr, 0,
		  para->inFormat[0]) < 0) {
		DI_ERR("setDiFb failed\n");
		return -1;
	}

	if (setDiFb(&arg.in_fb1,
		  hal->srcSize.width, hal->srcSize.height,
		  para->inFb[1].fd, para->inFb[1].phyaddr, 0,
		  para->inFormat[1]) < 0) {
		DI_ERR("setDiFb failed\n");
		return -1;
	}

	if (setDiFb(&arg.out_tnr_fb0,
		  hal->dstSize.width, hal->dstSize.height,
		  para->outTnrFb.fd, para->outTnrFb.phyaddr, 0,
		  para->outTnrFormat) < 0) {
		DI_ERR("setDiFb failed\n");
		return -1;
	}

	if (sunxi_di_control(DI_IOC_PROCESS_FB, &arg) < 0) {
		DI_ERR("DI_IOC_PROCESS_FB failed");
		return -1;
	}

	return 0;
}

static int diFbOut1(struct div3xx_hal *hal, struct fb_para_out1 *para)
{
	struct di_process_fb_arg arg;
	MEMCLEAR(&arg);

	arg.is_interlace = 1;
	arg.top_field_first = para->topFieldFirst;

	if (setDiFb(&arg.in_fb0,
		  hal->srcSize.width, hal->srcSize.height,
		  para->inFb[0].fd, para->inFb[0].phyaddr, para->inFb[0].field_type,
		  para->inFormat[0]) < 0) {
		DI_ERR("setDiFb failed\n");
		return -1;
	}

#ifdef CONF_DI_SINGEL_FILE
	if (para->inFb[0].field_type && para->inFbNf[0].field_type
		&& (para->inFb[0].field_type != para->inFbNf[0].field_type)

		&& setDiFb(&arg.in_fb0_nf,
			  hal->srcSize.width, hal->srcSize.height,
			  para->inFbNf[0].fd, para->inFbNf[0].phyaddr, para->inFbNf[0].field_type,
			  para->inFormat[0]) < 0) {
		DI_ERR("setDiFbNf0 failed\n");
	}
#endif

	if (setDiFb(&arg.in_fb1,
		  hal->srcSize.width, hal->srcSize.height,
		  para->inFb[1].fd, para->inFb[1].phyaddr, para->inFb[1].field_type,
		  para->inFormat[1]) < 0) {
		DI_ERR("setDiFb failed\n");
		return -1;
	}

#ifdef CONF_DI_SINGEL_FILE
	if (para->inFb[1].field_type && para->inFbNf[1].field_type
		&& (para->inFb[1].field_type != para->inFbNf[1].field_type)
		&& setDiFb(&arg.in_fb1_nf,
			  hal->srcSize.width, hal->srcSize.height,
			  para->inFbNf[1].fd, para->inFbNf[1].phyaddr, para->inFbNf[1].field_type,
		  para->inFormat[1]) < 0) {
		DI_ERR("setDiFbNf1 failed\n");
	}
#endif

	if (setDiFb(&arg.out_dit_fb0,
		  hal->dstSize.width, hal->dstSize.height,
		  para->outFb.fd, para->outFb.phyaddr, 0,
		  para->outFormat) < 0) {
		DI_ERR("setDiFb failed\n");
		return -1;
	}

	if (sunxi_di_control(DI_IOC_PROCESS_FB, &arg) < 0) {
		DI_ERR("DI_IOC_PROCESS_FB failed");
		return -1;
	}

	return 0;
}

static int diFbBob(struct div3xx_hal *hal, struct fb_para_bob *para)
{
	struct di_process_fb_arg arg;
	MEMCLEAR(&arg);

	arg.is_interlace = 1;
	arg.top_field_first = para->topFieldFirst;
	arg.base_field = para->base_field;//process with top_field, also can change to bottom_field

	if (setDiFb(&arg.in_fb0,
		  hal->srcSize.width, hal->srcSize.height,
		  para->inFb[0].fd, para->inFb[0].phyaddr, para->inFb[0].field_type,
		  para->inFormat[0]) < 0) {
		DI_ERR("setDiFb failed\n");
		return -1;
	}

	if (setDiFb(&arg.out_dit_fb0,
		  hal->dstSize.width, hal->dstSize.height,
		  para->outFb.fd, para->outFb.phyaddr, 0,
		  para->outFormat) < 0) {
		DI_ERR("setDiFb failed\n");
		return -1;
	}

	if (sunxi_di_control(DI_IOC_PROCESS_FB, &arg) < 0) {
		DI_ERR("DI_IOC_PROCESS_FB failed");
		return -1;
	}

	return 0;
}

static int diFbOut2Tnr(struct div3xx_hal *hal, struct fb_para_out2_tnr *para)
{
	struct di_process_fb_arg arg;

	MEMCLEAR(&arg);

	if (sunxi_di_control(DI_IOC_SET_TNR_MODE,
		(struct di_tnr_mode *)&para->tnrMode) < 0) {
		DI_ERR("DI_IOC_SET_TNR_MODE failed\n");
		return -1;
	}

	arg.is_interlace = 1;
	arg.top_field_first = para->topFieldFirst;

	if (hal->processCount == 0 || hal->processCount == 1) {
		if (setDiFb(&arg.in_fb0,
			  hal->srcSize.width, hal->srcSize.height,
			  para->inFb[0].fd, para->inFb[0].phyaddr, para->inFb[0].field_type,
			  para->inFormat[0]) < 0) {
			DI_ERR("setDiFb0 failed\n");
			return -1;
		}

		if (para->inFb[0].field_type && para->inFbNf[0].field_type
		&& (para->inFb[0].field_type != para->inFbNf[0].field_type)
#ifdef CONF_DI_SINGEL_FILE
		&& setDiFb(&arg.in_fb0_nf,
			  hal->srcSize.width, hal->srcSize.height,
			  para->inFbNf[0].fd, para->inFbNf[0].phyaddr, para->inFbNf[0].field_type,
			  para->inFormat[0]) < 0
#endif
			  ) {
			DI_ERR("setDiFbNf0 failed\n");
		}
	} else {
		memcpy(&arg.in_fb0, &hal->tnrFb[(hal->processCount + 1) % 3],
					sizeof(struct di_fb));
#ifdef CONF_DI_SINGEL_FILE
		memset(&arg.in_fb0_nf, 0, sizeof(struct di_fb));
#endif
	}

	if (hal->processCount == 0) {
		if (setDiFb(&arg.in_fb1,
			  hal->srcSize.width, hal->srcSize.height,
			  para->inFb[1].fd, para->inFb[1].phyaddr, para->inFb[1].field_type,
			  para->inFormat[1]) < 0) {
			DI_ERR("setDiFb1 failed\n");
			return -1;
		}

		if (para->inFb[1].field_type && para->inFbNf[1].field_type
		&& (para->inFb[1].field_type != para->inFbNf[1].field_type)
#ifdef CONF_DI_SINGEL_FILE
		&& setDiFb(&arg.in_fb1_nf,
			  hal->srcSize.width, hal->srcSize.height,
			  para->inFbNf[1].fd, para->inFbNf[1].phyaddr, para->inFbNf[1].field_type,
			  para->inFormat[1]) < 0
#endif
			  ) {
			DI_ERR("setDiFbNf1 failed\n");
		}

	} else {
		memcpy(&arg.in_fb1, &hal->tnrFb[(hal->processCount + 2) % 3],
					sizeof(struct di_fb));
#ifdef CONF_DI_SINGEL_FILE
		memset(&arg.in_fb1_nf, 0, sizeof(struct di_fb));
#endif
	}

	if (setDiFb(&arg.in_fb2,
		  hal->srcSize.width, hal->srcSize.height,
		  para->inFb[2].fd, para->inFb[2].phyaddr, para->inFb[2].field_type,
		  para->inFormat[2]) < 0) {
		DI_ERR("setDiFb2 failed\n");
		return -1;
	}

	if (para->inFb[2].field_type && para->inFbNf[2].field_type
	&& (para->inFb[2].field_type != para->inFbNf[2].field_type)
#ifdef CONF_DI_SINGEL_FILE
	&& setDiFb(&arg.in_fb2_nf,
		  hal->srcSize.width, hal->srcSize.height,
		  para->inFbNf[2].fd, para->inFbNf[2].phyaddr, para->inFbNf[2].field_type,
		  para->inFormat[2]) < 0
#endif
		  ) {
		DI_ERR("setDiFbNf2 failed\n");
	}

	if (setDiFb(&arg.out_dit_fb0,
		  hal->dstSize.width, hal->dstSize.height,
		  para->outFb[0].fd, para->outFb[0].phyaddr, 0,
		  para->outFormat[0]) < 0) {
		DI_ERR("setDiFb failed\n");
		return -1;
	}

	if (setDiFb(&arg.out_dit_fb1,
		  hal->dstSize.width, hal->dstSize.height,
		  para->outFb[1].fd, para->outFb[1].phyaddr, 0,
		  para->outFormat[1]) < 0) {
		DI_ERR("setDiFb failed\n");
		return -1;
	}

	memcpy(&arg.out_tnr_fb0, &hal->tnrFb[hal->processCount % 3],
				sizeof(struct di_fb));

	if (sunxi_di_control(DI_IOC_PROCESS_FB, &arg) < 0) {
		DI_ERR("DI_IOC_PROCESS_FB failed");
		return -1;
	}

	return 0;
}

static int diFbOut2(struct div3xx_hal *hal, struct fb_para_out2 *para)
{
	struct di_process_fb_arg arg;
	MEMCLEAR(&arg);

	arg.is_interlace = 1;
	arg.top_field_first = para->topFieldFirst;

	if (setDiFb(&arg.in_fb0,
		  hal->srcSize.width, hal->srcSize.height,
		  para->inFb[0].fd, para->inFb[0].phyaddr, para->inFb[0].field_type,
		  para->inFormat[0]) < 0) {
		DI_ERR("setDiFb0 failed\n");
		return -1;
	}

	if (para->inFb[0].field_type && para->inFbNf[0].field_type
	&& (para->inFb[0].field_type != para->inFbNf[0].field_type)
#ifdef CONF_DI_SINGEL_FILE
	&& setDiFb(&arg.in_fb0_nf,
		  hal->srcSize.width, hal->srcSize.height,
		  para->inFbNf[0].fd, para->inFbNf[0].phyaddr, para->inFbNf[0].field_type,
		  para->inFormat[0]) < 0
#endif
		  ) {
		DI_ERR("setDiFbNf0 failed\n");
	}

	if (setDiFb(&arg.in_fb1,
		  hal->srcSize.width, hal->srcSize.height,
		  para->inFb[1].fd, para->inFb[1].phyaddr, para->inFb[1].field_type,
		  para->inFormat[1]) < 0) {
		DI_ERR("setDiFb failed\n");
		return -1;
	}

	if (para->inFb[1].field_type && para->inFbNf[1].field_type
	&& (para->inFb[1].field_type != para->inFbNf[1].field_type)
#ifdef CONF_DI_SINGEL_FILE
	&& setDiFb(&arg.in_fb1_nf,
		  hal->srcSize.width, hal->srcSize.height,
		  para->inFbNf[1].fd, para->inFbNf[1].phyaddr, para->inFbNf[1].field_type,
		  para->inFormat[1]) < 0
#endif
		  ) {
		DI_ERR("setDiFbNf1 failed\n");
	}

	if (setDiFb(&arg.in_fb2,
		  hal->srcSize.width, hal->srcSize.height,
		  para->inFb[2].fd, para->inFb[2].phyaddr, para->inFb[2].field_type,
		  para->inFormat[2]) < 0) {
		DI_ERR("setDiFb2 failed\n");
		return -1;
	}

	if (para->inFb[2].field_type && para->inFbNf[2].field_type
	&& (para->inFb[2].field_type != para->inFbNf[2].field_type)
#ifdef CONF_DI_SINGEL_FILE
	&& setDiFb(&arg.in_fb2_nf,
		  hal->srcSize.width, hal->srcSize.height,
		  para->inFbNf[2].fd, para->inFbNf[2].phyaddr, para->inFbNf[2].field_type,
		  para->inFormat[2]) < 0
#endif
		  ) {
		DI_ERR("setDiFbNf2 failed\n");
	}

	if (setDiFb(&arg.out_dit_fb0,
		  hal->dstSize.width, hal->dstSize.height,
		  para->outFb[0].fd, para->outFb[0].phyaddr, 0,
		  para->outFormat[0]) < 0) {
		DI_ERR("setDiFb failed\n");
		return -1;
	}

	if (setDiFb(&arg.out_dit_fb1,
		  hal->dstSize.width, hal->dstSize.height,
		  para->outFb[1].fd, para->outFb[1].phyaddr, 0,
		  para->outFormat[1]) < 0) {
		DI_ERR("setDiFb failed\n");
		return -1;
	}

	if (sunxi_di_control(DI_IOC_PROCESS_FB, &arg) < 0) {
		DI_ERR("DI_IOC_PROCESS_FB failed");
		return -1;
	}

	return 0;
}

static int diInitOnlyTnr(struct div3xx_hal *hal, struct init_para_only_tnr *para)
{
	struct di_tnr_mode tnrmode;
	tnrmode.mode = DI_TNR_MODE_ADAPTIVE;
	tnrmode.level = DI_TNR_LEVEL_HIGH;

	hal->debug = para->debug_en;
	if (diInitSize(hal,
		para->src_w, para->src_h, para->dst_w, para->dst_h) < 0) {
		DI_ERR("diInitSrcSize failed\n");
		return -1;
	}

	if (diInitCrop(hal, &para->crop) < 0) {
		DI_ERR("diInitCrop failed\n");
		return -1;
	}

	if (diInitDemocrop(hal, &para->democrop) < 0) {
		DI_ERR("diInitDemocrop failed\n");
		return -1;
	}

	if (diInitDitMode(hal, DI_DIT_INTP_MODE_INVALID,
		DI_DIT_OUT_0FRAME) < 0) {
		DI_ERR("diInitDitMode failed\n");
		return -1;
	}

	if (diInitTnr(hal, &tnrmode) < 0) {
		DI_ERR("diInitTnr failed\n");
		return -1;
	};

	if (diInitFmd(hal, 0) < 0) {
		DI_ERR("diInitFmd failed\n");
		return -1;
	}

	return 0;
}

static int diInitOut2Tnr(struct div3xx_hal *hal, struct init_para_out2_tnr *para)
{
	int i;
	struct di_tnr_mode tnrmode;
	tnrmode.mode = DI_TNR_MODE_ADAPTIVE;
	tnrmode.level = DI_TNR_LEVEL_HIGH;

	DI_TRACE();
	hal->processCount = 0;
	hal->debug = para->debug_en;

	if (diInitSize(hal, para->src_w, para->src_h, para->dst_w, para->dst_h) < 0) {
		DI_ERR("diInitSrcSize failed\n");
		return -1;
	}


	/*if (diInitDisableCrop(hal) < 0) {
		DI_ERR("diInitDisableCrop failed\n");
		return -1;
	}*/

	if (diInitCrop(hal, &para->crop) < 0) {
		DI_ERR("diInitCrop failed\n");
		return -1;
	}

	if (diInitDemocrop(hal, &para->democrop) < 0) {
		DI_ERR("diInitDemocrop failed\n");
		return -1;
	}

	if (diInitDitMode(hal, DI_DIT_INTP_MODE_MOTION,
		DI_DIT_OUT_2FRAME) < 0) {
		DI_ERR("diInitDitMode failed\n");
		return -1;
	}

	if (diInitTnr(hal, &tnrmode) < 0) {
		DI_ERR("diInitTnr failed\n");
		return -1;
	};

	if (diInitFmd(hal, para->fmd_en) < 0) {
		DI_ERR("diInitFmd failed\n");
		return -1;
	}

	for (i = 0; i < 3; i++)
		setDiFb(&hal->tnrFb[i],
			  hal->dstSize.width, hal->dstSize.height,
			  para->tnrBuffer[i].fd, para->tnrBuffer[i].phyaddr, 0,
			  para->tnrFormat);

	return 0;
}

static int diInitOut2(struct div3xx_hal *hal, struct init_para_out2 *para)
{
	DI_TRACE();

	hal->debug = para->debug_en;

	if (diInitSize(hal,
		para->src_w, para->src_h, para->dst_w, para->dst_h) < 0) {
		DI_ERR("diInitSrcSize failed\n");
		return -1;
	}

	if (diInitCrop(hal, &para->crop) < 0) {
		DI_ERR("diInitCrop failed\n");
		return -1;
	}

	if (diInitDitMode(hal, DI_DIT_INTP_MODE_MOTION,
		DI_DIT_OUT_2FRAME) < 0) {
		DI_ERR("diInitDitMode failed\n");
		return -1;
	}

	if (diInitDisableTnr(hal) < 0) {
		DI_ERR("diInitDisableTnr failed\n");
		return -1;
	};

	if (diInitFmd(hal, para->fmd_en) < 0) {
		DI_ERR("diInitFmd failed\n");
		return -1;
	}

	return 0;
}

static int diInitOut1Tnr(struct div3xx_hal *hal, struct init_para_out1_tnr *para)
{
	int i;
	struct di_tnr_mode tnrmode;
	tnrmode.mode = DI_TNR_MODE_ADAPTIVE;
	tnrmode.level = DI_TNR_LEVEL_HIGH;

	hal->processCount = 0;
	hal->debug = para->debug_en;

	if (diInitSize(hal, para->src_w, para->src_h, para->dst_w, para->dst_h) < 0) {
		DI_ERR("diInitSrcSize failed\n");
		return -1;
	}

	if (diInitCrop(hal, &para->crop) < 0) {
		DI_ERR("diInitCrop failed\n");
		return -1;
	}

	if (diInitDemocrop(hal, &para->democrop) < 0) {
		DI_ERR("diInitDemocrop failed\n");
		return -1;
	}

	if (diInitDitMode(hal, DI_DIT_INTP_MODE_MOTION,
		DI_DIT_OUT_1FRAME) < 0) {
		DI_ERR("diInitDitMode failed\n");
		return -1;
	}

	if (diInitTnr(hal, &tnrmode) < 0) {
		DI_ERR("diInitTnr failed\n");
		return -1;
	};

	if (diInitFmd(hal, para->fmd_en) < 0) {
		DI_ERR("diInitFmd failed\n");
		return -1;
	}

	for (i = 0; i < 2; i++)
		setDiFb(&hal->tnrFb[i],
			  hal->dstSize.width, hal->dstSize.height,
			  para->tnrBuffer[i].fd, para->tnrBuffer[i].phyaddr, 0,
			  para->tnrFormat);

	return 0;
}

static int diInitOut1(struct div3xx_hal *hal, struct init_para_out1 *para)
{
	hal->debug = para->debug_en;

	if (diInitSize(hal,
		para->src_w, para->src_h, para->dst_w, para->dst_h) < 0) {
		DI_ERR("diInitSrcSize failed\n");
		return -1;
	}

	if (diInitCrop(hal, &para->crop) < 0) {
		DI_ERR("diInitCrop failed\n");
		return -1;
	}

	if (diInitDitMode(hal, DI_DIT_INTP_MODE_MOTION,
		DI_DIT_OUT_1FRAME) < 0) {
		DI_ERR("diInitDitMode failed\n");
		return -1;
	}

	if (diInitDisableTnr(hal) < 0) {
		DI_ERR("diInitDisableTnr failed\n");
		return -1;
	};

	if (diInitFmd(hal, para->fmd_en) < 0) {
		DI_ERR("diInitFmd failed\n");
		return -1;
	}

	return 0;
}

static int diInitBob(struct div3xx_hal *hal, struct init_para_bob *para)
{
	hal->debug = para->debug_en;

	if (diInitSize(hal,
		para->src_w, para->src_h, para->dst_w, para->dst_h) < 0) {
		DI_ERR("diInitSrcSize failed\n");
		return -1;
	}

	if (diInitCrop(hal, &para->crop) < 0) {
		DI_ERR("diInitCrop failed\n");
		return -1;
	}

	if (diInitDitMode(hal, DI_DIT_INTP_MODE_BOB,
		DI_DIT_OUT_1FRAME) < 0) {
		DI_ERR("diInitDitMode failed\n");
		return -1;
	}

	if (diInitDisableTnr(hal) < 0) {
		DI_ERR("diInitDisableTnr failed\n");
		return -1;
	};

	if (diInitFmd(hal, 0) < 0) {
		DI_ERR("diInitFmd failed\n");
		return -1;
	}

	return 0;
}

int diInit(DIDevice *deinterlace, unsigned int mode, void *init_para)
{
	struct div3xx_hal *hal = hal_malloc(sizeof(struct div3xx_hal));
	int client_num = 0;
	int ret = 0;

	MEMCLEAR(hal);
	DI_INF("Init MODE %d \n", mode);

    hal->diMode = mode;

	client_num = sunxi_di_open();
	if (client_num < 0) {
		DI_ERR("open di failed!\n");
		return -1;
	}

	if (call_close_iommu() < 0) {
		DI_ERR("Call close iommu fail \n");
		return -1;
	}

	if (diGetIPVersion(hal) < 0) {
		DI_ERR("diGetIPVersion failed!\n");
		return -1;
	}


	if (diReset(hal) < 0) {
		DI_ERR("Di Reset failed \n");
		return -1;
	}

	if (mode == DIV3X_MODE_BOB) {
		struct init_para_bob *para
			= (struct init_para_bob *)init_para;
		hal->debug = para->debug_en;

		dumpInitDebugInfo(hal, init_para);
		ret = diInitBob(hal, para);
	} else if (mode == DIV3X_MODE_OUT1_TNR) {
		DI_INF("Init MODE OUT1\n");
		struct init_para_out1_tnr *para
			= (struct init_para_out1_tnr *)init_para;
		hal->debug = para->debug_en;

		dumpInitDebugInfo(hal, init_para);
		ret = diInitOut1Tnr(hal, para);
	} else if (mode ==  DIV3X_MODE_ONLY_TNR) {
		struct init_para_only_tnr *para
			= (struct init_para_only_tnr *)init_para;
		hal->debug = para->debug_en;

		dumpInitDebugInfo(hal, init_para);
		ret = diInitOnlyTnr(hal, para);
	} else if (mode == DIV3X_MODE_OUT1) {
		struct init_para_out1 *para
			= (struct init_para_out1 *)init_para;
		hal->debug = para->debug_en;

		dumpInitDebugInfo(hal, init_para);
		ret = diInitOut1(hal, para);
	} else if (mode == DIV3X_MODE_OUT2) {
		struct init_para_out2 *para
			= (struct init_para_out2 *)init_para;
		hal->debug = para->debug_en;

		dumpInitDebugInfo(hal, init_para);
		ret = diInitOut2(hal, para);
	} else if (mode == DIV3X_MODE_OUT2_TNR) {
		struct init_para_out2_tnr *para
			= (struct init_para_out2_tnr *)init_para;
		hal->debug = para->debug_en;

		dumpInitDebugInfo(hal, init_para);
		ret = diInitOut2Tnr(hal, para);
	} else {
		DI_ERR("unsupport mode %d \n", mode);
	}

	if (ret < 0) {
		DI_ERR("init para failed\n");
		return -1;
	}

	if (diCheckInitPara(hal) < 0) {
		DI_ERR("diCheckInitPara failed\n");
		return -1;
	}

	*deinterlace = (DIDevice)hal;
	return client_num;
}

static void dumpDiFrambufferInfo(struct div3xx_hal *hal, void *para)
{
	if (!hal->debug)
		return;
	DI_INF("DI FRAMEBUFFER INFO:\n");
	if (hal->diMode == DIV3X_MODE_OUT2_TNR) {
		struct fb_para_out2_tnr *out2tnr
				= (struct fb_para_out2_tnr *)para;
		DI_INF("ttf:%d\n", out2tnr->topFieldFirst);
		DI_INF("infb:(fd, phyaddr, field_type) (%d, %lld, %d) (%d, %lld, %d) (%d, %lld, %d)\n",
			out2tnr->inFb[0].fd, out2tnr->inFb[0].phyaddr, out2tnr->inFb[0].field_type,
			out2tnr->inFb[1].fd, out2tnr->inFb[1].phyaddr, out2tnr->inFb[1].field_type,
			out2tnr->inFb[2].fd, out2tnr->inFb[2].phyaddr, out2tnr->inFb[2].field_type);
		DI_INF("infbnf:(fd, phyaddr, field_type) (%d, %lld, %d) (%d, %lld, %d) (%d, %lld, %d)\n",
			out2tnr->inFbNf[0].fd, out2tnr->inFbNf[0].phyaddr, out2tnr->inFbNf[0].field_type,
			out2tnr->inFbNf[1].fd, out2tnr->inFbNf[1].phyaddr, out2tnr->inFbNf[1].field_type,
			out2tnr->inFbNf[2].fd, out2tnr->inFbNf[2].phyaddr, out2tnr->inFbNf[2].field_type);
		DI_INF("infmt:(%s, %s, %s)\n",
			getDiPixelFormatName(out2tnr->inFormat[0]),
			getDiPixelFormatName(out2tnr->inFormat[1]),
			getDiPixelFormatName(out2tnr->inFormat[2]));
		DI_INF("outfb:(fd, phyaddr) (%d, %lld) (%d, %lld)\n",
			out2tnr->outFb[0].fd, out2tnr->outFb[0].phyaddr,
			out2tnr->outFb[1].fd, out2tnr->outFb[1].phyaddr);
		DI_INF("outfmt:(%s, %s)\n",
			getDiPixelFormatName(out2tnr->outFormat[0]),
			getDiPixelFormatName(out2tnr->outFormat[1]));
		DI_INF("tnr: mode:%d level:%d\n", out2tnr->tnrMode.mode,
						  out2tnr->tnrMode.level);
	} else if (hal->diMode == DIV3X_MODE_OUT2) {
		struct fb_para_out2 *out2
				= (struct fb_para_out2 *)para;
		DI_INF("ttf:%d\n", out2->topFieldFirst);
		DI_INF("infb:(fd, phyaddr, field_type) (%d, %lld, %d) (%d, %lld, %d) (%d, %lld, %d)\n",
			out2->inFb[0].fd, out2->inFb[0].phyaddr, out2->inFb[0].field_type,
			out2->inFb[1].fd, out2->inFb[1].phyaddr, out2->inFb[1].field_type,
			out2->inFb[2].fd, out2->inFb[2].phyaddr, out2->inFb[2].field_type);
		DI_INF("infbnf:(fd, phyaddr, field_type) (%d, %lld, %d) (%d, %lld, %d) (%d, %lld, %d)\n",
			out2->inFbNf[0].fd, out2->inFbNf[0].phyaddr, out2->inFbNf[0].field_type,
			out2->inFbNf[1].fd, out2->inFbNf[1].phyaddr, out2->inFbNf[1].field_type,
			out2->inFbNf[2].fd, out2->inFbNf[2].phyaddr, out2->inFbNf[2].field_type);
		DI_INF("outfb:(fd, phyaddr) (%d, %lld) (%d, %lld)\n",
			out2->outFb[0].fd, out2->outFb[0].phyaddr,
			out2->outFb[1].fd, out2->outFb[1].phyaddr);
		DI_INF("outfmt:(%s, %s)\n",
			getDiPixelFormatName(out2->outFormat[0]),
			getDiPixelFormatName(out2->outFormat[1]));
	} else if (hal->diMode == DIV3X_MODE_OUT1_TNR) {
		struct fb_para_out1_tnr *out1tnr
				= (struct fb_para_out1_tnr *)para;
		DI_INF("ttf:%d\n", out1tnr->topFieldFirst);
		DI_INF("infb:(fd, phyaddr, field_type) (%d, %lld, %d) (%d, %lld, %d)\n",
			out1tnr->inFb[0].fd, out1tnr->inFb[0].phyaddr, out1tnr->inFb[0].field_type,
			out1tnr->inFb[1].fd, out1tnr->inFb[1].phyaddr, out1tnr->inFb[1].field_type);
		DI_INF("infbNf:(fd, phyaddr, field_type) (%d, %lld, %d) (%d, %lld, %d)\n",
			out1tnr->inFbNf[0].fd, out1tnr->inFbNf[0].phyaddr, out1tnr->inFbNf[0].field_type,
			out1tnr->inFbNf[1].fd, out1tnr->inFbNf[1].phyaddr, out1tnr->inFbNf[1].field_type);
		DI_INF("infmt:(%s, %s)\n",
			getDiPixelFormatName(out1tnr->inFormat[0]),
			getDiPixelFormatName(out1tnr->inFormat[1]));
		DI_INF("outfb:(fd, phyaddr) (%d, %lld)\n",
			out1tnr->outFb.fd, out1tnr->outFb.phyaddr);
		DI_INF("outfmt:(%s)\n",
			getDiPixelFormatName(out1tnr->outFormat));
		DI_INF("tnr: mode:%d level:%d\n", out1tnr->tnrMode.mode,
						  out1tnr->tnrMode.level);
	} else if (hal->diMode == DIV3X_MODE_OUT1) {
		struct fb_para_out1_tnr *out1
				= (struct fb_para_out1_tnr *)para;
		DI_INF("ttf:%d\n", out1->topFieldFirst);
		DI_INF("infb:(fd, phyaddr, field_type) (%d, %lld, %d) (%d, %lld, %d)\n",
			out1->inFb[0].fd, out1->inFb[0].phyaddr, out1->inFb[0].field_type,
			out1->inFb[1].fd, out1->inFb[1].phyaddr, out1->inFb[1].field_type);
		DI_INF("infbNf:(fd, phyaddr, field_type) (%d, %lld, %d) (%d, %lld, %d)\n",
			out1->inFb[0].fd, out1->inFbNf[0].phyaddr, out1->inFbNf[0].field_type,
			out1->inFb[1].fd, out1->inFbNf[1].phyaddr, out1->inFbNf[1].field_type);
		DI_INF("infmt:(%s, %s)\n",
			getDiPixelFormatName(out1->inFormat[0]),
			getDiPixelFormatName(out1->inFormat[1]));
		DI_INF("outfb:(fd, phyaddr) (%d, %lld)\n",
			out1->outFb.fd, out1->outFb.phyaddr);
	} else if (hal->diMode == DIV3X_MODE_ONLY_TNR) {
		struct fb_para_only_tnr *onlytnr
				= (struct fb_para_only_tnr *)para;
		DI_INF("infb:(fd, phyaddr) (%d, %lld) (%d, %lld)\n",
			onlytnr->inFb[0].fd, onlytnr->inFb[0].phyaddr,
			onlytnr->inFb[1].fd, onlytnr->inFb[1].phyaddr);
		DI_INF("infmt:(%s, %s)\n",
			getDiPixelFormatName(onlytnr->inFormat[0]),
			getDiPixelFormatName(onlytnr->inFormat[1]));
		DI_INF("outfb:(fd, phyaddr) (%d, %lld)\n",
			onlytnr->outTnrFb.fd, onlytnr->outTnrFb.phyaddr);
		DI_INF("outfmt:(%s)\n",
			getDiPixelFormatName(onlytnr->outTnrFormat));
		DI_INF("tnr: mode:%d level:%d\n", onlytnr->tnrMode.mode,
						  onlytnr->tnrMode.level);
	} else if (hal->diMode == DIV3X_MODE_BOB) {
		struct fb_para_bob *bob
				= (struct fb_para_bob *)para;
		DI_INF("ttf:%d\n", bob->topFieldFirst);
		DI_INF("infb:(fd, phyaddr, field_type) (%d, %lld, %d)\n",
			bob->inFb[0].fd, bob->inFb[0].phyaddr, bob->inFb[0].field_type);
		DI_INF("infbNf:(fd, phyaddr, field_type) (%d, %lld, %d)\n",
			bob->inFb[0].fd, bob->inFbNf[0].phyaddr, bob->inFbNf[0].field_type);
		DI_INF("infmt:(%s)\n",
			getDiPixelFormatName(bob->inFormat[0]));
		DI_INF("outfb:(fd, phyaddr) (%d, %lld)\n",
			bob->outFb.fd, bob->outFb.phyaddr);
	}
	DI_INF("\n\n\n");
}

int diFrameBuffer(DIDevice deinterlace, void *fb_para)
{
	int ret = 0;
	struct fb_para_out2_tnr *paraOut2Tnr;
	struct fb_para_out2 *paraOut2;
	struct fb_para_out1_tnr *paraOut1Tnr;
	struct fb_para_out1 *paraOut1;
	struct fb_para_only_tnr *paraOnlyTnr;
	struct fb_para_bob *paraBob;

	struct div3xx_hal *hal = (struct div3xx_hal *)deinterlace;

	dumpDiFrambufferInfo(hal, fb_para);
	switch(hal->diMode) {
	case DIV3X_MODE_BOB:
		paraBob = (struct fb_para_bob *)fb_para;
		ret = diFbBob(hal, paraBob);
		break;
	case DIV3X_MODE_OUT2_TNR:
		paraOut2Tnr = (struct fb_para_out2_tnr *)fb_para;
		ret = diFbOut2Tnr(hal, paraOut2Tnr);
		break;
	case DIV3X_MODE_OUT2:
		paraOut2 = (struct fb_para_out2 *)fb_para;
		ret = diFbOut2(hal, paraOut2);
		break;

	case DIV3X_MODE_OUT1_TNR:
		paraOut1Tnr = (struct fb_para_out1_tnr *)fb_para;
		ret = diFbOut1Tnr(hal, paraOut1Tnr);
		break;
	case DIV3X_MODE_OUT1:
		paraOut1 = (struct fb_para_out1 *)fb_para;
		ret = diFbOut1(hal, paraOut1);
		break;
	case DIV3X_MODE_ONLY_TNR:
		paraOnlyTnr = (struct fb_para_only_tnr *)fb_para;
		ret = diFbOnlyTnr(hal, paraOnlyTnr);
		break;

	default:
		DI_ERR("invalid hal->diMode:%d\n", hal->diMode);
		return -1;
	}

	if (ret < 0) {
		DI_ERR("di FB failed\n\n");
		return -1;
	}

	hal->processCount++;
	if ((hal->diMode == DIV3X_MODE_OUT2_TNR) && (hal->processCount == 0))
		hal->processCount = 2;
	else if ((hal->diMode == DIV3X_MODE_OUT1_TNR) && (hal->processCount == 0))
		hal->processCount = 1;

	return 0;
}

int diExit(DIDevice deinterlace){
	struct div3xx_hal *hal = (struct div3xx_hal *)deinterlace;
	int client_number = 0;

	sunxi_di_control(DI_IOC_DESTROY, &client_number);
	sunxi_di_release();
	hal_free(hal);
	call_open_iommu();

	return 0;
}
