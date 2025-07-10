
#ifndef _CJ_PLATFORM_CFG_H_
#define _CJ_PLATFORM_CFG_H_

#include <stdio.h>
#include "hal_gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * reg addr
 */
//#define PERIPH_BASE         (0x40000000U)
//#define CSI_BASE            (PERIPH_BASE + 0x00007000)
//#define JPEG_BASE           (PERIPH_BASE + 0x00007400)
#define CSI_BASE            (0x40300000)
#define JPEG_BASE           (0x40300400)

//CSI GPIO
#define CSI_PIN_NUM		12
struct csi_pinctl {
	gpio_pin_t csi_gpio;
	unsigned char func;
};

#define CSI_JPEG_IRQn	109  // rv
//#define CSI_JPEG_IRQn	93  // arm

//jpeg
#define JPEG_SRAM_SWITCH_THR_W	720
#define JPEG_SRAM_SWITCH_THR_H	576
#define SUPPORT_JPEG	1
#define SUPPORT_JPEG_SHARE_64K	0

#ifdef __cplusplus
}
#endif
#endif
