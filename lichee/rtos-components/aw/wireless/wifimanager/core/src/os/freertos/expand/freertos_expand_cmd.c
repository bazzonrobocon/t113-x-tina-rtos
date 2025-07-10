#include <wifi_log.h>
#include <wifimg.h>
#include <stdlib.h>
#include <string.h>
#include <freertos_common.h>
#include "sysinfo.h"

#ifdef CONFIG_DRIVERS_XRADIO
#include "wlan.h"
#include "net_init.h"
#include "ethernetif.h"
#include "tcpip_adapter.h"
extern struct netif *aw_netif[IF_MAX];
#endif

wmg_status_t wmg_freertos_send_expand_cmd_gmac(char *expand_cmd, void *expand_cb)
{
	char ifname[10] = {0};
	uint8_t *mac_addr = (uint8_t *)expand_cb;
	struct sysinfo *sysinfo = sysinfo_get();

	strcpy(ifname, expand_cmd);
	if(ifname == NULL) {
		WMG_ERROR("ifname is NULL\n");
		return WMG_STATUS_FAIL;
	}

	if(sysinfo){
		memcpy(mac_addr, sysinfo->mac_addr, IEEE80211_ADDR_LEN);
		return WMG_STATUS_SUCCESS;
	} else {
		WMG_ERROR("sysinfo get faile\n");
		return WMG_STATUS_FAIL;
	}
}

wmg_status_t wmg_freertos_send_expand_cmd_smac(char *expand_cmd, void *expand_cb)
{
	uint8_t mac_addr[6] = {0};
	char ifname[10] = {0};
	struct sysinfo *sysinfo = sysinfo_get();
	char *pch;
	int i = 0;

	pch = strtok(expand_cmd, ":");
	strcpy(ifname, pch);
	pch = strtok(NULL, ":");
	pch++;
	for(i = 0;(pch != NULL) && (i < 6); i++){
		mac_addr[i] = char2uint8(pch);
		pch = strtok(NULL, ":");
	}

	if(i != 6) {
		WMG_ERROR("%s: mac address format is incorrect\n", expand_cmd);
		return WMG_STATUS_FAIL;
	}
	//set mac_addr
	if(sysinfo) {
		memcpy(sysinfo->mac_addr, mac_addr, IEEE80211_ADDR_LEN);
		//wlan_set_mac_addr(NULL, mac_addr, IEEE80211_ADDR_LEN);
	} else {
		WMG_ERROR("sysinfo get faile\n");
		return WMG_STATUS_FAIL;
	}
	//close wifi first
	xr_wlan_off();
	//default open wlan sta mode
	xr_wlan_on(WLAN_MODE_STA);
	//sync mac_addr to ifconfig cmd
	memcpy(aw_netif[MODE_STA]->hwaddr, mac_addr, NETIF_MAX_HWADDR_LEN);
	return WMG_STATUS_SUCCESS;
}

wmg_status_t wmg_freertos_send_expand_cmd(char *expand_cmd, void *expand_cb)
{
	WMG_DEBUG("freertos get exp cmd: %s\n", expand_cmd);
	if(!strncmp(expand_cmd, "gmac:", 5)) {
		return wmg_freertos_send_expand_cmd_gmac((expand_cmd + 6), expand_cb);
	} else if(!strncmp(expand_cmd, "smac:", 5)) {
		return wmg_freertos_send_expand_cmd_smac((expand_cmd + 6), expand_cb);
	} else {
		WMG_ERROR("unspport freertos expand_cmd: %s\n", expand_cmd);
	}
	return WMG_STATUS_FAIL;
}
