/*
* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
*
* Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
* the the people's Republic of China and other countries.
* All Allwinner Technology Co.,Ltd. trademarks are used with permission.
*
* DISCLAIMER
* THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
* IF YOU NEED TO INTEGRATE THIRD PARTY’S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
* IN ALLWINNERS’SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
* ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
* ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
* COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
* YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY’S TECHNOLOGY.
*
*
* THIS SOFTWARE IS PROVIDED BY ALLWINNER"AS IS" AND TO THE MAXIMUM EXTENT
* PERMITTED BY LAW, ALLWINNER EXPRESSLY DISCLAIMS ALL WARRANTIES OF ANY KIND,
* WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING WITHOUT LIMITATION REGARDING
* THE TITLE, NON-INFRINGEMENT, ACCURACY, CONDITION, COMPLETENESS, PERFORMANCE
* OR MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
* IN NO EVENT SHALL ALLWINNER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
* NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS, OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
* OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#ifdef ADB_SUPPORT_SOCKET_API
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "adb.h"

#ifndef ADB_SOCKET_UNUSED
#include <lwip/inet.h>
#include "lwip/sockets.h"
#endif
#include <semaphore.h>

#include <aw_list.h>
#include <console.h>

#ifdef ADB_SOCKET_UNUSED
int socket_enqueue(aservice *ser, apacket *p)
{
	printf("[%s] line:%d not support...\n", __func__, __LINE__);
	return 0;
}
#else
int socket_enqueue(aservice *ser, apacket *p)
{
	int ret;
	socket_xfer *xfer = &ser->xfer_handle.socket_handle;
	adbd_debug("");
	/*adb_enqueue(xfer->queue_recv, (void **)&p, 100);*/
	adbd_debug("data:%s\n", p->data);
	ret = send(xfer->s, p->data, p->msg.data_length, 0);
	put_apacket(p);
	if (ret >= 0)
		return 0;
	return -1;
}
#endif

int socket_ready(aservice *ser)
{
	socket_xfer *xfer = &ser->xfer_handle.socket_handle;

	adbd_debug("");
	adb_schd_wakeup(xfer->schd, ADB_SCHD_READ);
	return 0;
}

static void socket_wait_ready(aservice *ser)
{
	socket_xfer *xfer = &ser->xfer_handle.socket_handle;

	adbd_debug("");
	adb_schd_wait(xfer->schd);
	adbd_debug("");
}

/* for debug */
static uint64_t record_recv_len = 0;
uint64_t adb_get_record_recv_len(void)
{
	return record_recv_len;
}

#define LISTEN_LENGTH	(20)
#ifdef ADB_SOCKET_UNUSED
int socket_loopback_client(int port)
{

}
#else
int socket_loopback_client(int port)
{
	struct sockaddr_in addr;
	int s, enable;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	/*addr.sin_addr.s_addr = htonl(INADDR_ANY);*/

	adbd_info("port:%d", port);
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0) {
		adbd_err("create socket failed, errno=%d(%s)", errno, strerror(errno));
		return -1;
	}
	enable = 1;
	setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (void*)&enable, sizeof(enable));
	if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		adbd_err("connect failed, errno=%d(%s)", errno, strerror(errno));
		close(s);
		return -1;
	}
	record_recv_len = 0;
	return s;
}
#endif

