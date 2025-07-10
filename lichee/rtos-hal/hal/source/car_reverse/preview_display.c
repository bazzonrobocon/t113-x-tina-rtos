#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <hal_thread.h>

#include <hal_osal.h>
#include <hal_sem.h>
#include <hal_cache.h>
#include <hal_msgbox.h>
#include <openamp/sunxi_helper/openamp.h>
#include <hal_hwspinlock.h>
#include <hal_cache.h>
#include <video/sunxi_display2.h>

#include "buffer_pool.h"
#include "fastboot_com.h"
#include "preview_display.h"

#define RPMSG_SERVICE_NAME		"sunxi,rpmsg_preview"
#define PREVIEW_PACKET_MAGIC		0x10244021

#define SPINLOCK_DISP_PARA		(0)
#define __section_t(s)			__attribute((__section__(#s)))
#define __disp2_para_section		__section_t(.disp_para)

//#define PRVIEW_DEBUG

#ifdef PRVIEW_DEBUG
#define debug(fmt, arg...) printf(fmt, ##arg)
#else
#define debug(fmt, arg...) do{}while(0);
#endif

/*
 Usage:
	1. preview_display_init
	2. preview_display_rpmsg_init() after openamp_init()
	3. boot_animation_preview_display_init or/and car_reverse_preview_display_init
	4. preview_output_init to prepare display
	5. preview_layer_config_update to set param for new frame
	6. preview_display to show the preview frame configured by preview_layer_config_update
	7. repeat 5 and 6 to display continue
	8. preview_output_exit to stop and exit display
	9. repeat 4/5/6/7/8 to perform another display work

	1 must be done before 2 and 3
	2/3 can be excuted async
*/

enum preview_packet_type {
	PREVIEW_START,
	PREVIEW_START_ACK,
	PREVIEW_STOP,
	PREVIEW_STOP_ACK,
};

struct preview_packet {
	u32 magic;
	u32 type;
};

struct layer_reg_info {
	u32 address_reg[3];
	u32 alpha_reg;
	u32 alpha_shift;
} __attribute__ ((packed));

struct display_info {
	u32 line_reg;
	u32 line_shift;
	u32 line_mask;
	u32 safe_line;
	u32 sleep_us;
	struct layer_reg_info rgb; /* auxline or logo layer*/
	u32 fill_color_en_reg;
	u32 fill_color_en_bit;
	u32 fill_color_reg;
	u32 logo_addr;
} __attribute__ ((packed));


struct amp_preview_driver_info {
	struct display_info disp[2];
	struct layer_reg_info layer[4];
} __attribute__ ((packed));

struct preview_display_param {
	u32 src_w;
	u32 src_h;
	u32 fmt;
	u32 src_rot;
	u32 screen_x;
	u32 screen_y;
	u32 screen_w;
	u32 screen_h;
	u32 buffer_addr;
	u32 aux_line_buffer_addr;
} __attribute__ ((packed));

struct preview_exchange_param {
	u32 magic;
	struct preview_display_param from_rtos;
	u8 from_ready;
	struct amp_preview_driver_info to_rtos;
	u8 to_ready;
} __attribute__ ((packed));

struct rpmsg_preview_private {
	hal_sem_t finish;		/* notify remote call finish */
	hal_spinlock_t ack_lock;	/* protect ack */
	hal_mutex_t call_lock;		/* protect status and make sure remote call is exclusive */
	bool init;
	bool aux_line;
	struct preview_packet ack;
	bool init_by_kernel;
	int status;
	struct rpmsg_endpoint *ept;
	struct preview_exchange_param param;
	u32 preview_addr[4][3];
};

extern const unsigned long __disp_para_start__[], __disp_para_end__[];

static struct rpmsg_preview_private preview;

