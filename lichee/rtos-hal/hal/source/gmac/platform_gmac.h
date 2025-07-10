/*
* Allwinner GMAC driver.
*
* Copyright(c) 2022-2027 Allwinnertech Co., Ltd.
*
* This file is licensed under the terms of the GNU General Public
* License version 2.  This program is licensed "as is" without any
* warranty of any kind, whether express or implied.
*/

#ifndef __PLATFORM_GMAC_H__
#define __PLATFORM_GMAC_H__

#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "lwip/ethip6.h"
#include "lwip/etharp.h"
#include "netif/ppp/pppoe.h"

#include <FreeRTOS.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <hal_gpio.h>
#include <hal_log.h>
#include <hal_clk.h>
#include <hal_reset.h>
#include <hal_timer.h>
#include <hal_mem.h>
#include <hal_cache.h>
#include <console.h>

#include <hal_mii.h>
#include <hal_mdio.h>
#include <hal_miiphy.h>
#include <hal_phy.h>

#include <osal/hal_mem.h>

#if defined(CONFIG_ARCH_SUN8IW20)
#include "platform/gmac_sun8iw20.h"
#include "arch/riscv/barrier.h"
#endif

#define GMAC_BASIC_CTL0		0x00
#define GMAC_BASIC_CTL1		0x04
#define GMAC_INT_STA		0x08
#define GMAC_INT_EN		0x0C
#define GMAC_TX_CTL0		0x10
#define GMAC_TX_CTL1		0x14
#define GMAC_TX_FLOW_CTL	0x1C
#define GMAC_TX_DESC_LIST	0x20
#define GMAC_RX_CTL0		0x24
#define GMAC_RX_CTL1		0x28
#define GMAC_RX_DESC_LIST	0x34
#define GMAC_RX_FRM_FLT		0x38
#define GMAC_RX_HASH0		0x40
#define GMAC_RX_HASH1		0x44
#define GMAC_MDIO_ADDR		0x48
#define GMAC_MDIO_DATA		0x4C
#define GMAC_ADDR_HI(reg)	(0x50 + ((reg) << 3))
#define GMAC_ADDR_LO(reg)	(0x54 + ((reg) << 3))
#define GMAC_TX_DMA_STA		0xB0
#define GMAC_TX_CUR_DESC	0xB4
#define GMAC_TX_CUR_BUF		0xB8
#define GMAC_RX_DMA_STA		0xC0
#define GMAC_RX_CUR_DESC	0xC4
#define GMAC_RX_CUR_BUF		0xC8
#define GMAC_RGMII_STA		0xD0
#define MII_BUSY		0x00000001
#define MII_WRITE		0x00000002

#define GMAC_PHY_RGMII_MASK	0x00000004
#define GMAC_ETCS_RMII_MASK	0x00002003
#define GMAC_RGMII_INTCLK_MASK	0x00000002
#define GMAC_RMII_MASK		0x00002000
#define GMAC_TX_DELAY_MASK	0x07
#define GMAC_TX_DELAY_OFFSET	10
#define GMAC_RX_DELAY_MASK	0x1F
#define GMAC_RX_DELAY_OFFSET	5

