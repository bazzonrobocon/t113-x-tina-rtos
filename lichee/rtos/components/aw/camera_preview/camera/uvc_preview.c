/*
 * Copyright (C) 2017 XRADIO TECHNOLOGY CO., LTD. All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *	1. Redistributions of source code must retain the above copyright
 *	   notice, this list of conditions and the following disclaimer.
 *	2. Redistributions in binary form must reproduce the above copyright
 *	   notice, this list of conditions and the following disclaimer in the
 *	   documentation and/or other materials provided with the
 *	   distribution.
 *	3. Neither the name of XRADIO TECHNOLOGY CO., LTD. nor the names of
 *	   its contributors may be used to endorse or promote products derived
 *	   from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "camera_cmd.h"

#if defined(CONFIG_USB_CAMERA)
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include "uvcvideo.h"

/*********************
 *      DEFINES
 *********************/

/* Color Depth: 16 (RGB565), 32 (ARGB8888) */
#define LCD_COLOR_DEPTH 32

/* The Resolution of Camera Acquisition */
#define UVC_WIDTH  320
#define UVC_HEIGHT 240

/* Preview Size: 0 (Non-full Screen), 1 (Full Screen) */
#define FULL_SCREEN_PREVIEW 1

/* The Resolution of Non-full Screen Display */
#define PREVIEW_WIDTH  320
#define PREVIEW_HEIGHT 240

#ifdef CONFIG_DRIVERS_SPILCD
#define FB_NUM 1
#else
#define FB_NUM 2
#endif

/**********************
 *  STATIC VARIABLES
 **********************/
static struct camera_preview_buf src_buf;
static struct camera_preview_buf dst_buf;
static int fbindex = 0;

static int save_frame_to_file(void *str, void *start, int length)
{
	FILE *fp = NULL;

	fp = fopen(str, "wb+"); //save more frames
	if (!fp) {
		printf(" Open %s error\n", (char *)str);

		return -1;
	}

	if (fwrite(start, length, 1, fp)) {
		fclose(fp);

		return 0;
	} else {
		printf(" Write file fail (%s)\n", strerror(errno));
		fclose(fp);

		return -1;
	}

	return 0;
}

static int uvc_preview_init(void)
{
    memset(&src_buf, 0, sizeof(struct camera_preview_buf));
    memset(&dst_buf, 0, sizeof(struct camera_preview_buf));

	src_buf.width  = UVC_WIDTH;
	src_buf.height = UVC_HEIGHT;
	src_buf.fmt    = CP_FMT_YUYV;
	printf("[%s]: uvc_width = %d, uvc_height = %d, uvc format is YUYV\n", __func__,
			src_buf.width, src_buf.height);

	if (FULL_SCREEN_PREVIEW)
		display_get_sizes(&(dst_buf.width), &(dst_buf.height));
	else {
		dst_buf.width  = PREVIEW_WIDTH;
		dst_buf.height = PREVIEW_HEIGHT;
	}
	dst_buf.fmt  = CP_FMT_RGB;
	dst_buf.size = dst_buf.width * dst_buf.height * LCD_COLOR_DEPTH / 8;
	dst_buf.addr = hal_malloc_coherent(dst_buf.size * FB_NUM);
	if (dst_buf.addr == NULL) {
		printf("[%s,%d]: malloc dst_buf size :%d failed!\n", __func__, __LINE__, dst_buf.size * FB_NUM);
		return -1;
	}
	memset((void *)dst_buf.addr, 0, dst_buf.size);
	hal_dcache_clean((unsigned long)dst_buf.addr, dst_buf.size * FB_NUM);

	printf("[%s]: lcd_width = %d, lcd_height = %d, lcd color depth is %d\n", __func__,
			dst_buf.width, dst_buf.height, LCD_COLOR_DEPTH);
	printf("[%s]: disp addr = 0x%08x, disp size = %d*%d\n", __func__, dst_buf.addr, dst_buf.size, FB_NUM);

	return 0;
}

static void uvc_preview_to_lcd(void *uvc_buf, unsigned long uvc_size)
{
	src_buf.addr = uvc_buf;
	src_buf.size = uvc_size;
	cp_dbg("[%s]: uvc addr = 0x%08x uvc size = %d\n", __func__, src_buf.addr, src_buf.size);

	format_convert(src_buf, dst_buf, fbindex);

	display_pan_display(dst_buf, fbindex);

	if (FB_NUM == 2)
		fbindex = !fbindex;
}

