#ifndef PREVIEW_DISPLAY_H
#define PREVIEW_DISPLAY_H

#include "fastboot_com.h"
#include "buffer_pool.h"

struct rect {
	int x, y;
	int w, h;
};

int car_reverse_preview_display_init(struct preview_params *p, void *aux_line);
int boot_animation_preview_display_init(void);
int preview_display_init(void);
int preview_output_init(void);
int preview_output_exit(void);
void preview_display(void);
void preview_layer_config_update(struct buffer_node *frame);
void aux_line_layer_config_update(struct buffer_node *frame);
int preview_output_config_get(struct preview_params *config, struct rect *display);
void preview_display_tmp_test(unsigned long addr);
int preview_display_rpmsg_init(void);

int preview_image_rotate(struct buffer_node *frame, struct buffer_node **rotate, int src_w,
			 int src_h, int angle, int format);

#endif
