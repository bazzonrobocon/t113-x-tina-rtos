#ifndef H_FASTBOOT_COM_H
#define H_FASTBOOT_COM_H

#include <sunxi_hal_common.h>
#include <hal_queue.h>

/* STACK */
#define STACK_FASTBOOT_THRED				(4096)
#define STACK_FASTBOOT_DISPLAY_INT_THRED	(1024)
#define STACK_FASTBOOT_STATUS_THRED			(1024*2)

/* PRIORITY */
#define PRIORITY_FASTBOOT_THRED	(30)
#define PRIORITY_FASTBOOT_DISPLAY_INT_THRED	(29)
#define PRIORITY_FASTBOOT_STATUS_THRED	(29)

/* msg type */
#define	SIGNAL_TYPE_REVERSE_STATUS (0)
#define SIGNAL_TYPE_DISP_DRIVER_REALY	(1)
#define	SIGNAL_TYPE_DISP_RPMSG_REALY  (2)
#define	SIGNAL_TYPE_CAR_REVERSE_RPMSG_REALY  (3)
#define	ARM_REVERSE_SIGNAL_TYPE  (4)
#define	SIGNAL_TYPE_VIN_RPMSG_REALY (5)
#define	SIGNAL_TYPE_DI_RPMSG_REALY (6)

/* queue size */
#define CONFIG_FASTBOOT_COM_QUEUE_SIZE 	(30)

#define CAR_MAX_CH 4

struct fastboot_com_queue_t {
	int type;
	int disp_driver_ready;
	int disp_rpmsg_ready;
	int vin_driver_rpmsg_ready;
	int di_driver_rpmsg_ready;
	int car_reverse_rpmsg_ready;
	int reverse_status;
	int arm_car_reverse_status;
};

extern u32 loglevel_debug;

#define CAR_REVERSE_PRINT(fmt, ...)			printf("[%s]-<%d>:" fmt, __func__, __LINE__, ##__VA_ARGS__);
#define CAR_REVERSE_ERR(fmt, ...)			printf("[CAR_ERR]-[%s]-<%d>: "fmt, __func__, __LINE__, ##__VA_ARGS__)
#define CAR_REVERSE_WRN(fmt, ...)			printf("[CAR_WRN]-[%s]-<%d>: "fmt, __func__, __LINE__, ##__VA_ARGS__)
#define CAR_REVERSE_INFO(fmt, ...)			printf("[CAR_INFO]-[%s]-<%d>: "fmt, __func__, __LINE__, ##__VA_ARGS__)

#define CAR_DRIVER_DBG(fmt, ...) \
			do { \
				if (loglevel_debug & 0x1) \
					printf("[CAR_DRVIVER]-[%s]-<%d>: "fmt, __func__, __LINE__, ##__VA_ARGS__); \
			} while (0)

#define BUFFER_POOL_DBG(fmt, ...) \
			do { \
				if (loglevel_debug & 0x2) \
					printf("[CAR_BUFFER_POOL]-[%s]-<%d>: "fmt, __func__, __LINE__, ##__VA_ARGS__); \
			} while (0)

#define VIDEO_SOURCE_DBG(fmt, ...) \
			do { \
				if (loglevel_debug & 0x4) \
					printf("[CAR_VIDEO_SOURCE]-[%s]-<%d>: "fmt, __func__, __LINE__, ##__VA_ARGS__); \
			} while (0)

#define PREVIEW_DISPLAY_DBG(fmt, ...) \
			do { \
				if (loglevel_debug & 0x8) \
					printf("[CAR_PREVIEW_DISPLAY]-[%s]-<%d>: "fmt, __func__, __LINE__, ##__VA_ARGS__); \
			} while (0)

#define PREVIEW_ENHANCER_DBG(fmt, ...) \
			do { \
				if (loglevel_debug & 0x10) \
					printf("[CAR_PREVIEW_ENHANCER]-[%s]-<%d>: "fmt, __func__, __LINE__, ##__VA_ARGS__); \
			} while (0)

#define PREVIEW_ROTATOR_DBG(fmt, ...) \
			do { \
				if (loglevel_debug & 0x10) \
					printf("[CAR_PREVIEW_ROTATOR]-[%s]-<%d>: "fmt, __func__, __LINE__, ##__VA_ARGS__); \
			} while (0)

#define PREVIEW_AUXLINE_DBG(fmt, ...) \
			do { \
				if (loglevel_debug & 0x20) \
					printf("[CAR_AUX_LINE]-[%s]-<%d>: "fmt, __func__, __LINE__, ##__VA_ARGS__); \
			} while (0)



enum car_reverse_status {
	CAR_REVERSE_NULL  = 0,
	CAR_REVERSE_START = 1,
	CAR_REVERSE_STOP  = 2,
	CAR_REVERSE_HOLD  = 3,
	CAR_REVERSE_BUSY  = 4,
	ARM_TRY_GET_CRTL  = 5,
	ARM_GET_CRTL_OK  = 6,
	ARM_GET_CRTL_FAIL  = 7,
	CAR_VIDEO_START  = 8,
	CAR_VIDEO_HOLD  = 9,
	CAR_REVERSE_STATUS_MAX,
};

enum g2d_buffer_enum {
	G2D_BUF_ROTATE = 0,
	G2D_BUF_SCALER,
	G2D_BUF_AUXLINE,
	G2D_BUF_MAX,
};

enum car_reverse_ioctl_cmd {
	CMD_NONE = 0,
	CMD_CAR_REVERSE_FORCE_SET_STATE = 0x1,
	CMD_NUM,
};

struct preview_params {
	int video_channel;
	int src_width;
	int src_height;
	int src_size_adaptive;
	int screen_width;
	int screen_height;
	int discard_frame;

	int rotation;
	int di_used;
	int g2d_used;
	int aux_line_type;
	int format;
	int input_src;

	/*
	0: singal preview display
	>0:muti preview display
	*/
	int oview_type;

	int aux_angle;
	int aux_lr;
};

enum car_buffer_type_t {
	DI_BUFFER  = 1,
	G2D_BUFFER = 2,
	DISPLAY_BUFFER  = 3,
	RESET_DISPLAY_BUFFER_MSG = 4,
	TYPE_BUFFER_MAX,
};

struct fastboot_private_data {
	struct preview_params config;
	struct buffer_pool *buffer_pool;
	struct buffer_pool *bufferOv_pool[CAR_MAX_CH];
	struct buffer_node *buffer_auxiliary_line;
	struct buffer_node *buffer_di[2];
	/* 0:for rotate  1:for scaler  2:for auxiliary_line */
	struct buffer_node *buffer_g2d[G2D_BUF_MAX];
	/* display buffer node */
	struct buffer_node *buffer_display;

	int status;
	int last_status;
	int discard_frame;
	int standby;
	int first_buf_unready;
	int disp_driver_ready;
	int disp_preview_flag;
	int disp_rpmsg_ready;
	int disp_first_flag;
	int car_reverse_rpmsg_ready;
	int arm_car_reverse_status;
	int vin_driver_rpmsg_ready;
	int di_driver_rpmsg_ready;

	int car_reverse_enable;
	int video_enable;
};

#if defined CONFIG_VIDEO_SUNXI_VIN_SPECIAL
extern void vin_return_control(int type);
#endif
extern hal_queue_t fastboot_com_queue;
#endif


