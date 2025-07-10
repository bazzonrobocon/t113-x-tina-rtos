#ifndef __ADB_H
#define __ADB_H

#include "adb_queue.h"
#include "adb_ev.h"
#include "adb_rb.h"
#include "misc.h"
#include <assert.h>
#include <aw_common.h>
#include <aw_list.h>
#include <cli_console.h>

#define ADBD_VERSION 	"AW-V1.1.6"

#if 0

#define adbd_debug(fmt, args...)	\
    printf("[ADBD-DEBUG][%s] line:%d " fmt "\n", __func__, __LINE__, ##args)
#else
#define adbd_debug(fmt, args...)
#endif

#ifndef unlikely
#define unlikely(x)             __builtin_expect ((x), 0)
#endif

extern int g_adbd_debug_mask;
#define adbd_info(fmt, args...) \
	do { \
		if (unlikely(g_adbd_debug_mask)) \
			printf("[ADBD-INFO][%s] line:%d " fmt "\n", \
					__func__, __LINE__, ##args); \
	} while (0)
#define adbd_info_raw(fmt, args...) \
	do { \
		if (unlikely(g_adbd_debug_mask)) \
			printf( fmt , ##args); \
	} while (0)


#define adbd_err(fmt, args...)	\
    printf("[ADBD-ERROR][%s] line:%d " fmt "\n", __func__, __LINE__, ##args)
#define adbd_warn(fmt, args...) \
    printf("[ADBD-WARN][%s] line:%d " fmt "\n", __func__, __LINE__, ##args)

#define fatal(msg) \
do {\
	printf("[ADBD-FATAL][%s] line:%d %s \n", __func__, __LINE__,msg);\
	assert(0);\
} while (0)

#define ADBD_RECORD_ALIVE_THREAD

#define USB_ADB_EP0	(0)
#define USB_ADB_IN	(1)
#define USB_ADB_OUT	(2)

#define MAX_PAYLOAD (4096)

#define A_SYNC 0x434e5953
#define A_CNXN 0x4e584e43
#define A_OPEN 0x4e45504f
#define A_OKAY 0x59414b4f
#define A_CLSE 0x45534c43
#define A_WRTE 0x45545257
#define A_AUTH 0x48545541


#define A_VERSION 0x01000000        // ADB protocol version

#define ADB_VERSION_MAJOR 1         // Used for help/version information
#define ADB_VERSION_MINOR 0         // Used for help/version information

#define ADB_SERVER_VERSION    31    // Increment this when we want to force users to start a new adb server

#define DUMPMAX 32

typedef struct usb_handle usb_handle;
typedef struct aservice aservice;
typedef struct atransport atransport;
typedef struct amessage amessage;
typedef struct apacket apacket;
typedef struct shell_xfer shell_xfer;
typedef struct sync_xfer sync_xfer;
typedef struct socket_xfer socket_xfer;

struct amessage {
	unsigned int command;
	unsigned int arg0;
	unsigned int arg1;
	unsigned int data_length;
	unsigned int data_check;
	unsigned int magic;
};

struct apacket {
	amessage msg;
	__aligned(CACHELINE_LEN) uint8_t data[MAX_PAYLOAD];
};

typedef enum transport_type {
        kTransportUsb,
        kTransportLocal,
        kTransportAny,
        kTransportHost,
} transport_type;

struct atransport {
	int (*read_from_remote)(apacket *p, atransport *t);
	int (*write_to_remote)(apacket *p, atransport *t);
	void (*close)(atransport *t);
	void (*kick)(atransport *t);

	transport_type type;

	usb_handle *usb;
	int sfd; /* socket fd */
	int adb_port;

	unsigned sync_token;
	int connection_state;
	int online;

	adb_queue to_remote;
};

#define CS_ANY       -1
#define CS_OFFLINE    0
#define CS_BOOTLOADER 1
#define CS_DEVICE     2
#define CS_HOST       3
#define CS_RECOVERY   4
#define CS_NOPERM     5 /* Insufficient permissions to communicate with the device */
#define CS_SIDELOAD   6


enum {
	ADB_SERVICE_TYPE_UNKNOWN = 0,
	ADB_SERVICE_TYPE_SHELL,
	ADB_SERVICE_TYPE_SYNC,
	ADB_SERVICE_TYPE_SOCKET,
};

struct shell_xfer {
	adb_schd_t schd;
	adb_ev *ev_handle;
	adb_rb *rb_read_from_pc;
	adb_rb *rb_write_by_shell;
	adb_rb *rb_write_by_adb;
	cli_console *console;
	int recv_close;
	int interactive;
	int abnormal_alive;
};

struct sync_xfer {
	adb_queue_ev *queue_recv;
	adb_queue queue_send;
	adb_schd_t schd;
	apacket *p;
	unsigned int offset;
};

struct socket_xfer {
	adb_schd_t schd;
	int s;
	int recv_close;
};

struct aser_cookie {
	adb_thread_t tid;
	char command_name[32];
};

struct aservice {
	atransport *transport;
	void (*func)(void *handle, void *cookie);
	int service_type;
	union {
		shell_xfer shell_handle;
		sync_xfer sync_handle;
		socket_xfer socket_handle;
	} xfer_handle;
	struct aser_cookie cookie;

	unsigned int localid;
	unsigned int remoteid;

	int (*enqueue)(struct aservice *ser, apacket *pkt);
	int (*ready)(struct aservice *ser);

	int has_send_close;
	adb_thread_t service_tid;
	struct list_head list;
};


void print_apacket(const char *label, apacket *p);
void handle_packet(apacket *p, atransport *t);
apacket *get_apacket(void);
void put_apacket(apacket *p);
void send_apacket(apacket *p, atransport *t);
void send_write(const char *buf, int size, unsigned int local, unsigned int remote, atransport *t);
void send_close(unsigned int local, unsigned int remote, atransport *t);
void send_ready(unsigned int local, unsigned int remote, atransport *t);

int check_header(apacket *p);
int check_data(apacket *p);


int init_usb_transport(atransport *t, usb_handle *h, int state);


aservice * adb_service_create(const char *name, unsigned int remoteid, atransport *t);
void adb_service_destroy(unsigned int localid);
aservice *find_adb_service(unsigned int localid);

int usb_write(usb_handle *h, const void *data, int len);
int usb_read(usb_handle *h, void *data, int len);

int usb_init(void);
int usb_exit(void);
int deinit_usb_transport(atransport *t, usb_handle *h, int state);
void register_usb_transport(usb_handle *usb, const char *serial, const char *devpath, unsigned writeable);
void unregister_usb_transport(usb_handle *usb, const char *serial, const char *devpath, unsigned writeable);
int usb_gadget_adb_deinit(void);
void adb_service_destroy_all(void);

#ifdef CONFIG_COMPONENTS_USB_GADGET_ADB_LOCAL_TRANSPORT
int local_init(int port);
int init_socket_transport(atransport *t, int s, int port, int local);
int deinit_socket_transport(atransport *t, int s);
void register_socket_transport(int s, const char *serial, int port, int local);
void unregister_socket_transport(int s, const char *serial, int port, int local);
int socket_inaddr_any_server(int port, int type);
#endif

#ifdef ADB_SUPPORT_SOCKET_API
int socket_loopback_client(int port);
void socket_recv_service(void *xfer_handle, void *cookie);
void socket_recv_rawdata_service(void *xfer_handle, void *cookie);
int socket_enqueue(aservice *ser, apacket *p);
int socket_ready(aservice *ser);
#endif

#ifdef MEM_DEBUG
void *malloc_wrapper(size_t size);
void *calloc_wrapper(size_t nmemb, size_t size);
void *realloc_wrapper(void *ptr, size_t size);
char *strdup_wrapper(const char *s);
void free_wrapper(void *ptr);
#define adb_malloc(size)				malloc_wrapper(size)
#define adb_calloc(num, size)				calloc_wrapper(num, size)
#define adb_realloc(ptr, size)				realloc_wrapper(ptr, size)
#define adb_strdup(s)					strdup_wrapper(s)
//#define adb_scandir(dirp,namelist,filter,compar)	scandir_wrapper(dirp,namelist,filter,compar)
#define adb_free(ptr)					free_wrapper(ptr)
void memleak_print(void);
#else
#define adb_malloc(size)				malloc(size)
#define adb_calloc(num, size)				calloc(num, size)
#define adb_realloc(ptr, size)				realloc(ptr, size)
#define adb_strdup(s)					strdup(s)
//#define adb_scandir(dirp,namelist,filter,compar)	scandir(dirp,namelist,filter,compar)
#define adb_free(ptr)					free(ptr)
#endif

extern int adb_thread_high_priority;
extern int adb_thread_normal_priority;

#define ADB_THREAD_HIGH_PRIORITY	adb_thread_high_priority
#define ADB_THREAD_NORMAL_PRIORITY	adb_thread_normal_priority

#ifdef ADBD_RECORD_ALIVE_THREAD
void record_alive_thread_print(void);
void record_alive_thread_add(const char *name);
#endif

void send_write_combine(const char *buf1, int size1,
                        const char *buf2, int size2,
                        unsigned int local, unsigned int remote, atransport *t);

#endif /* __ADB_H */