static void disp_para_exchange(struct preview_exchange_param *p, bool get)
{
	bool finish = 0;
	uint32_t size = sizeof(*p);
	unsigned long flush_start_addr = (unsigned long)__disp_para_start__;
	struct preview_exchange_param *tmp = (struct preview_exchange_param *)flush_start_addr;

	do {
		if (hal_hwspin_lock(SPINLOCK_DISP_PARA) == HWSPINLOCK_OK) {
			if (get) {
				hal_dcache_clean_invalidate(flush_start_addr, size);
				memcpy(p, (void *)flush_start_addr, size);
				if (p->magic == PREVIEW_PACKET_MAGIC && p->to_ready) {
					finish = 1;
					tmp->magic = 0;
					printf("get ok \n");
				}
			} else {
				p->from_ready = 1;
				p->magic = PREVIEW_PACKET_MAGIC;
				memcpy((void *)flush_start_addr, p, size);
				printf("put ok \n");
				hal_dcache_clean_invalidate(flush_start_addr, size);
				finish = 1;
			}
			hal_hwspin_unlock(SPINLOCK_DISP_PARA);
		} else {
			printf("disp para get err\n");
			finish = 1;
		}
		if (!finish)
			hal_msleep(1);
	} while (!finish);
}

static void wait_for_update_region(int disp)
{
	struct display_info *info = &preview.param.to_rtos.disp[disp];
	u32 current_line;
	u32 val;
	while (true) {
		val = hal_readl(info->line_reg);
		current_line = (val & info->line_mask) >> info->line_shift;
		if (current_line > info->safe_line)
			hal_udelay(info->sleep_us);
		else
			break;
	};
	while (true) {
		val = hal_readl(info->line_reg);
		current_line = (val & info->line_mask) >> info->line_shift;
		if (current_line <= info->safe_line)
			hal_udelay(info->sleep_us);
		else
			break;
	};
	debug("%d\n",current_line);
}

static int rpmsg_preview_remote_call(enum preview_packet_type type)
{
	int ret;
	struct preview_packet packet;
	enum preview_packet_type ack_type;
	u32 flag;

	/* do noting when linux is not ok */
	if (!preview.ept)
		return 0;

	packet.magic = PREVIEW_PACKET_MAGIC;
	packet.type = type;
	ret = openamp_rpmsg_send(preview.ept, (void *)&packet, sizeof(packet));
	if (ret <  sizeof(packet)) {
		printf("%s rpmsg sent fail\r\n", __FUNCTION__);
		ret = -1;
		goto out;
	}

	ret =  hal_sem_timedwait(preview.finish, MS_TO_OSTICK(1000));
	if (ret) {
		printf("%s sem wait fail\r\n", __FUNCTION__);
//		ret = -1;
//		goto out;
	}
	flag = hal_spin_lock_irqsave(&preview.ack_lock);
	ack_type = preview.ack.type;
	// clear processed packet to allow callback receive a new packet
	memset(&preview.ack, 0, sizeof(preview.ack));
	hal_spin_unlock_irqrestore(&preview.ack_lock, flag);
	if (ack_type != type + 1) {
		printf("%s request %d ack %d \r\n", __FUNCTION__, type, ack_type);
		ret = -1;
	} else {
		printf("remote call %s ok \r\n", type == PREVIEW_START ?
			"preview start" : "preview stop");
	}
out:
	return ret;
}

static int rpmsg_preview_start(void)
{
	hal_mutex_lock(preview.call_lock);
	if (preview.status != PREVIEW_START) {
		preview.init_by_kernel = preview.ept != NULL;
		rpmsg_preview_remote_call(PREVIEW_START);
		preview.status = PREVIEW_START;
	}
	hal_mutex_unlock(preview.call_lock);
	return 0;
}

static int rpmsg_preview_stop(void)
{
	hal_mutex_lock(preview.call_lock);
	if (preview.status != PREVIEW_STOP) {
		rpmsg_preview_remote_call(PREVIEW_STOP);
		preview.status = PREVIEW_STOP;
	}
	hal_mutex_unlock(preview.call_lock);
	return 0;
}

/* receive packet supposed to be a remote call ack */
static int rpmsg_ept_callback(struct rpmsg_endpoint *ept, void *data,
		size_t len, uint32_t src, void *priv)
{
	struct rpmsg_preview_private *preview = priv;
	int ret;
	u32 flag;
	struct preview_packet *pack = data;
	bool wait_for_read = true;

	if (pack->magic != PREVIEW_PACKET_MAGIC || len != sizeof(*pack)) {
		printf("packet invalid magic or size %d %d %x\n", (int)len, (int)(sizeof(*pack)), pack->magic);
		return 0;
	}

	do {
		flag = hal_spin_lock_irqsave(&preview->ack_lock);
		wait_for_read = preview->ack.magic != 0;
		hal_spin_unlock_irqrestore(&preview->ack_lock, flag);
		hal_usleep(100);
	} while (wait_for_read);

	flag = hal_spin_lock_irqsave(&preview->ack_lock);
	memcpy(&preview->ack, data, len);
	hal_spin_unlock_irqrestore(&preview->ack_lock, flag);

	ret = hal_sem_post(preview->finish);
	if (ret != 0) {
		printf("%s failed to post sem\n", __FUNCTION__);
		ret = -1;
		goto out;
	}

	ret = 0;
out:
	return ret;
}

