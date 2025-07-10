#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

#include <hal_osal.h>
#include <hal_sem.h>
#include <hal_cache.h>
#include <hal_msgbox.h>
#include <openamp/sunxi_helper/openamp.h>
#include <openamp/sunxi_helper/msgbox_ipi.h>

#define RPMSG_NOTIFY_MODULE_EPT_NAME "sunxi,notify"
#define RPMSG_NOTIFY_MAX_LEN			32

#define RPMSG_NOTIFY_MODULE_THREAD_NAME "rpmsg_notify"

#if 1
#define debug(fmt, args...)		printf(fmt, ##args)
#else
#define debug(fmt, args...)
#endif

static struct rpmsg_endpoint *srm_ept = NULL;

static int rpmsg_ept_callback(struct rpmsg_endpoint *ept, void *data,
		size_t len, uint32_t src, void *priv)
{
	return 0;
}

static void rpmsg_unbind_callback(struct rpmsg_endpoint *ept)
{
	debug("%s is destroyed\n", ept->name);
	srm_ept = NULL;
}

int rpmsg_notify(char *name, void *data, int len)
{
	uint8_t buf[RPMSG_MAX_LEN];

	if (!srm_ept) {
		debug("rpmsg notify module not init or init failed previously!\n");
		return -ENXIO;
	}

	memcpy(buf, name, strlen(name));
	memset(buf + strlen(name), 0, RPMSG_NOTIFY_MAX_LEN - strlen(name));
	if (data)
		memcpy(buf + RPMSG_NOTIFY_MAX_LEN, data, len);
	else
		len = 0;

	openamp_rpmsg_send(srm_ept, buf, RPMSG_NOTIFY_MAX_LEN + len);

	return 0;
}

int rpmsg_notify_init(void)
{
	int ret;
	struct rpmsg_endpoint *ept;
	const char *ept_name;

	ret = openamp_init();
	if (ret != 0) {
		printf("init openamp framework in rpmsg notify module failed, ret: %d\r\n", ret);
		return -1;
	}

	ept_name = RPMSG_NOTIFY_MODULE_EPT_NAME;
	ept = openamp_ept_open(ept_name, 0, RPMSG_ADDR_ANY, RPMSG_ADDR_ANY,
					NULL, rpmsg_ept_callback, rpmsg_unbind_callback);
	if (ept == NULL) {
		printf("create rpmsg endpoint('%s') failed\r\n", ept_name);
		return -2;
	}

	srm_ept = ept;
	return 0;
}

static void cmd_rpmsg_demo_thread(void *arg)
{
	rpmsg_notify_init();

	hal_thread_stop(NULL);
}

int rpmsg_notify_init_async(void)
{
	void *thread;
	const char *thread_name = RPMSG_NOTIFY_MODULE_THREAD_NAME;

	thread = hal_thread_create(cmd_rpmsg_demo_thread, NULL, thread_name, 2048, 8);
	if(thread != NULL)
		hal_thread_start(thread);
	else
		printf("create thread('%s') failed\r\n", thread_name);

	return 0;
}
