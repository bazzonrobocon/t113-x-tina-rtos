/*
* Allwinner GMAC netif cmdline driver.
*
* Copyright(c) 2022-2027 Allwinnertech Co., Ltd.
*
* This file is licensed under the terms of the GNU General Public
* License version 2.  This program is licensed "as is" without any
* warranty of any kind, whether express or implied.
*/

#include <lwip/tcpip.h>
#include <lwip/netif.h>
#include <lwip/inet.h>
#include "platform_gmac.h"

struct netif *eth_netif;
struct ip_info {
	ip4_addr_t ip;
	ip4_addr_t netmask;
	ip4_addr_t gw;
};
static bool is_tcpip_init = false;

void ethernet_init(void)
{
	struct ip_info ip_info;

	eth_netif = hal_malloc(sizeof(*eth_netif));
	if (!eth_netif) {
		printf("alloc netif failed\n");
		return;
	}

	IP4_ADDR(&ip_info.ip, 192, 168, 200, 10);
	IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
	IP4_ADDR(&ip_info.gw, 192, 168, 200, 1);

	if (!is_tcpip_init) {
		tcpip_init(NULL, NULL);
		is_tcpip_init = true;
	}

	netif_add(eth_netif, &ip_info.ip, &ip_info.netmask, &ip_info.gw,
			NULL,  ethernetif_init, tcpip_input);
}

void ethernet_exit(void)
{
	ethernetif_exit(eth_netif);
	netif_remove(eth_netif);
	hal_free(eth_netif);
}

void ifconfig_help(void)
{
	printf("help:\n");
	printf("ifconfig up		: up ethernet\n");
	printf("ifconfig down		: down ethernet\n");
	printf("ifconfig status		: print ethernet status\n");
	printf("ifconfig ip IPADDR	: set ipaddr\n");
	printf("ifconfig gw GW		: set gwaddr\n");
	printf("ifconfig netmask NETMASK: set netmask\n");
}

int netif_ifconfig(int argc, char **argv)
{
	ip4_addr_t tmp;

	if (argc < 2) {
		ifconfig_help();
		return -1;
	}

	if (strcmp(argv[1], "up") != 0) {
		if (eth_netif == NULL) {
			printf("ifconfig up first!\n");
			return -1;
		}
	}

	if (strcmp(argv[1], "status") == 0) {
		printf("%c%c%u\t%s\n \
		\t%s%02"X16_F":%02"X16_F":%02"X16_F":%02"X16_F":%02"X16_F":%02"X16_F"\n \
		\t%s%"U16_F".%"U16_F".%"U16_F".%"U16_F"\n \
		\t%s%"U16_F".%"U16_F".%"U16_F".%"U16_F"\n \
		\t%s%"U16_F".%"U16_F".%"U16_F".%"U16_F"\n \
		\t%s %s %s %s%d\n",
		eth_netif->name[0], eth_netif->name[1], eth_netif->num,
		"Link encap:Ethernet",
		"HWaddr ",
		eth_netif->hwaddr[0], eth_netif->hwaddr[1], eth_netif->hwaddr[2],
		eth_netif->hwaddr[3], eth_netif->hwaddr[4], eth_netif->hwaddr[5],
		"inet addr ",
		ip4_addr1_16(netif_ip4_addr(eth_netif)),
		ip4_addr2_16(netif_ip4_addr(eth_netif)),
		ip4_addr3_16(netif_ip4_addr(eth_netif)),
		ip4_addr4_16(netif_ip4_addr(eth_netif)),
		"gw addr ",
		ip4_addr1_16(netif_ip4_gw(eth_netif)),
		ip4_addr2_16(netif_ip4_gw(eth_netif)),
		ip4_addr3_16(netif_ip4_gw(eth_netif)),
		ip4_addr4_16(netif_ip4_gw(eth_netif)),
		"netmask ",
		ip4_addr1_16(netif_ip4_netmask(eth_netif)),
		ip4_addr2_16(netif_ip4_netmask(eth_netif)),
		ip4_addr3_16(netif_ip4_netmask(eth_netif)),
		ip4_addr4_16(netif_ip4_netmask(eth_netif)),
		((eth_netif->flags & NETIF_FLAG_UP) == NETIF_FLAG_UP) ? "UP" : "DOWN",
		((eth_netif->flags & NETIF_FLAG_BROADCAST) == NETIF_FLAG_BROADCAST) ? "BROADCAST" : " ",
		((eth_netif->flags & NETIF_FLAG_LINK_UP) == NETIF_FLAG_LINK_UP) ? "LINK_UP" : "LINK_DOWN",
		" mtu: ",eth_netif->mtu);
	} else if (strcmp(argv[1], "up") == 0) {
		ethernet_init();
	} else if (strcmp(argv[1], "down") == 0) {
		ethernet_exit();
	} else if (strcmp(argv[1], "ip") == 0) {
		inet_aton(argv[2], &tmp);
		ip4_addr_set(ip_2_ip4(&eth_netif->ip_addr), &tmp);
	} else if (strcmp(argv[1], "gw") == 0) {
		inet_aton(argv[2], &tmp);
		ip4_addr_set(ip_2_ip4(&eth_netif->gw), &tmp);
	} else if (strcmp(argv[1], "netmask") == 0) {
		inet_aton(argv[2], &tmp);
		ip4_addr_set(ip_2_ip4(&eth_netif->netmask), &tmp);
	}

	return 0;
}
FINSH_FUNCTION_EXPORT_CMD(netif_ifconfig, ifconfig, ethernet ifconfig);