#ifdef ADB_SOCKET_UNUSED
void socket_recv_service(void *xfer_handle, void *cookie)
{
	return ;
}
#else
void socket_recv_service(void *xfer_handle, void *cookie)
{
	fd_set fds;
	int ret;
	socket_xfer *xfer = (socket_xfer *)xfer_handle;
	aservice *ser = container_of((void *)xfer, struct aservice, xfer_handle);

#if 1
	char *buffer;
	struct timeval timeout;
	int size;

	buffer = adb_malloc(MAX_PAYLOAD);
	if (!buffer)
		fatal("no memory");

	while (1) {
		FD_ZERO(&fds);
		FD_SET(xfer->s, &fds);

		timeout.tv_sec  = 0;
		timeout.tv_usec = 200000;
		adbd_debug("select...");
		ret = lwip_select(xfer->s + 1, &fds, NULL, NULL, &timeout);
		adbd_debug("select return %d", ret);
		if (ret < 0) {
			adbd_err("select failed, errno=%d\n", errno);
			break;
		} else if (ret == 0) {
			adbd_debug("select timeout...");
			if (xfer->recv_close != 0)
				break;
			continue;
		}
		if (!FD_ISSET(xfer->s, &fds)) {
			adbd_err("unknow fd.");
			break;
		}
		adbd_debug("");
		memset(buffer, 0, MAX_PAYLOAD);
		size = lwip_recv(xfer->s, buffer, MAX_PAYLOAD, MSG_DONTWAIT);
		adbd_debug("size=%d\n", size);
		if (xfer->recv_close) {
			adbd_info("recv_close=%d, break loop", xfer->recv_close);
			break;
		}
		if (size <= 0) {
			adbd_info("recv return %d, recv_close=%d", size, xfer->recv_close);
			break;
		} else if (size > 0) {
			record_recv_len += size;
			socket_wait_ready(ser);
			send_write((const char *)buffer, size, ser->localid,
						ser->remoteid, ser->transport);
		}
		adbd_debug("");
	}
	adb_free(buffer);
	adbd_info("socket recv service exit...");
	pthread_exit(NULL);
#else
	char buf[128];
	int size;

	while (1) {
		memset(buf, 0, sizeof(buf));
		size = lwip_read(xfer->s, buf, sizeof(buf));
		if (size < 0) {
			adbd_err("lwip_read failed, return %d\n", size);
			break;
		}
		adbd_info("size=%d\n", size);
		if (size > 0 && !xfer->recv_close) {
			adbd_info("data=%s\n", buf);
			send_write((const char *)buf, size, ser->localid,
						ser->remoteid, ser->transport);
			socket_wait_ready(ser);
		}
	}
	pthread_exit(NULL);
#endif


}
#endif

#define UPLOAD_START	"-->AW_RTOS_SOCKET_UPLOAD_START"
#define UPLOAD_END	"-->AW_RTOS_SOCKET_UPLOAD_END"

typedef struct adb_forward {
	pthread_t tid;
	int port;
	int local_sock, data_sock;
	int state;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	//dlist_t list;
	struct list_head list;
	void *send_buf;
	unsigned int send_size;
	sem_t sem;
	int is_raw_data;
} adb_forward_t;

enum {
	ADB_FORWARD_THREAD_UNKNOWN = 0,
	ADB_FORWARD_THREAD_PREPARE,
	ADB_FORWARD_THREAD_INIT,
	ADB_FORWARD_THREAD_RUNNING,
	ADB_FORWARD_THREAD_FINISH,
	ADB_FORWARD_THREAD_EXIT,
};

//static dlist_t gAdbForwardThreadList = AOS_DLIST_INIT(gAdbForwardThreadList);
static LIST_HEAD(gAdbForwardThreadList);

static adb_forward_t *adb_forward_find(int port)
{
	adb_forward_t *a = NULL;
	//list_for_each_entry(&gAdbForwardThreadList, a, struct adb_forward, list) {
	list_for_each_entry(a, &gAdbForwardThreadList, list) {
		if (a->port == port)
			return a;
	}
	return NULL;
}