static void rpmsg_unbind_callback(struct rpmsg_endpoint *ept)
{
	printf("Remote endpoint is destroyed\n");
}

//call when preview start
int preview_output_init(void)
{
	rpmsg_preview_start();
	return 0;
}

//call when preview stop
int preview_output_exit(void)
{
	u32 val;
	struct layer_reg_info *info = &preview.param.to_rtos.layer[0];
	struct display_info *disp_info = &preview.param.to_rtos.disp[0];

	wait_for_update_region(0);

	if (preview.init_by_kernel) {
		//rgb channel layer fill with black, enable fill color
		val = hal_readl(disp_info->fill_color_en_reg);
		val |= 1UL << disp_info->fill_color_en_bit;
		hal_writel(val, disp_info->fill_color_en_reg);
		hal_writel(0xff000000, disp_info->fill_color_reg);
	} else {
		//disable fill color
		val = hal_readl(disp_info->fill_color_en_reg);
		val &= ~(1UL << disp_info->fill_color_en_bit);
		hal_writel(val, disp_info->fill_color_en_reg);
		//rgb channal layer addr set to logo addr
		hal_writel(disp_info->logo_addr, disp_info->rgb.address_reg[0]);
	}

	//set preview alpha to 0
	val = hal_readl(info->alpha_reg);
	val &= ~(0xFFUL << info->alpha_shift);
	hal_writel(val, info->alpha_reg);

	//set logo alpha to 0xff
	val = hal_readl(disp_info->rgb.alpha_reg);
	val |= 0xFFUL << disp_info->rgb.alpha_shift;
	hal_writel(val, disp_info->rgb.alpha_reg);

	rpmsg_preview_stop();
	return 0;
}

//call when single preview in (include aux line) to update layer info
void preview_layer_config_update(struct buffer_node *frame)
{
	unsigned long addr = (unsigned long)frame->phy_address;
	int width = preview.param.from_rtos.src_w;
	int height = preview.param.from_rtos.src_h;
	preview.preview_addr[0][0] = addr;
	preview.preview_addr[0][1] = addr + width * height;
	preview.preview_addr[0][2] = addr + width * height * 5 / 4;
}

//call when multi preview layer in (no aux line)
//void preview_Ov_layer_config_update(struct buffer_node **frame_input, int overview_type)
//{
//}

//call when single preview layer with aux line, to update aux line layer info
void aux_line_layer_config_update(struct buffer_node *frame)
{
/* nothing to do for now only support static aux line draw by uboot
	if (preview.aux_line) {
		preview.aux_line_addr = (unsigned long)frame->phy_address;
	} else {
		CAR_REVERSE_ERR("do not update aux line config when aux is not enable\n");
	}*/
}

//commit to display, call when single/multi preview
void preview_display(void)
{
	u32 val;
	struct layer_reg_info *info = &preview.param.to_rtos.layer[0];
	struct display_info *disp_info = &preview.param.to_rtos.disp[0];

	wait_for_update_region(0);

	if (!preview.aux_line) {
		if (!preview.init_by_kernel) {
			// switch from logo to transparent layer
			val = hal_readl(disp_info->fill_color_en_reg);
			val |= 1UL << disp_info->fill_color_en_bit;
			hal_writel(val, disp_info->fill_color_en_reg);
		}
		// fill transparent color
		hal_writel(0xffffff, disp_info->fill_color_reg);
	} else {
		//disable rgb layer fill color
		val = hal_readl(disp_info->fill_color_en_reg);
		val &= ~(1UL << disp_info->fill_color_en_bit);
		hal_writel(val, disp_info->fill_color_en_reg);

		//set rgb layer to auxline addr
		hal_writel(preview.param.from_rtos.aux_line_buffer_addr,
				disp_info->rgb.address_reg[0]);
	}

	//set preview addr
	hal_writel(preview.preview_addr[0][0], info->address_reg[0]);
	hal_writel(preview.preview_addr[0][1], info->address_reg[1]);
	hal_writel(preview.preview_addr[0][2], info->address_reg[2]);

}