#define GMAC_RX_INT_EN		0x100
#define GMAC_SOFT_RST		0x01
#define GMAC_TX_FRM_LEN_OFFSET	30
#define	GMAC_TX_EN_OFFSET	31
#define GMAC_TX_MD		0x2
#define GMAC_TX_NEXT_FRM	0x4
#define GMAC_TX_TH_64		0x000
#define GMAC_TX_TH_128		0x100
#define GMAC_TX_MD		0x2
#define GMAC_CHECK_CRC_OFFSET	27
#define GMAC_RX_EN_OFFSET	28
#define GMAC_RX_MODE		0x02
#define GMAC_RX_DMA_MODE	0xe
#define GMAC_RX_RECV_ALL	0x1
#define GMAC_BURST_LEN		0x8
#define GMAC_BURST_LEN_OFFSET	24
#define GMAC_RX_INT_MASK	0x2300
#define GMAC_RX_CLEAR_MASK	0x3F00
#define GMAC_PHY_SELECT_OFFSET	15
#define GMAC_TX_DMA_STOP	0x0
#define GMAC_TX_DMA_SUSPEND	0x6
#define GMAC_TX_DMA_MASK	0x7
#define GMAC_DMA_OWN_DESC	0x80000000
#define GMAC_TX_DMA_ONE_DESC	0x61000000
#define GMAC_TX_DMA_CRC_FULL	(0x3 << 27)
#define GMAC_DMA_BUFF_SIZE(length)	(((1 << 11) -1) & length)
#define GMAC_TX_DMA_FLUSH_FIFO	0x00000001
#define GMAC_TX_DMA_EN		0x40000000
#define GMAC_TX_DMA_START	0x80000000
#define GMAC_RX_DMA_INT_CTL	0x81000000
#define GMAC_DMA_BUFF_SIZE_MASK	((1 << 11) -1)
#define GMAC_RX_DMA_STOP	0x0
#define GMAC_RX_DMA_EN		0x40000000
#define GMAC_RX_DMA_START	0x80000000
#define GMAC_RX_DMA_MASK	0x7
#define GMAC_HASH_MULTICAST	0x200

#define GMAC_MDIO_MDC_DIV	(0x06 << 20)
#define GMAC_MDIO_PHYADDR_MASK	0x0001F000
#define GMAC_MDIO_PHYADDR_OFFSET	12
#define GMAC_MDIO_PHYREG_MASK	0x000007F0
#define GMAC_MDIO_PHYREG_OFFSET		4

#define GMAC_MAC_DUPLEX_MASK	0x01
#define GMAC_MAC_SPEED_MASK	0x0c
#define GMAC_MAC_SPEED_100M	0x0c
#define GMAC_MAC_SPEED_1000M	0x08

#define GMAC_PHY_ADDR_MAX	32
#define GMAC_PHY_REG_MASK	0xFFFF
#define GMAC_PHY_UNUSE_ID	0x1FFFFFFF

#ifndef SZ_2K
#define SZ_2K			0x00000800
#endif

#define MAX_BUF_SZ		(SZ_2K - 1)
#define GMAC_TIMEOUT_US		100
#define GMAC_CHAIN_MODE_OFFSET	24

enum rx_frame_status { /* IPC status */
	good_frame = 0,
	discard_frame = 1,
	csum_none = 2,
	llc_snap = 4,
};

typedef union {
	struct {
		/* Tx descriptor0 */
		u32 deferred:1;         /* Deferred bit (only half-duplex) */
		u32 under_err:1;        /* Underflow error */
		u32 ex_deferral:1;      /* Excessive deferral */
		u32 coll_cnt:4;         /* Collision count */
		u32 vlan_tag:1;         /* VLAN Frame */

		u32 ex_coll:1;          /* Excessive collision */
		u32 late_coll:1;        /* Late collision */
		u32 no_carr:1;          /* No carrier */
		u32 loss_carr:1;        /* Loss of collision */

		u32 ipdat_err:1;        /* IP payload error */
		u32 frm_flu:1;          /* Frame flushed */
		u32 jab_timeout:1;      /* Jabber timeout */
		u32 err_sum:1;          /* Error summary */

		u32 iphead_err:1;       /* IP header error */
		u32 ttss:1;             /* Transmit time stamp status */
		u32 reserved0:13;
		u32 own:1;              /* Own bit. CPU:0, DMA:1 */
	} tx;

	struct {
		/* Rx desctriptor0 */
		u32 chsum_err:1;        /* Payload checksum error */
		u32 crc_err:1;          /* CRC error */
		u32 dribbling:1;        /* Dribble bit error */
		u32 mii_err:1;          /* Received error (bit3) */

		u32 recv_wt:1;          /* Received watchdog timeout */
		u32 frm_type:1;         /* Frame type */
		u32 late_coll:1;        /* Late Collision */
		u32 ipch_err:1;         /* IPv header checksum error (bit7) */

		u32 last_desc:1;        /* Laset descriptor */
		u32 first_desc:1;       /* First descriptor */
		u32 vlan_tag:1;         /* VLAN Tag */
		u32 over_err:1;         /* Overflow error (bit11) */

		u32 len_err:1;          /* Length error */
		u32 sou_filter:1;       /* Source address filter fail */
		u32 desc_err:1;         /* Descriptor error */
		u32 err_sum:1;          /* Error summary (bit15) */

		u32 frm_len:14;         /* Frame length */
		u32 des_filter:1;       /* Destination address filter fail */
		u32 own:1;              /* Own bit. CPU:0, DMA:1 */
	} rx;

	u32 all;
} desc0_u;

