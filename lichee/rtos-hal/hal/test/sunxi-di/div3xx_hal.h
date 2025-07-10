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
#ifndef _DIV3X_HAL_H
#define _DIV3X_HAL_H

#include "div3xx_uapi.h"
#include "ioctl.h"

#define u8	unsigned char
#define u16	unsigned short
#define u32	unsigned int
#define u64	unsigned long long

enum {
	DI_DIT_INTP_MODE_INVALID = 0x0,
	DI_DIT_INTP_MODE_WEAVE,
	DI_DIT_INTP_MODE_BOB,
	DI_DIT_INTP_MODE_MOTION,
};

enum {
	DI_DIT_OUT_0FRAME = 0x0,
	DI_DIT_OUT_1FRAME,
	DI_DIT_OUT_2FRAME,
};

enum {
	DI_TNR_MODE_INVALID = 0,
	DI_TNR_MODE_ADAPTIVE,
	DI_TNR_MODE_FIX,
};

enum {
	DI_TNR_LEVEL_HIGH = 0,
	DI_TNR_LEVEL_MIDDLE,
	DI_TNR_LEVEL_LOW,
};

struct di_version {
	u32 version_major;
	u32 version_minor;
	u32 version_patchlevel;

	u32 ip_version;
};

struct di_timeout_ns {
	u64 wait4start;
	u64 wait4finish;
};

/*
 * @intp_mode: see enum DI_DIT_INTP_MODE_XXX.
 * @out_frame_mode: see enum DI_DIT_OUT_XFRAME.
 */
struct di_dit_mode {
	u32 intp_mode;
	u32 out_frame_mode;
};

struct di_tnr_mode {
	unsigned int mode;
	unsigned int level;
};

struct di_fmd_enable {
	u32 en;
};

/*
 * support dma_buf method or phy_addr_buf method.
 * 1.On dma_buf method:
 *     @y_addr is address-offset of luma-buf to dma_addr;
 *     @cb_addr/@cr_addr is address-offset of chroma-buf to dma_addr.
 * 2.On phy_addr_buf method:
 *     @y_addr is physical address of luma-buf;
 *     @cb_addr/@cr_addr is physical address of chroma-buf.
 *
 * @ystride: line stride of luma buf. unit: byte.
 * @cstride: line stride of chroma buf. unit: byte.
 */
struct di_buf {
	u64 y_addr;
	u64 cb_addr;
	u64 cr_addr;
	u32 ystride;
	u32 cstride;
};

/*
 * @format: see DRM_FORMAT_XXX in drm_fourcc.h.
 * @dma_buf_fd: dma buf fd that from userspace.
 *    @dma_buf_fd must be invalid(<0) on phy_addr_buf method.
 * @size.width,@size.height: size of pixel datas of image. unit: pixel.
 */
struct di_fb {
	u32 format;
	int dma_buf_fd;
	struct di_buf buf;
	struct di_size size;
#ifdef CONF_DI_SINGEL_FILE
	u8 field_type;
#endif
};

/*
 * @base_field:
 *     0: top_field;
 *     1: bottom_field.
 */
struct di_process_fb_arg {
	unsigned int client_number;

	u8 is_interlace; /*must be set*/
	u8 is_pulldown; /* fixme: define enum value, not used in driver*/
	u8 top_field_first; /*recorrected by drivers*/
	u8 base_field; /*top_field or bottom_field, if use BOB, must be set*/

	struct di_fb in_fb0;
	struct di_fb in_fb1;
	struct di_fb in_fb2;
#ifdef CONF_DI_SINGEL_FILE
	struct di_fb in_fb0_nf; //*_nf, next frame
	struct di_fb in_fb1_nf; //*_nf, next frame
	struct di_fb in_fb2_nf; //*_nf, next frame
#endif
	struct di_fb out_dit_fb0;
	struct di_fb out_dit_fb1;
	struct di_fb out_tnr_fb0;

};

struct di_mem_arg {
	unsigned int size;
	unsigned int handle;
	u64 phys_addr;
};

struct di_demo_crop_arg {
	struct di_rect dit_demo;
	struct di_rect tnr_demo;
};

#define DI_IOC_MAGIC 'D'
#define DI_IO(nr)          _IO(DI_IOC_MAGIC, nr)
#define DI_IOR(nr, size)   _IOR(DI_IOC_MAGIC, nr, size)
#define DI_IOW(nr, size)   _IOW(DI_IOC_MAGIC, nr, size)
#define DI_IOWR(nr, size)  _IOWR(DI_IOC_MAGIC, nr, size)
#define DI_IOCTL_NR(n)     _IOC_NR(n)

#define DI_IOC_GET_VERSION    DI_IOR(0x0, struct di_version)
#define DI_IOC_RESET          DI_IO(0x1)
#define DI_IOC_CHECK_PARA     DI_IO(0x2)
#define DI_IOC_SET_TIMEOUT    DI_IOW(0x3, struct di_timeout_ns)
#define DI_IOC_SET_VIDEO_SIZE DI_IOW(0x4, struct di_size)
#define DI_IOC_SET_DIT_MODE   DI_IOW(0x5, struct di_dit_mode)
#define DI_IOC_SET_TNR_MODE   DI_IOW(0x6, struct di_tnr_mode)
#define DI_IOC_SET_FMD_ENABLE DI_IOW(0x7, struct di_fmd_enable)
#define DI_IOC_PROCESS_FB     DI_IOW(0x8, struct di_process_fb_arg)
#define DI_IOC_SET_VIDEO_CROP DI_IOW(0x9, struct di_rect)
#define DI_IOC_MEM_REQUEST    DI_IOWR(0x10, struct di_mem_arg)
#define DI_IOC_MEM_RELEASE    DI_IOWR(0x11, struct di_mem_arg)
#define DI_IOC_SET_DEMO_CROP  DI_IOW(0x12, struct di_demo_crop_arg)
#define DI_IOC_GET_TNRPARA    DI_IOR(0x13, struct tnr_module_param_t)
#define DI_IOC_SET_TNRPARA    DI_IOW(0x14, struct tnr_module_param_t)
#define DI_IOC_DESTROY     DI_IOW(0x15, int)
#define DI_IOC_GET_CLIENT     DI_IOR(0x16, int)

extern unsigned int di_device_get_debug_mode(void);
#endif