#ifndef __F_NCM__H__
#define __F_NCM__H__
#include <typedef.h>
#include <stdbool.h>
#include "ncm_sk_buff.h"
#include "sunxi_timer.h"
#include "semaphore.h"

#define NCM_STATUS_INTERVAL_MS		32
#define NCM_STATUS_BYTECOUNT		16	/* 8 byte header + data */
#define NCM_IAP_ENABLE

typedef enum {
	NCM_FUNCTION_NORMAL = 0,
	NCM_FUNCTION_EXIT_STAGE_0,
	NCM_FUNCTION_EXIT_STAGE_1,
	NCM_FUNCTION_EXIT
} ncm_state;
struct ndp_parser_opts {
	u32		nth_sign;
	u32		ndp_sign;
	unsigned	nth_size;
	unsigned	ndp_size;
	unsigned	dpe_size;
	unsigned	ndplen_align;
	/* sizes in u16 units */
	unsigned	dgram_item_len; /* index or length */
	unsigned	block_length;
	unsigned	ndp_index;
	unsigned	reserved1;
	unsigned	reserved2;
	unsigned	next_ndp_index;
};

struct gether {
	u16 cdc_filter;
	u32	header_len;
	u32	fixed_out_len;
	u32	fixed_in_len;
};

struct f_ncm {
	u8	ctrl_id;
	u8	data_id;
	char ethaddr[14];
	u8	notify_state;
	ncm_state state;
	bool is_open;
#ifdef NCM_IAP_ENABLE
	rt_device_t dev;
#endif
	struct ndp_parser_opts* parser_opts;
	bool is_crc;
	u16	ndp_dgram_count;
	u32	ndp_sign;
	struct gether port;
	bool timer_force_tx;
	bool timer_stopping;
	hal_timer_id_t hrtimer;

	struct sk_buff *skb_tx_data;
	struct sk_buff *skb_tx_ndp;
	sem_t recv_sk_buff_list_sem;
	struct list_head recv_sk_buff_list;
};
#endif
