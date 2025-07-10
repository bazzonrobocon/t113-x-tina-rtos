/*
* Allwinner GMAC LWIP ethernet interface driver.
*
* Copyright(c) 2022-2027 Allwinnertech Co., Ltd.
*
* This file is licensed under the terms of the GNU General Public
* License version 2.  This program is licensed "as is" without any
* warranty of any kind, whether express or implied.
*/

#include "platform_gmac.h"

/* Define those to better describe your network interface. */
#define IFNAME0 'e'
#define IFNAME1 'n'

static void get_random_hwaddr(u8_t *hwaddr)
{
	int i;

	for (i = 0; i < NETIF_MAX_HWADDR_LEN; i ++)
		hwaddr[i] = (u8_t)rand();

	hwaddr[0] &= 0xfe;	/* clear multicast bit */
	hwaddr[0] |= 0x02;	/* set local assignment bit (IEEE802) */
}

/**
 * In this function, the hardware should be initialized.
 * Called from ethernetif_init().
 *
 * @param netif the already initialized lwip network interface structure
 *        for this ethernetif
 */
static void low_level_init(struct netif *netif)
{
	struct ethernetif *ethernetif = netif->state;
	u8_t hwaddr[ETHARP_HWADDR_LEN];
	int ret;

	/* set MAC hardware address length */
	netif->hwaddr_len = ETHARP_HWADDR_LEN;

	/* generate MAC hardware address */
	get_random_hwaddr(hwaddr);

	/* set MAC hardware address */
	memcpy(netif->hwaddr, hwaddr, netif->hwaddr_len);

	/* maximum transfer unit */
	netif->mtu = 1500;

	/* device capabilities */
	/* don't set NETIF_FLAG_ETHARP if this device is not an ethernet one */
	netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

#if LWIP_IPV6 && LWIP_IPV6_MLD
	/*
	 * For hardware/netifs that implement MAC filtering.
	 * All-nodes link-local is handled by default, so we must let the hardware know
	 * to allow multicast packets in.
	 * Should set mld_mac_filter previously. */
	if (netif->mld_mac_filter != NULL) {
		ip6_addr_t ip6_allnodes_ll;
		ip6_addr_set_allnodes_linklocal(&ip6_allnodes_ll);
		netif->mld_mac_filter(netif, &ip6_allnodes_ll, NETIF_ADD_MAC_FILTER);
	}
#endif /* LWIP_IPV6 && LWIP_IPV6_MLD */

	ret = hal_gmac_netif_init(ethernetif);
	if (ret == 0)
		netif_set_up(netif);
	else
		hal_free(ethernetif);
}

/**
 * This function should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
 * @return ERR_OK if the packet could be sent
 *         an err_t value if the packet couldn't be sent
 *
 * @note Returning ERR_MEM here if a DMA queue of your MAC is full can lead to
 *       strange results. You might consider waiting for space in the DMA queue
 *       to become available since the stack doesn't retry to send a packet
 *       dropped because of memory failure (except for the TCP timers).
 */

static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
	struct ethernetif *ethernetif = netif->state;

	hal_gmac_output(ethernetif, p);

	return ERR_OK;
}

/**
 * Should allocate a pbuf and transfer the bytes of the incoming
 * packet from the interface into the pbuf.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return a pbuf filled with the received packet (including MAC header)
 *         NULL on memory error
 */
static struct pbuf *low_level_input(struct netif *netif)
{
	struct ethernetif *ethernetif = netif->state;
	struct pbuf *p;

	p = hal_gmac_input(ethernetif);

	return p;
}

/**
 * This function should be called when a packet is ready to be read
 * from the interface. It uses the function low_level_input() that
 * should handle the actual reception of bytes from the network
 * interface. Then the type of the received packet is determined and
 * the appropriate input function is called.
 *
 * @param netif the lwip network interface structure for this ethernetif
 */
static void ethernetif_input(void *argument)
{
	struct pbuf *p;
	struct netif *netif = (struct netif *) argument;
	struct ethernetif *ethernetif = netif->state;

	for(;;)
	{
		while (ethernetif->irq_signal == false)
			hal_usleep(10);

		do {
			p = low_level_input(netif);
			if(p != NULL) {
				if (netif->input(p, netif) != ERR_OK)
					pbuf_free(p);
			}
		} while(p != NULL);
	}
}

/**
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 * This function should be passed as a parameter to netif_add().
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return ERR_OK if the loopif is initialized
 *         ERR_MEM if private data couldn't be allocated
 *         any other err_t on error
 */
err_t ethernetif_init(struct netif *netif)
{
	struct ethernetif *ethernetif;

	LWIP_ASSERT("netif != NULL", (netif != NULL));

	ethernetif = hal_malloc(sizeof(* ethernetif));
	if (ethernetif == NULL) {
		LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_init: out of memory\n"));
		return ERR_MEM;
	}

#if LWIP_NETIF_HOSTNAME
	/* Initialize interface hostname */
	netif->hostname = "lwip";
#endif	/* LWIP_NETIF_HOSTNAME */

	/*
	 * Initialize the snmp variables and counters inside the struct netif.
	 * The last argument should be replaced with your link speed, in units
	 * of bits per second.
	 */
	MIB2_INIT_NETIF(netif, snmp_ifType_ethernet_csmacd, LINK_SPEED_OF_YOUR_NETIF_IN_BPS);

	netif->state = ethernetif;
	netif->name[0] = IFNAME0;
	netif->name[1] = IFNAME1;
	ethernetif->netif = netif;

	/* We directly use etharp_output() here to save a function call.
	 * You can instead declare your own function an call etharp_output()
	 * from it if you have to do some checks before sending (e.g. if link
	 * is available...) */

	netif->output = etharp_output;
#if LWIP_IPV6
	netif->output_ip6 = ethip6_output;
#endif	/* LWIP_IPV6 */
	netif->linkoutput = low_level_output;

	ethernetif->ethaddr = (struct eth_addr *)&(netif->hwaddr[0]);

	/* initialize the hardware */
	low_level_init(netif);

	ethernetif->rx_task = hal_thread_create(ethernetif_input, netif, "gmac rx task",
			HAL_THREAD_STACK_SIZE, HAL_THREAD_PRIORITY_HIGHEST);

	if (!ethernetif->rx_task) {
		printf("ethernetif_init: create gmac rx task failed\n");
		return -1;
	}

	hal_thread_start(ethernetif->rx_task);

	return ERR_OK;
}

void ethernetif_exit(struct netif *netif)
{
	struct ethernetif *ethernetif = netif->state;

	hal_thread_stop(ethernetif->rx_task);
	hal_gmac_netif_exit(ethernetif);
	hal_free(ethernetif);
}