void preview_display_tmp_test(unsigned long addr)
{
	int width = preview.param.from_rtos.src_w;
	int height = preview.param.from_rtos.src_h;

	preview.preview_addr[0][0] = addr;
	preview.preview_addr[0][1] = addr + width * height;
	preview.preview_addr[0][2] = addr + width * height * 5 / 4;
	preview_display();
}

int preview_display_rpmsg_init(void)
{
	struct rpmsg_endpoint *ept;

	ept = openamp_ept_open(RPMSG_SERVICE_NAME, 0, RPMSG_ADDR_ANY, RPMSG_ADDR_ANY,
					&preview, rpmsg_ept_callback, rpmsg_unbind_callback);
	if (ept == NULL) {
		printf("Failed to Create Endpoint\r\n");
		return -1;
	}

	/* tell linux our preview status */
	hal_mutex_lock(preview.call_lock);
	preview.ept = ept;
	rpmsg_preview_remote_call(preview.status);
	hal_mutex_unlock(preview.call_lock);
	printf("%s ok\r\n", __FUNCTION__);

	return 0;
}

static int get_format(void)
{
#if defined CONFIG_PREVIEW_SOURCE_FORMAT_NV16
	return DISP_FORMAT_YUV422_SP_UVUV;
#elif defined CONFIG_PREVIEW_SOURCE_FORMAT_NV61
	return DISP_FORMAT_YUV422_SP_VUVU;
#elif defined CONFIG_PREVIEW_SOURCE_FORMAT_NV12
	return DISP_FORMAT_YUV420_SP_UVUV;
#elif defined CONFIG_PREVIEW_SOURCE_FORMAT_NV21
	return DISP_FORMAT_YUV420_SP_VUVU;
#elif defined CONFIG_PREVIEW_SOURCE_FORMAT_YV12
	return DISP_FORMAT_YUV420_P;
#else
#error "fastboot input format is not configured!"
#endif
}

static void preview_display_post_init(void)
{
	struct preview_exchange_param tmp;

	memcpy(&tmp.from_rtos, &preview.param.from_rtos, sizeof(tmp.from_rtos));
	disp_para_exchange(&preview.param, 0);
	disp_para_exchange(&preview.param, 1);
	memcpy(&preview.param.from_rtos, &tmp.from_rtos, sizeof(tmp.from_rtos));
}

int preview_display_init(void)
{
	preview.finish = hal_sem_create(0);

	if (!preview.finish) {
		printf("failed to create sem\r\n");
		return -1;
	}
	preview.call_lock = hal_mutex_create();
	if (!preview.call_lock) {
		printf("failed to create sem\r\n");
		hal_sem_delete(preview.finish);
		return -1;
	}
	preview.status = PREVIEW_STOP;

	preview.param.from_rtos.src_w = CONFIG_PREVIEW_SOURCE_WIDTH;
	preview.param.from_rtos.src_h = CONFIG_PREVIEW_SOURCE_HEIGHT;
	preview.param.from_rtos.fmt = get_format();
	preview.param.from_rtos.screen_x = 0;
	preview.param.from_rtos.screen_y = 0;
	preview.param.from_rtos.screen_w = CONFIG_PREVIEW_SCREEN_WIDTH;
	preview.param.from_rtos.screen_h = CONFIG_PREVIEW_SCREEN_HEIGHT;
#ifdef CONFIG_BUFFER_POOL_VE_MEM_ADDR
//	preview.param.from_rtos.buffer_addr = CONFIG_BUFFER_POOL_VE_MEM_ADDR;
	preview.param.from_rtos.buffer_addr = 0x44000000;/* FIXME */
#endif
#ifdef CONFIG_BUFFER_POOL_MEM_ADDR
	preview.param.from_rtos.buffer_addr = CONFIG_BUFFER_POOL_MEM_ADDR;
#endif
	printf("%s finish\r\n", __func__);
	return 0;
}

int boot_animation_preview_display_init(void)
{
#ifndef CONFIG_DRIVERS_CAR_REVERSE
	preview_display_post_init();
#endif
	printf("%s finish\r\n", __func__);
	return 0;
}