int adb_forward_create_with_rawdata(int port);
void socket_recv_rawdata_service(void *xfer_handle, void *cookie)
{
	socket_xfer *xfer = (socket_xfer *)xfer_handle;
	aservice *ser = container_of((void *)xfer, struct aservice, xfer_handle);

	int port = xfer->s;
	adb_forward_t *a;

	a = adb_forward_find(port);
	if (!a) {
		if (adb_forward_create_with_rawdata(port) != 0) {
			adbd_err("adb_forward_create_with_rawdata failed\n");
			return;
		}
		a = adb_forward_find(port);
	}
	if (!a->is_raw_data) {
		adbd_err("isn't raw data port:%d\n", port);
		return;
	}

	a->state = ADB_FORWARD_THREAD_PREPARE;
	pthread_mutex_lock(&a->mutex);
	while (!xfer->recv_close) {
		switch(a->state) {
		case ADB_FORWARD_THREAD_INIT:
			socket_wait_ready(ser);
			send_write((const char *)UPLOAD_START, strlen(UPLOAD_START), ser->localid,
				ser->remoteid, ser->transport);
			a->state = ADB_FORWARD_THREAD_RUNNING;
		case ADB_FORWARD_THREAD_RUNNING: {
			int offset = 0, size = a->send_size;
			if (!a->send_buf || !a->send_size)
				break;
			while (offset < a->send_size) {
				socket_wait_ready(ser);
				size = a->send_size - offset;
				if (size > MAX_PAYLOAD)
					size = MAX_PAYLOAD;
				send_write((const char *)a->send_buf + offset, size, ser->localid,
					ser->remoteid, ser->transport);
				offset += size;
			}
			a->send_buf = NULL;
			a->send_size = 0;
			sem_post(&a->sem);
			}
			break;
		case ADB_FORWARD_THREAD_FINISH:
			socket_wait_ready(ser);
			send_write((const char *)UPLOAD_END, strlen(UPLOAD_END), ser->localid,
				ser->remoteid, ser->transport);
			a->state = ADB_FORWARD_THREAD_PREPARE;
			break;
		case ADB_FORWARD_THREAD_EXIT:
			socket_wait_ready(ser);
			send_write((const char *)UPLOAD_END, strlen(UPLOAD_END), ser->localid,
				ser->remoteid, ser->transport);
			goto exit_loop;
		}

		struct timespec tp = {0};
		int clock_gettime (clockid_t clock_id, struct timespec *tp);
		clock_gettime(CLOCK_REALTIME, &tp);
		tp.tv_nsec += 200*1000;
		pthread_cond_timedwait(&a->cond, &a->mutex, &tp);
	}
exit_loop:
	a->state = ADB_FORWARD_THREAD_UNKNOWN;
	pthread_mutex_unlock(&a->mutex);
	adbd_debug("socket recv rawdata service exit...");
	pthread_exit(NULL);
}

int is_raw_data_port(int port)
{
	adb_forward_t *a = NULL;
	a = adb_forward_find(port);
	if (!a)
		return 0;
	if (a->state != ADB_FORWARD_THREAD_UNKNOWN)
		return -1;
	if (a->is_raw_data)
		return 1;
	return 0;
}