int uvc_preview_test(int argc, const char **argv)
{
	int fd;
	struct v4l2_capability cap;      /* Query device capabilities */
	struct v4l2_streamparm parms;    /* set streaming parameters */
	struct v4l2_format fmt;          /* try a format */
	struct v4l2_requestbuffers req;  /* Initiate Memory Mapping or User Pointer I/O */
	struct v4l2_buffer buf;          /* Query the status of a buffer */
	enum v4l2_buf_type type;
	int n_buffers;
	char source_data_path[64];
	int np;

	if (uvc_preview_init()) {
		return -1;
	}

	/* 1.open /dev/videoX node */
	fd = open("/dev/video", O_RDWR);

	/* 2.Query device capabilities */
	memset(&cap, 0, sizeof(cap));
	if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
		printf(" Query device capabilities fail!!!\n");
	} else {
		printf(" Querey device capabilities succeed\n");
		printf(" cap.driver=%s\n", cap.driver);
		printf(" cap.card=%s\n", cap.card);
		printf(" cap.bus_info=%s\n", cap.bus_info);
		printf(" cap.version=0x%08x\n", cap.version);
		printf(" cap.capabilities=0x%08x\n", cap.capabilities);
	}

	/* 7.set streaming parameters */
	memset(&parms, 0, sizeof(struct v4l2_streamparm));
	parms.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	parms.parm.capture.timeperframe.numerator = 1;
	parms.parm.capture.timeperframe.denominator = 30;
	if (ioctl(fd, VIDIOC_S_PARM, &parms) < 0) {
		printf(" Setting streaming parameters failed, numerator:%d denominator:%d\n",
			   parms.parm.capture.timeperframe.numerator,
			   parms.parm.capture.timeperframe.denominator);
		close(fd);
		return -1;
	}

	/* 9.set the data format */
	memset(&fmt, 0, sizeof(struct v4l2_format));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = UVC_WIDTH;
	fmt.fmt.pix.height = UVC_HEIGHT;
	/* fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG; */
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

	if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
		printf(" 123setting the data format failed!\n");
		close(fd);
		return -1;
	}

	/* 10.Initiate Memory Mapping or User Pointer I/O */
	memset(&req, 0, sizeof(struct v4l2_requestbuffers));
	req.count = 2;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;
	if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
		printf(" VIDIOC_REQBUFS failed\n");
		close(fd);
		return -1;
	}

	/* 11.Exchange a buffer with the driver */
	for (n_buffers = 0; n_buffers < req.count; n_buffers++) {
		memset(&buf, 0, sizeof(struct v4l2_buffer));

		buf.index = n_buffers;
		if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
			printf(" VIDIOC_QBUF error\n");

			close(fd);
			return -1;
		}
	}

	/* streamon */
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(fd, VIDIOC_STREAMON, &type) == -1) {
		printf(" VIDIOC_STREAMON error! %s\n", strerror(errno));
	} else
		printf(" stream on succeed\n");

	np = 0;
//	while (np < 5) {
	while (1) {
		printf(" camera capture num is [%d]\n", np);

		/* wait uvc frame */
		memset(&buf, 0, sizeof(struct v4l2_buffer));

		if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
			printf(" VIDIOC_DQBUF error\n");

			goto EXIT;
		} else
			printf("*****DQBUF[%d] FINISH*****\n", buf.index);

#if 0
		sprintf(source_data_path, "/data/source_frame_%d.bin", np);
		save_frame_to_file(source_data_path, (void *)buf.mem_buf, buf.length);
#else
		uvc_preview_to_lcd((void *)buf.mem_buf, buf.length);
#endif

		if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
			printf(" VIDIOC_QBUF error\n");

			goto EXIT;
		} else
			printf("************QBUF[%d] FINISH**************\n\n", buf.index);

		np++;
	}

	printf("\n\n Capture thread finish\n");

EXIT:
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ioctl(fd, VIDIOC_STREAMOFF, &type);

	memset(&req, 0, sizeof(struct v4l2_requestbuffers));
	req.count = 0;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;
	ioctl(fd, VIDIOC_REQBUFS, &req);
	hal_free_coherent(dst_buf.addr);

	close(fd);

	return 0;
}

#endif
