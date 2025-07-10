#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <console.h>
#include <hal_time.h>
#include <hal_mem.h>
#include <hal_thread.h>
#include <hal_osal.h>
#include <hal_sem.h>
#include <hal_cache.h>
#include <hal_msgbox.h>
#include <openamp/sunxi_helper/openamp.h>
#include <openamp/sunxi_helper/msgbox_ipi.h>
#ifdef CONFIG_DRIVERS_HTIMER
#include <sunxi_hal_htimer.h>
#endif

#define RPMSG_SERVICE_NAME "sunxi,rpmsg_openamp_test"

#define OPENAMP_JITTER_PACKET_MAGIC	(0x12344321)
#define OPENAMP_JITTER_TIMER_INIT	(0)
#define OPENAMP_JITTER_TEST_START	(1)
#define OPENAMP_JITTER_TIMER_INIT_ACK	(2)
#define OPENAMP_JITTER_TIMER_START_ACK	(3)

struct openamp_jitter_packet {
	int magic;
	int type;
	unsigned int timer_reglow;
	unsigned int rt2linux_cnt;
	unsigned int linux2rt_cnt;
	int timer_freq;
};

struct rpmsg_openamp_jitter {
	const char *name;
	uint32_t src;
	uint32_t dst;
	struct rpmsg_endpoint *ept;
	int unbound;

};

static int htimer_freq = 0;
static int test_htimer0 = HAL_HRTIMER0;
static int test_step = 0;
static int htimer0_cnt0 = 0;
static int htimer0_cnt1 = 0;
static int htimer0_cnt2 = 0;
static int openamp_jitter_test = 0;
struct rpmsg_openamp_jitter *openamp_jitter = NULL;

static int rpmsg_ept_callback(struct rpmsg_endpoint *ept, void *data,
		size_t len, uint32_t src, void *priv)
{
	struct openamp_jitter_packet *pack = data;
	if (pack->magic != OPENAMP_JITTER_PACKET_MAGIC) {
		return 0;
	}

	switch(pack->type) {
	case OPENAMP_JITTER_TIMER_INIT_ACK:
		test_step = 1;
		break;
	case OPENAMP_JITTER_TIMER_START_ACK:
		htimer0_cnt1 = pack->rt2linux_cnt;
		htimer0_cnt2 = hal_htimer_read_cntlow(test_htimer0);
		test_step = 2;
		break;
	}

	return 0;
}

static void rpmsg_unbind_callback(struct rpmsg_endpoint *ept)
{
	struct rpmsg_openamp_jitter *priv = ept->priv;

	printf("%s is destroyed\n", ept->name);
	priv->unbound = 1;

}

static void hal_htimer0_irq_callback(void *param)
{

}

static void openamp_jitter_test_thread(void *arg)
{
	struct openamp_jitter_packet pack;
	float rt2linux;
	float linux2rt;
	int test_cnt = 0;
	while(1) {

		switch(test_step) {
		case 0:
			hal_htimer_base_init();
			test_htimer0 = HAL_HRTIMER0;
			memset(&pack, 0, sizeof(struct openamp_jitter_packet));
			pack.type = OPENAMP_JITTER_TIMER_INIT;
			pack.magic = OPENAMP_JITTER_PACKET_MAGIC;
			pack.timer_reglow = hal_htimer_read_reglow(test_htimer0);
			openamp_rpmsg_send(openamp_jitter->ept, (void *)&pack, sizeof(struct openamp_jitter_packet));
			test_step = 0xff;
			break;
		case 1:
			memset(&pack, 0, sizeof(struct openamp_jitter_packet));
			hal_htimer_init(test_htimer0);
			hal_htimer_set_oneshot(test_htimer0, (5*1000*1000), hal_htimer0_irq_callback, NULL);
			pack.type = OPENAMP_JITTER_TEST_START;
			pack.magic = OPENAMP_JITTER_PACKET_MAGIC;
			htimer0_cnt0 = hal_htimer_read_cntlow(test_htimer0);
			openamp_rpmsg_send(openamp_jitter->ept, (void *)&pack, sizeof(struct openamp_jitter_packet));
			test_step = 0xff;
			break;
		case 2:
			htimer_freq = hal_htimer_freq();
			rt2linux = 1.0f * (htimer0_cnt0 - htimer0_cnt1)*1000/htimer_freq;
			linux2rt = 1.0f * (htimer0_cnt1 - htimer0_cnt2)*1000/htimer_freq;
			printf("test[%d] stop: rt->linux delay %0.8f ms, linux->rt delay %0.8f ms\n", \
					test_cnt, rt2linux, linux2rt);

			/* clsoe htimer */
			hal_htimer_deinit(test_htimer0);
			hal_htimer_base_deinit();

			if (++test_cnt >= openamp_jitter_test) {
				test_step = 0xff;
				goto test_out;
			} else {
				test_step = 0;
			}
			break;
		default:
			hal_msleep(1);
			break;
		}
	}

test_out:
	test_step = 0;
	hal_thread_stop(NULL);
}

int rpmsg_openamp_jitter_init(void)
{

	openamp_jitter = hal_malloc(sizeof(*openamp_jitter));
	if (!openamp_jitter) {
		printf("Failed to malloc memory\r\n");
		goto out;
	}

	if (openamp_init() != 0) {
		printf("Failed to init openamp framework\r\n");
		goto out;
	}

	openamp_jitter->name = RPMSG_SERVICE_NAME;
	openamp_jitter->unbound = 0;
	openamp_jitter->src = RPMSG_ADDR_ANY;
	openamp_jitter->dst = RPMSG_ADDR_ANY;

	openamp_jitter->ept = openamp_ept_open(openamp_jitter->name, 0, \
					openamp_jitter->src, openamp_jitter->dst, \
					openamp_jitter, rpmsg_ept_callback, rpmsg_unbind_callback);
	if (openamp_jitter->ept == NULL) {
		printf("Failed to Create Endpoint\r\n");
		goto out;
	}

out:
	return 0;
}

static int cmd_openamp_jitter_test(int argc, char *argv[])
{
	void *thread;

	test_step = 0;

	if (argc == 2) {
		if (strcmp("-h", argv[1]) == 0) {
			printf("usage: openamp_jitter_test <test_cnt>\n");
			return 0;
		}
	}else {
		printf("usage: openamp_jitter_test <test_cnt>\n");
		return 0;
	}

	openamp_jitter_test = atoi(argv[1]);

	thread = hal_thread_create(openamp_jitter_test_thread, \
								NULL, "openamp jitter test thread", 1 * 1024, 15);
	if (thread != NULL)
        hal_thread_start(thread);

	return 0;
}

FINSH_FUNCTION_EXPORT_CMD(cmd_openamp_jitter_test, openamp_jitter_test, openamp jitter test for os);