#ifdef ADB_SOCKET_UNUSED
static void *adb_forward_thread(void *arg)
{

}
#else
static void *adb_forward_thread(void *arg)
{
	int ret;
	adb_forward_t *a = arg;
	struct sockaddr_in cli_addr;
	socklen_t length = sizeof(cli_addr);
	struct timeval timeout;

	while (1) {
		ret = listen(a->local_sock, LISTEN_LENGTH);
		if (ret != 0) {
			printf("listen failed, return %d\n", ret);
			goto err;
		}
	retry_accept:
		timeout.tv_sec = 0;
		timeout.tv_usec = 500000;
		setsockopt(a->local_sock, SOL_SOCKET,SO_RCVTIMEO, &timeout, sizeof(timeout));
		a->state = ADB_FORWARD_THREAD_PREPARE;
		a->data_sock = accept(a->local_sock,
				(struct sockaddr *)&cli_addr, &length);
		if (a->data_sock <= 0) {
			adbd_debug("accept failed, return %d\n", a->data_sock);
			if (a->state != ADB_FORWARD_THREAD_PREPARE)
				break;
			goto retry_accept;
		}
		a->state = ADB_FORWARD_THREAD_INIT;
		pthread_mutex_lock(&a->mutex);
		while (a->state != ADB_FORWARD_THREAD_FINISH &&
			a->state != ADB_FORWARD_THREAD_EXIT) {
			if (a->send_buf && a->send_size != 0) {
				ssize_t size = -1;
				switch (a->state) {
				case ADB_FORWARD_THREAD_INIT:
					size = send(a->data_sock, UPLOAD_START, strlen(UPLOAD_START), 0);
					a->state = ADB_FORWARD_THREAD_RUNNING;
					if (size != strlen(UPLOAD_START))
						printf("send upload_start failed, return %zd\n", size);
				case ADB_FORWARD_THREAD_RUNNING:
					size = send(a->data_sock, a->send_buf, a->send_size, 0);
					break;
				}
				if (size != a->send_size)
					printf("send %u bytes, but actual return %zd\n", a->send_size, size);
				a->send_buf = NULL;
				a->send_size = 0;
				sem_post(&a->sem);
			}
			pthread_cond_wait(&a->cond, &a->mutex);
		}
		/* send upload end flag */
		send(a->data_sock, UPLOAD_END, strlen(UPLOAD_END), 0);
		if (a->state == ADB_FORWARD_THREAD_EXIT)
			goto exit_state;
		close(a->data_sock);
		/* clear file_adb info */
		a->data_sock = -1;
		a->state = ADB_FORWARD_THREAD_UNKNOWN;
		pthread_mutex_unlock(&a->mutex);
	}
err:
	pthread_mutex_lock(&a->mutex);
exit_state:
	a->state = ADB_FORWARD_THREAD_EXIT;
	if (a->data_sock > 0)
		close(a->data_sock);
	if (a->local_sock > 0)
		close(a->local_sock);
	a->data_sock = -1;
	a->local_sock = -1;
	pthread_mutex_unlock(&a->mutex);

	pthread_exit(NULL);
	return NULL;
}
#endif

static void adb_forward_set_state(adb_forward_t *a, int state)
{
	pthread_mutex_lock(&a->mutex);
	a->state = state;
	pthread_cond_signal(&a->cond);
	pthread_mutex_unlock(&a->mutex);
	return;
}

int adb_forward_create_with_rawdata(int port)
{
	adb_forward_t *a = NULL;
	char name[32];

	if (port < 0) {
		printf("unknown port=%d\n", port);
		return -1;
	}

	a = adb_forward_find(port);
	if (a != NULL) {
		if (!a->is_raw_data) {
			printf("adb forward already exist, but not raw data\n");
			return -1;
		}
		return 0;
	}

	a = calloc(1, sizeof(adb_forward_t));
	if (!a) {
		printf("no memory\n");
		return -1;
	}

	a->port = port;
	a->state = ADB_FORWARD_THREAD_UNKNOWN;

	snprintf(name, sizeof(name), "adb_forward_r_%d", port);
	pthread_mutex_init(&a->mutex, NULL);
	pthread_cond_init(&a->cond, NULL);

	sem_init(&a->sem, 0, 0);
	a->is_raw_data = 1;

	list_add(&a->list, &gAdbForwardThreadList);
	return 0;
}

