#include <stdlib.h>
#include <stdint.h>
#include <hal_queue.h>

#include "rpmsg_console.h"

#define RPMSG_BIND_NAME				"console"
#define RPMSG_CONSOLE_MAX			100

#define log					printf

static void rpmsg_create_console(void *arg)
{
	int ret;
	struct rpmsg_service *ser;
	struct rpmsg_ept_client *client = arg;

	ser = client->priv;
	if (!ser) {
		rpmsg_err("no rpmsg service found! client: %p", client);
		goto exit;
	}

	log("create rpmsg%u console.\r\n", client->id);
	ser->shell = rpmsg_console_create(client->ept, client->id);
	if (!ser->shell) {
		rpmsg_err("rpmsg_console_create failed!\r\n");
		ret = -1;
		goto exit_with_release_sem;
	}

	log("create rpmsg%u success.\r\n", client->id);

exit_with_release_sem:
	ret = hal_sem_post(&ser->ready_sem);
	if (ret)
		rpmsg_err("release sem failed, ret: %d\n", ret);

exit:
	hal_thread_stop(NULL);
}

static int rpmsg_bind_cb(struct rpmsg_ept_client *client)
{
	int ret = 0;
	void *thread;
	struct rpmsg_service *ser;

	log("rpmsg%u : binding, name: '%s', client: %p\n",
		client->id, client->name, client);

	ser = hal_malloc(sizeof(*ser));
	if (!ser) {
		rpmsg_err("failed to alloc rpmsg service\n");
		ret = -1;
		goto exit;
	}
	memset(ser, 0, sizeof(*ser));

	hal_sem_init(&ser->ready_sem, 0);
	client->priv = ser;

	thread = hal_thread_create(rpmsg_create_console, client,
					"rp_con_init", 8 * 1024, 5);
	if (thread != NULL) {
		hal_thread_start(thread);
		ret = 0;
	} else
		ret = -EFAULT;

	if (ret) {
		client->priv = NULL;
		free(ser);
	}

exit:
	return ret;
}

static int rpmsg_unbind_cb(struct rpmsg_ept_client *client)
{
	int ret;
	struct rpmsg_service *ser = client->priv;

	log("rpmsg%u: unbinding, name: '%s', client: %p\n",
		client->id, client->name, client);

	if (!ser) {
		rpmsg_err("no rpmsg service found! client: %p\n", client);
		return -ENODEV;
	}

	ret = hal_sem_timedwait(&ser->ready_sem, MS_TO_OSTICK(5000));
	if (ret)
		rpmsg_err("wait rpmsg service ready failed! ret: %d, client: %p\n", ret, client);

	if (!ser->shell) {
		rpmsg_err("rpmsg service without shell instance!\n");
		goto exit_with_free;
	}

	rpmsg_console_delete(ser->shell);

exit_with_free:
	hal_free(ser);

	return 0;
}

static int rpmsg_tmp_ept_cb(struct rpmsg_endpoint *ept, void *data,
		size_t len, uint32_t src, void *priv)
{
	return 0;
}

int rpmsg_multi_console_init(void)
{
	rpmsg_client_bind(RPMSG_BIND_NAME, rpmsg_tmp_ept_cb, rpmsg_bind_cb,
					rpmsg_unbind_cb, RPMSG_CONSOLE_MAX, NULL);

	return 0;
}

static void rpmsg_multi_console_init_thread(void *arg)
{
	rpmsg_multi_console_init();
	hal_thread_stop(NULL);
}

int rpmsg_multi_console_init_async(void)
{
	void *thread;

	thread = hal_thread_create(rpmsg_multi_console_init_thread, NULL,
					"init", 8 * 1024, 5);
	if (thread != NULL)
		hal_thread_start(thread);

	return 0;
}

void rpmsg_multi_console_close(void)
{
	rpmsg_client_unbind(RPMSG_BIND_NAME);
}

int rpmsg_multi_console_register(void)
{
	return rpmsg_multi_console_init_async();
}
void rpmsg_multi_console_unregister(void)
{
	rpmsg_multi_console_close();
}
