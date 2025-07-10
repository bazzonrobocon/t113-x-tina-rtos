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
#include <stdio.h>
#include <vfs.h>
#include <fcntl.h>
#include <errno.h>
#include <hal_time.h>
#include "lwip/sockets.h"
#include "adb.h"

#define FD_CLOEXEC 1
#define F_SETFD    2

static __inline__ void close_on_exec(int fd)
{
	fcntl(fd, F_SETFD, FD_CLOEXEC);
}

static __inline__ int adb_socket_accept(int serverfd, struct sockaddr* addr, socklen_t *addrlen)
{
	int fd;

	fd = accept(serverfd, addr, addrlen);
	if (fd >= 0)
		close_on_exec(fd);

	return fd;
}

static __inline__ void disable_tcp_nagle(int fd)
{
	int on = 1;
	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void*)&on, sizeof(on));
}

static int readx(int fd, void *ptr, size_t len)
{
	char *p = ptr;
	int r = 0, count = 0;

	while (len > 0) {
		/* r = lwip_recv(fd, ptr, len, MSG_DONTWAIT); */
		r = lwip_read(fd, ptr, len);
		if (r > 0) {
			len -= r;
			p += r;
		} else {
			if (r < 0) {
				count++;
				adbd_warn("readx fd:%d error\n", fd);
				if (errno == EINTR && count < 500)
					continue;
			} else {
				adbd_err("readx: fd=%d disconnected\n", fd);
			}
			return -1;
		}
	}

	return 0;
}

static int remote_read(apacket *p, atransport *t)
{
	int ret = 0;

	ret = readx(t->sfd, &p->msg, sizeof(amessage));
	if (ret) {
		adbd_err("local: expect size=%d\n", sizeof(amessage));
		adbd_err("remote local: read terminated(message)\n");
		return -1;
	}

	if (check_header(p)) {
		adbd_err("local: check header failed");
		return -1;
	}

	adbd_debug("local: command:0x%x, data_length:%u\n", p->msg.command, p->msg.data_length);
	readx(t->sfd, p->data, p->msg.data_length);

	if (check_data(p)) {
		adbd_err("local:check data failed");
		return -1;
	}
	return 0;
}

static int writex(int sfd, void *ptr, size_t len)
{
	char *p = (char *)ptr;
	int r = 0, count = 0;

	while (len > 0) {
		/* r = lwip_send(sfd, ptr, len, MSG_DONTWAIT); */
		r = lwip_write(sfd, ptr, len);
		if (r > 0) {
			len -= r;
			p += r;
		} else {
			if (r < 0) {
				count++;
				adbd_warn("writex fd=%d wait\n", sfd);
				if (errno == EINTR && count < 500)
					continue;
			} else {
				adbd_err("wirtex fd=%d disconnect\n", sfd);
			}
			return -1;
		}
	}

	return 0;
}

static int remote_write(apacket *p, atransport *t)
{
	int length = p->msg.data_length;

	if (writex(t->sfd, &p->msg, sizeof(amessage) + length)) {
		adbd_debug("remote local: write terminated(message)");
		return -1;
	}

	return 0;
}

static void *server_socket_thread(void * arg)
{
	int serverfd = -1, fd = -1;
	int port = (int)arg;
	struct sockaddr addr;
	socklen_t alen;

	adbd_info("transport: server_socket_thread() starting\n");
	for (;;) {
		adbd_debug("serverfd:%d\n", serverfd);
		if (serverfd == -1) {
			serverfd = socket_inaddr_any_server(port, SOCK_STREAM);
			if (serverfd < 0) {
				adbd_err("%s: cannot bind socket yet\n", __func__);
				hal_msleep(1000);
				continue;
			}
			close_on_exec(serverfd);
		}

		alen = sizeof(addr);
		printf("local server: trying to get new connection from %d\n", port);
		fd = adb_socket_accept(serverfd, &addr, &alen);
		adbd_debug("adb_socket_accept fd:%d\n", fd);
		if (fd > 0) {
			adbd_info("%s: new connection on fd %d\n", __func__, fd);
			close_on_exec(fd);
			disable_tcp_nagle(fd);
			register_socket_transport(fd, "host", port, 1);
		}
	}
	adbd_info("transport: server_socket_thread() exiting\n");
}

int local_init(int port)
{
	adb_thread_t thr;

	if (adb_thread_create(&thr, server_socket_thread, "adbd-socket",
				(void *)port, ADB_THREAD_HIGH_PRIORITY)) {
		fatal("cannot create local socket server thread");
		return -1;
	}
	return 0;
}

static void remote_kick(atransport *t)
{
	/* FIXME: shutdown and clsoe */

	return;
}

static void remote_close(atransport *t)
{
	/* FIXME: close t->fd */

	return;
}

int init_socket_transport(atransport *t, int s, int port, int local)
{
	adbd_debug("transport: local");
	t->kick = remote_kick;
	t->close = remote_close;
	t->read_from_remote = remote_read;
	t->write_to_remote = remote_write;
	t->sfd = s;
	t->sync_token = 1;
	t->connection_state = CS_OFFLINE;
	t->type = kTransportLocal;
	t->adb_port = 0;

	return 0;
}

int deinit_socket_transport(atransport *t, int s)
{
	adbd_debug("transport: local");
	t->kick = NULL;
	t->close = NULL;
	t->read_from_remote = NULL;
	t->write_to_remote = NULL;
	t->sfd = s;
	t->sync_token = 0;
	t->connection_state = 0;
	t->type = 0;
	t->adb_port = 0;

	return 0;
}