typedef union {
	struct {
		/* TDES1 */
		u32 buf1_size:11;       /* Transmit buffer1 size */
		u32 buf2_size:11;       /* Transmit buffer2 size */
		u32 ttse:1;             /* Transmit time stamp enable */
		u32 dis_pad:1;          /* Disable pad (bit23) */

		u32 adr_chain:1;        /* Second address chained */
		u32 end_ring:1;         /* Transmit end of ring */
		u32 crc_dis:1;          /* Disable CRC */
		u32 cic:2;              /* Checksum insertion control (bit27:28) */
		u32 first_sg:1;         /* First Segment */
		u32 last_seg:1;         /* Last Segment */
		u32 interrupt:1;        /* Interrupt on completion */
	} tx;

	struct {
		/* RDES1 */
		u32 buf1_size:11;       /* Received buffer1 size */
		u32 buf2_size:11;       /* Received buffer2 size */
		u32 reserved1:2;

		u32 adr_chain:1;        /* Second address chained */
		u32 end_ring:1;         /* Received end of ring */
		u32 reserved2:5;
		u32 dis_ic:1;           /* Disable interrupt on completion */
	} rx;

	u32 all;
} desc1_u;

typedef struct dma_desc {
       desc0_u desc0;	/* 1st: Status */
       desc1_u desc1;	/* 2nd: Buffer Size */
       u32 desc2;	/* 3rd: Buffer Addr */
       u32 desc3;	/* 4th: Next Desc */
} __attribute__((packed)) dma_desc_t;

struct ethernetif {
	struct netif *netif;
	struct eth_addr *ethaddr;

	/* private variable */
	struct reset_control *rst;
	hal_clk_t gmac_clk;
	hal_clk_t gmac25m_clk;

	struct dma_desc *dma_desc_tx;
	struct dma_desc *dma_desc_rx;

	u32 dma_desc_tx_phy;
	u32 dma_desc_rx_phy;

	unsigned char **rx_buf;

	hal_thread_t rx_task;

	bool irq_signal;

	u32 tx_clean;
	u32 tx_dirty;
	u32 rx_clean;
	u32 rx_dirty;

	u32 tx_delay;
	u32 rx_delay;

#define RMII_PIN_NUM		10
	u32 rmii_pins[RMII_PIN_NUM];
	u32 rmii_pinmux;
	u32 phyrst;
	phy_interface_t phy_interface;

	u32 base;
	u32 syscfg_base;

	hal_spinlock_t tx_lock;
};

err_t ethernetif_init(struct netif *netif);
void ethernetif_exit(struct netif *netif);
int hal_gmac_netif_init(struct ethernetif *ethernetif);
void hal_gmac_netif_exit(struct ethernetif *ethernetif);
int hal_gmac_output(struct ethernetif *ethernetif, struct pbuf *p);
struct pbuf *hal_gmac_input(struct ethernetif *ethernetif);
#endif /* __PLATFORM_GMAC_H__ */