int car_reverse_preview_display_init(struct preview_params *p, void *aux_line)
{
	preview.param.from_rtos.src_rot = p->rotation;
	preview.param.from_rtos.aux_line_buffer_addr = (u32)(unsigned long)aux_line;
	preview.aux_line = p->aux_line_type ? 1 : 0;
	preview_display_post_init();
	printf("%s finish\r\n", __func__);
	return 0;
}

//for user to do a total exit
void preview_display_exit(void)
{
	if (preview.ept)
		openamp_ept_close(preview.ept);
	if (preview.call_lock)
		hal_mutex_delete(preview.call_lock);
	if (preview.finish)
		hal_sem_delete(preview.finish);
//	hal_workqueue_destroy(preview.init_workqueue);
	printf("preview_display_exit\r\n");
}

void test_thread(void *param)
{
	int stop;
	int delay_cnt = 0;
	int delay_time = 0;
	bool x = 0;
	delay_time = (rand() % 100) * 100;
	struct buffer_node frame;
	frame.phy_address = (void *)0x61400000;

	for(;;)
	{
		stop = *(int *)(param);
		if (delay_cnt++ >= delay_time && !stop) {
			delay_cnt = 0;
			delay_time = (rand() % 100) * 10;
			if (delay_time < 150) {
				delay_time = 0;
			}
			CAR_REVERSE_INFO("change status time %d\n", delay_time);
			if (x) {
				x = 0;
				preview_output_init();
				preview_layer_config_update(&frame);
				preview_display();
			} else {
				x = 1;
				preview_output_exit();
			}
		}
		hal_msleep(1);
	}

}

static void ageing_test(void) {
	static int init;
	static int stop;
	void *thread;
	if (init == 0) {
		thread = hal_thread_create(test_thread, &stop, \
						"disp test thread", 1 * 1024, 29);
		if (thread != NULL)
			hal_thread_start(thread);
		init = 1;
	}
	if (stop)
		stop = 0;
	else
		stop = 1;
}

static void print_help_msg(void)
{
	printf("\n");
	printf("  -s 1                  : test init preview\n");
	printf("  -s 0                  : test exit preview\n");
	printf("  -a [<addr w h size>]  : auto preview display test\n");
	printf("\n");
}

static int cmd_preview_display(int argc, char *argv[])
{
	int *test_time;
//	struct timeval tv;
	int ret, status, c, i, w, h, size = 100;
	const int test_frame = 60;
	struct buffer_node frame;
	frame.phy_address = (void *)0x42380100;

	while((c = getopt(argc, argv, "s:a")) != -1) {
		switch(c) {
		case 's':
			status = atoi(optarg);
			if (status) {
				preview_output_init();
			} else {
				preview_output_exit();
			}
			break;
		case 'a':
			w = preview.param.from_rtos.src_w;
			h = preview.param.from_rtos.src_h;
			preview.param.from_rtos.src_w = 400;
			preview.param.from_rtos.src_h = 400;

			if (argc == 6) {
				frame.phy_address = (void *)strtoul(argv[2], NULL, 0);;
				preview.param.from_rtos.src_w = atoi(argv[3]);
				preview.param.from_rtos.src_h = atoi(argv[4]);
				size = atoi(argv[5]);
				printf("test addr 0x%lx, w: %d h: %d size:%d\n", (unsigned long)frame.phy_address,
						preview.param.from_rtos.src_w, preview.param.from_rtos.src_h, size);
			}

			test_time = malloc(sizeof(*test_time) * test_frame);
			for (i = 0; i < test_frame; i++) {
				preview_layer_config_update(&frame);
				preview_display();
//				gettimeofday(&tv, NULL);
//				test_time[i] = tv.tv_sec * 1000 + tv.tv_usec /1000;
				frame.phy_address = i % 2 ? frame.phy_address + size : frame.phy_address - size;
			};
			printf("interval:\n");
			for (i = 0; i < test_frame - 1; i++) {
				printf("%d ms\n",test_time[i+1] - test_time[i]);
			}
			preview.param.from_rtos.src_w = w;
			preview.param.from_rtos.src_h = h;
			free(test_time);
			break;
		case 't':
			ageing_test();
			break;
		default:
			printf("Invalid option: -%c\n", c);
			ret = -1;
		case 'h':
			print_help_msg();
			goto out;
		}
	}

out:
	return ret;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_preview_display, preview_display, amp preview display);