#ifdef ADB_SOCKET_UNUSED
int adb_forward_create(int port)
{
	return -1;
}
#else
int adb_forward_create(int port)
{
	adb_forward_t *a = NULL;
	struct sockaddr_in addr;
	char name[32];
	int s;

	if (port < 0) {
		printf("unknown port=%d\n", port);
		return -1;
	}

	a = adb_forward_find(port);
	if (a != NULL) {
		printf("adb forward thread with port:%d, already exist\n", port);
		return 0;
	}

	a = calloc(1, sizeof(adb_forward_t));
	if (!a) {
		printf("no memory\n");
		return -1;
	}
	a->port = port;
	a->state = ADB_FORWARD_THREAD_UNKNOWN;

	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(port);

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0) {
		printf("create socket failed\n");
		free(a);
		return -1;
	}

	int opt = 1;
	setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		printf("bind socket failed\n");
		close(s);
		free(a);
		return -1;
	}
	a->local_sock = s;
	snprintf(name, sizeof(name), "adb_forward_%d", port);
	pthread_mutex_init(&a->mutex, NULL);
	pthread_cond_init(&a->cond, NULL);

	sem_init(&a->sem, 0, 0);

	adb_thread_create(&a->tid, adb_forward_thread, name, (void *)a, ADB_THREAD_NORMAL_PRIORITY);
	list_add(&a->list, &gAdbForwardThreadList);
	return 0;
}
#endif

void adb_forward_destroy(int port)
{
	adb_forward_t *a = NULL;

	a = adb_forward_find(port);
	if (!a) {
		printf("adb forward thread with port:%d, isn't exist\n", port);
		return;
	}

	adb_forward_set_state(a, ADB_FORWARD_THREAD_EXIT);
	if (a->tid > 0)
		adb_thread_wait(a->tid, NULL);
	list_del(&a->list);
	pthread_mutex_destroy(&a->mutex);
	pthread_cond_destroy(&a->cond);
	sem_destroy(&a->sem);
	free(a);
}

int adb_forward_end(int port)
{
	adb_forward_t *a = NULL;

	a = adb_forward_find(port);
	if (!a) {
		printf("adb forward thread with port:%d, isn't exist\n", port);
		return -1;
	}

	adb_forward_set_state(a, ADB_FORWARD_THREAD_FINISH);
	return 0;
}

int adb_forward_send(int port, void *data, unsigned len)
{
	int err;
	adb_forward_t *a = NULL;

	a = adb_forward_find(port);
	if (!a) {
		printf("adb forward thread with port:%d, isn't exist\n", port);
		return -1;
	}

	switch (a->state) {
	case ADB_FORWARD_THREAD_UNKNOWN:
	case ADB_FORWARD_THREAD_FINISH:
		return -1;
	}

	pthread_mutex_lock(&a->mutex);
	a->send_buf = data;
	a->send_size = len;
	if (a->state != ADB_FORWARD_THREAD_RUNNING)
		a->state = ADB_FORWARD_THREAD_INIT;
	pthread_cond_signal(&a->cond);
	pthread_mutex_unlock(&a->mutex);

	err = sem_wait(&a->sem);

	return err;
}



static void usage(void)
{
	printf("Usage: aft [option]\n");
	printf("-p,          port\n");
	printf("-c,          create adb forward thread with port\n");
	printf("-s,          send msg\n");
	printf("-f,          transfer finish, send upload end flag\n");
	printf("-e,          exit adb forward thread with port\n");
	printf("\n");
}

static int cmd_adbforward(int argc, char *argv[])
{
	int c, port = -1;

	optind = 0;
	while ((c = getopt(argc, argv, "p:s:frech")) != -1) {
		switch (c) {
		case 'p':
			port = atoi(optarg);
			break;
		case 'c':
			if (adb_forward_create(port))
				printf("adb forward init thread failed\n");
			else
				printf("adb forward init with port:%d success\n", port);
			return 0;
		case 'f':
			adb_forward_end(port);
			break;
		case 'e':
			adb_forward_destroy(port);
			break;
		case 's': {
			int ret;
			ret = adb_forward_send(port, optarg, strlen(optarg));
			printf("adb_forward_send return %d\n", ret);
		}
			break;
		case 'r':
			if (adb_forward_create_with_rawdata(port))
				printf("adb forward init rawdata thread failed\n");
			else
				printf("adb forward init with port:%d success\n", port);
			break;
		case 'h':
		default:
			usage();
			return -1;
		}
	}
	return 0;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_adbforward, af, adb forward);

#endif /* ADB_SUPPORT_SOCKET_API */
