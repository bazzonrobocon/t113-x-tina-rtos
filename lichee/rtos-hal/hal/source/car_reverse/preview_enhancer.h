#ifndef PREVIEW_ENHANCER_H
#define PREVIEW_ENHANCER_H

#include "fastboot_com.h"
#include "buffer_pool.h"

int preview_enhancer_init(struct preview_params *params);
int preview_enhancer_exit(void);
int preview_image_enhance(struct buffer_node *frame, struct buffer_node **di_frame, int src_width,
			 int src_height, int format);
int preview_enhancer_rpmsg_init();

#endif  //PREVIEW_ENHANCER_H