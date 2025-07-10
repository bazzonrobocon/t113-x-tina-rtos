/*
 * Copyright (C) 2017 XRADIO TECHNOLOGY CO., LTD. All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the
 *       distribution.
 *    3. Neither the name of XRADIO TECHNOLOGY CO., LTD. nor the names of
 *       its contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "cmd_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "btmg_dev_list.h"
#include "bt_manager.h"

btmg_callback_t btmg_cbs;
dev_list_t *discovered_devices = NULL;

#ifdef CONFIG_BT_A2DP_ENABLE
extern void btcli_a2dp_source_connection_state_cb(const char *bd_addr,
                                                  btmg_a2dp_source_connection_state_t state);
extern void btcli_a2dp_source_audio_state_cb(const char *bd_addr,
                                             btmg_a2dp_source_audio_state_t state);
extern void btcli_a2dp_sink_connection_state_cb(const char *bd_addr,
                                                btmg_a2dp_sink_connection_state_t state);
extern void btcli_a2dp_sink_audio_state_cb(const char *bd_addr, btmg_a2dp_sink_audio_state_t state);
extern void btcli_a2dp_sink_stream_cb(const char *bd_addr, uint16_t channels, uint16_t sampling,
                                      uint8_t *data, uint32_t len);
extern void btcli_avrcp_ct_play_state_cb(const char *bd_addr, btmg_avrcp_play_state_t state);
extern void btcli_avrcp_ct_track_changed_cb(const char *bd_addr, btmg_track_info_t *track_info);
extern void btcli_avrcp_ct_play_position_cb(const char *bd_addr, int song_len, int song_pos);
extern void btcli_avrcp_audio_volume_cb(const char *bd_addr, unsigned int volume);
extern void btcli_avrcp_tg_play_state_cb(const char *bd_addr, btmg_avrcp_play_state_t state);
#endif
#ifdef CONFIG_BT_SPP_ENABLED
extern void btcli_sppc_conn_status_cb(const char *bd_addr, btmg_spp_connection_state_t status);
extern void btcli_sppc_recvdata_cb(const char *bd_addr, char *data, int data_len);
extern void btcli_spps_conn_status_cb(const char *bd_addr, btmg_spp_connection_state_t status);
extern void btcli_spps_recvdata_cb(const char *bd_addr, char *data, int data_len);
#endif
#ifdef CONFIG_BT_HFP_CLIENT_ENABLE
extern void btcli_hfp_hf_event_cb(btmg_hfp_hf_event_t event, void *data);
extern void btcli_hfp_hf_connection_state_cb(const char *bd_addr,
                                             btmg_hfp_hf_connection_state_t state);
#endif
#ifdef CONFIG_BT_HFP_AG_ENABLE
extern void btcli_hfp_ag_event_cb(btmg_hfp_ag_event_t event, void *data);
extern void btcli_hfp_ag_connection_state_cb(const char *bd_addr,
                                             btmg_hfp_ag_connection_state_t state);
extern void btcli_hfp_ag_audio_incoming_cb(const uint8_t *buf, uint32_t sz);
extern uint32_t btcli_hfp_ag_audio_outgoing_cb(uint8_t *p_buf, uint32_t sz);
#endif
extern void btcli_ble_scan_cb(le_scan_cb_para_t *data);
extern void btcli_ble_connection_cb(le_connection_para_t *data);
extern void btcli_gattc_dis_att_cb(gattc_dis_cb_para_t *data);
extern void btcli_gattc_notify_indicate_cb(gattc_notify_indicate_cb_para_t *data);
extern void btcli_gatts_get_db_cb(gatts_get_db_t *data);
extern void btcli_gatts_char_read_req_cb(gatts_char_read_req_t *data);
extern void btcli_gatts_char_write_req_cb(gatts_char_write_req_t *data);
extern void btcli_gatts_ccc_cfg_cb(gatts_ccc_cfg_t *data);
extern void btcli_gatts_indicate_cb(gatts_indicate_cb_t *data);
extern void btcli_gattc_read_cb(gattc_read_cb_para_t *data);
extern void btcli_gattc_write_cb(gattc_write_cb_para_t *data);

#define AUTH_IDLE            0
#define AUTH_PIN_CODE        1
#define AUTH_ENTER_PASSKEY   2
#define AUTH_PASSKEY_CONFIRM 3
#define AUTH_PAIRING_CONFIRM 4

typedef struct {
    char addr[18];
    uint32_t passkey;
    uint8_t flag; //0:none 1:pin code 2:enter passkey 3:passkey_confirm 4: pairing confirm
} pair_req_t;

static btmg_gatt_db_t *db;
static int ble_gatt_register_service(void)
{
    if (db != NULL) {
        CMD_ERR("gatt already registered\n");
        return 0;
    }

    btmg_uuid_t uuid;
    btmg_gatt_properties_t prop;
    btmg_gatt_permission_t perm = BTMG_GATT_PERM_READ | BTMG_GATT_PERM_WRITE;

    /* service1 start, uuid=0xABCD */
    db = btmg_gatt_attr_create(12); //CHAR+2 other+1
    uuid.type = BTMG_UUID_16;
    uuid.value.u16 = 0xABCD;
    btmg_gatt_attr_primary_service(db, uuid); // +1

    uuid.value.u16 = 0xfff2;
    prop = BTMG_GATT_CHRC_READ | BTMG_GATT_CHRC_WRITE;
    btmg_gatt_attr_characteristic(db, uuid, prop, perm); // +2

    uuid.value.u16 = 0xfff3;
    prop = BTMG_GATT_CHRC_READ | BTMG_GATT_CHRC_WRITE | BTMG_GATT_CHRC_NOTIFY | BTMG_GATT_CHRC_INDICATE;
    btmg_gatt_attr_characteristic(db, uuid, prop, perm); // +2
    btmg_gatt_attr_ccc(db, perm);                        // +1
    /* service1 end*/
    /* service2 start, uuid=0x7788 */
    uuid.type = BTMG_UUID_16;
    uuid.value.u16 = 0x7788;
    btmg_gatt_attr_primary_service(db, uuid); // +1

    uuid.value.u16 = 0x1122;
    prop = BTMG_GATT_CHRC_READ;
    btmg_gatt_attr_characteristic(db, uuid, prop, perm); // +2

    uuid.value.u16 = 0x3344;
    prop = BTMG_GATT_CHRC_READ | BTMG_GATT_CHRC_WRITE | BTMG_GATT_CHRC_NOTIFY;
    btmg_gatt_attr_characteristic(db, uuid, prop, perm); // +2
    btmg_gatt_attr_ccc(db, perm);                        // +1
    /* service2 end*/

    btmg_gatt_register_service(db);

    return CMD_STATUS_OK;
}

static int ble_gatt_unregister_service(void)
{
    if (db == NULL) {
        CMD_ERR("gatt already unregistered\n");
        return 0;
    }

    btmg_gatt_unregister_service(db);

    return CMD_STATUS_OK;
}

static int ble_set_adv_data(void)
{
    int index = 0;
    uint16_t uuid = 0x1234;
    btmg_adv_scan_rsp_data_t adv_data;

    adv_data.data[index] = 0x02; /* flag len */
    adv_data.data[index + 1] = 0x01; /* type for flag */
    adv_data.data[index + 2] = 0x1A; //0x05

    /* AD Type: Complete local name */
    // //reply in broadcast scan reply packet
    // char ble_name[] = "aw-btcli-ble";
    // index += adv_data.data[index] + 1;
    // adv_data.data[index] = strlen(ble_name) + 1; /* name len */
    // adv_data.data[index + 1] = 0x09; /* type for local name */
    // int name_len;
    // name_len = strlen(ble_name);
    // strcpy(&(adv_data.data[index + 2]), ble_name);

    index += adv_data.data[index] + 1;
    adv_data.data[index] = 0x03; /* uuid len */
    adv_data.data[index + 1] = 0x03; /* type for complete list of 16-bit uuid */
    adv_data.data[index + 2] = (char)(uuid & 0xFF);
    adv_data.data[index + 3] = (char)((uuid >> 8) & 0xFF);
    index += adv_data.data[index] + 1;

    adv_data.data_len = index;

    return btmg_le_set_adv_scan_rsp_data(&adv_data, NULL);
}

static int ble_set_ext_adv_data(void)
{
    int index = 0;
    uint16_t uuid = 0x1234;
    btmg_adv_scan_rsp_data_t adv_data;

    adv_data.data[index] = 0x02; /* flag len */
    adv_data.data[index + 1] = 0x01; /* type for flag */
    adv_data.data[index + 2] = 0x06; //0x05

    index += adv_data.data[index] + 1;
    adv_data.data[index] = 0x02;
    adv_data.data[index + 1] = 0x0a; /* TX power level */
    adv_data.data[index + 2] = 0xeb;

    /* AD Type: Complete local name */
    //reply in broadcast scan reply packet
    // char ble_name[] = "aw-btcli-ble";
    // index += adv_data.data[index] + 1;
    // adv_data.data[index] = strlen(ble_name) + 1; /* name len */
    // adv_data.data[index + 1] = 0x09; /* type for local name */
    // int name_len;
    // name_len = strlen(ble_name);
    // strcpy(&(adv_data.data[index + 2]), ble_name);

    index += adv_data.data[index] + 1;
    adv_data.data[index] = 0x03; /* uuid len */
    adv_data.data[index + 1] = 0x03; /* type for complete list of 16-bit uuid */
    adv_data.data[index + 2] = (char)(uuid & 0xFF);
    adv_data.data[index + 3] = (char)((uuid >> 8) & 0xFF);
    index += adv_data.data[index] + 1;
    adv_data.data_len = index;

    return btmg_le_set_adv_scan_rsp_data(&adv_data, NULL);
}

static int ble_advertise_on(void)
{
    btmg_le_adv_param_t adv_param;

    adv_param.interval_min = 0x0020;
    adv_param.interval_max = 0x01E0;
    adv_param.adv_type = BTMG_LE_ADV_IND;

    btmg_le_set_adv_param(&adv_param);
    ble_set_adv_data();
    return btmg_le_enable_adv(true);
}

// fix unused-function
__attribute__((__unused__)) static int ble_ext_advertise_on(void)
{
    btmg_le_ext_adv_param_t ext_adv_param;

    ext_adv_param.prim_min_interval[0] = 0x00, ext_adv_param.prim_min_interval[1] = 0x00,
    ext_adv_param.prim_min_interval[2] = 0x30;
    ext_adv_param.prim_max_interval[0] = 0x00, ext_adv_param.prim_max_interval[1] = 0x00,
    ext_adv_param.prim_max_interval[2] = 0x30;

    btmg_le_set_ext_adv_param(&ext_adv_param);
    ble_set_ext_adv_data();
    return btmg_le_enable_ext_adv(true);
}

static pair_req_t pair_req = { { 0 }, 0, 0 };

void btcli_scan_status_cb(btmg_scan_state_t status)
{
    if (status == BTMG_SCAN_STARTED) {
        CMD_DBG("start scanning\n");
    } else if (status == BTMG_SCAN_STOPPED) {
        CMD_DBG("stop scanning\n");
    }
}

void btcli_scan_dev_add_cb(btmg_device_t *device)
{
    dev_node_t *dev_node = NULL;

    CMD_DBG("name:[%s]\t\taddress:[%s]\t\tclass:[%d]\t\trssi:[%d]\n", device->name, device->address,
            device->cod, device->rssi);

    dev_node = btmg_dev_list_find_device(discovered_devices, device->address);
    if (dev_node != NULL) {
        return;
    }

    btmg_dev_list_add_device(discovered_devices, device->name, device->address, 0);
}

void btcli_adapter_name_cb(char *name)
{
    CMD_DBG("btcli get adapter name:%s\n", name);
}

void btcli_device_name_cb(char *name)
{
    CMD_DBG("btcli get remote device name:%s\n", name);
}

void btcli_bonded_device_cb(btmg_bond_state_t state, const char *bd_addr)
{
    dev_node_t *dev_discovered_node = NULL;

    if (state == BTMG_BOND_STATE_UNBONDED) {
        CMD_DBG("unpair bond device addr:%s\n", bd_addr);
        return;
    }

    CMD_DBG("bonded device state:%d, addr:%s\n", state, bd_addr);
    dev_discovered_node = btmg_dev_list_find_device(discovered_devices, bd_addr);
    if (state == BTMG_BOND_STATE_BONDED) {
        if (dev_discovered_node != NULL) {
            btmg_dev_list_remove_device(discovered_devices, bd_addr);
        }
        CMD_DBG("Pairing state for %s is BONDED \n", bd_addr);
    } else if (state == BTMG_BOND_STATE_BOND_FAILED) {
        CMD_DBG("Pairing state for %s is BOND_FAILED \n", bd_addr);
    }
}

void btcli_device_pindcoe_request_cb(const char *addr)
{
    CMD_DBG("dev:%s,pindcoe request, Please enter pincode\n", addr);
    memcpy(pair_req.addr, addr, sizeof(pair_req.addr));
    pair_req.flag = AUTH_PIN_CODE;
}

void btcli_device_passkey_request_cb(const char *addr)
{
    CMD_DBG("dev:%s,passkey_request, Please enter passkey!\n", addr);
    memcpy(pair_req.addr, addr, sizeof(pair_req.addr));
    pair_req.flag = AUTH_ENTER_PASSKEY;
}

void btcli_device_passkey_confirm_cb(const char *addr, uint32_t passkey)
{
    CMD_DBG("please confirm the passkey(%d) sent by the %s device!\n", passkey, addr);
    memcpy(pair_req.addr, addr, sizeof(pair_req.addr));
    pair_req.passkey = passkey;
    pair_req.flag = AUTH_PASSKEY_CONFIRM;
}

void btcli_device_pairing_confirm_cb(const char *addr, uint32_t passkey)
{
    CMD_DBG("dev:%s,pairing_confirm,please compare the passkey numeric value: %d\n", addr, passkey);
    memcpy(pair_req.addr, addr, sizeof(pair_req.addr));
    pair_req.passkey = passkey;
    pair_req.flag = AUTH_PAIRING_CONFIRM;
}

void btcli_adapter_status_cb(btmg_adapter_state_t status)
{
    char bt_addr[18] = { 0 };
    char bt_name_buf[64] = { 0 };
    char bt_name[68] = { 0 };

    if (status == BTMG_ADAPTER_OFF) {
        CMD_DBG("BT is off\n");
    } else if (status == BTMG_ADAPTER_ON) {
        CMD_DBG("BT is on\n");
        btmg_adapter_get_address(bt_addr);
        if (bt_addr[0] != '\0') {
            snprintf(bt_name_buf, 12, "aw-btcli-%s-", (char *)(bt_addr + 12));
            sprintf(bt_name, "%s-%s", bt_name_buf, (char *)(bt_addr + 15));
            btmg_adapter_set_name(bt_name);
        } else {
            btmg_adapter_set_name("aw-btcli");
        }
        btmg_adapter_set_io_capability(BTMG_IO_CAP_NOINPUTNOOUTPUT);
        btmg_adapter_set_scanmode(BTMG_SCAN_MODE_CONNECTABLE_DISCOVERABLE);
    } else if (status == BTMG_ADAPTER_TURNING_ON) {
        CMD_DBG("BT is turnning on\n");
    } else if (status == BTMG_ADAPTER_TURNING_OFF) {
        CMD_DBG("BT is turnning off\n");
    }
}

/* btcli pincode <0000~9999> */
enum cmd_status btcli_pincode(char *cmd)
{
    if (pair_req.flag != AUTH_PIN_CODE) {
        CMD_ERR("No need to enter pin code now\n");
        return CMD_STATUS_FAIL;
    }

    int argc;
    char *argv[1];

    argc = cmd_parse_argv(cmd, argv, 1);
    if (argc < 1) {
        CMD_ERR("invalid param number %d\n", argc);
        return CMD_STATUS_INVALID_ARG;
    }

    btmg_device_pincode_reply(argv[0]);
    pair_req.flag = AUTH_IDLE;

    return CMD_STATUS_OK;
}

#if (BT_SSP_INCLUDED == TRUE)
/* btcli passkey <000000~999999> */
enum cmd_status btcli_passkey(char *cmd)
{
    if (pair_req.flag != AUTH_ENTER_PASSKEY) {
        CMD_ERR("No need to enter passkey now\n");
        return CMD_STATUS_FAIL;
    }

    uint32_t passkey = 0;
    int argc;
    char *argv[1];

    argc = cmd_parse_argv(cmd, argv, 1);
    if (argc < 1) {
        CMD_ERR("invalid param number %d\n", argc);
        return CMD_STATUS_INVALID_ARG;
    }
    passkey = atoi(argv[0]);
    if (passkey > 999999) {
        CMD_ERR("Passkey should be between 0-999999");
        return CMD_STATUS_INVALID_ARG;
    }

    btmg_device_passkey_reply(passkey);
    pair_req.flag = AUTH_IDLE;

    return CMD_STATUS_OK;
}

/* btcli passkey_confirm */
enum cmd_status btcli_passkey_confirm(char *cmd)
{
    if (pair_req.flag != AUTH_PASSKEY_CONFIRM) {
        CMD_ERR("No need to enter this command now\n");
        return CMD_STATUS_FAIL;
    }

    btmg_device_passkey_confirm(pair_req.passkey);
    pair_req.flag = AUTH_IDLE;

    return CMD_STATUS_OK;
}

/* btcli pairing_confirm */
enum cmd_status btcli_pairing_confirm(char *cmd)
{
    if (pair_req.flag != AUTH_PAIRING_CONFIRM) {
        CMD_ERR("No need to need to enter this command now\n");
        return CMD_STATUS_FAIL;
    }

    btmg_device_pairing_confirm();
    pair_req.flag = AUTH_IDLE;

    return CMD_STATUS_OK;
}
#endif

/* btcli scan_list */
enum cmd_status btcli_scan_list(char *cmd)
{
    dev_node_t *dev_node = NULL;

    if (discovered_devices == NULL) {
        CMD_WRN("scan list is empty\n");
        return CMD_STATUS_OK;
    }

    dev_node = discovered_devices->head;
    while (dev_node != NULL) {
        CMD_DBG("addr: %s, name: %s\n", dev_node->dev_addr, dev_node->dev_name);
        dev_node = dev_node->next;
    }

    return CMD_STATUS_OK;
}

/* btcli paired_list */
enum cmd_status btcli_paired_list(char *cmd)
{
    int device_num = -1;
    bool connected_state = false;
    btmg_err ret;
    dev_node_t *dev_node = NULL;
    dev_list_t connected_devices;

    btmg_get_paired_device_num(&device_num);
    if (device_num <= 0) {
        CMD_DBG("no device is bonded!\n");
        return CMD_STATUS_OK;
    }

    btmg_paired_device_t paired_list[device_num];

    if ((ret = btmg_get_paired_devices(device_num, paired_list)) != BT_OK) {
        CMD_ERR("return failed: %d\n", ret);
        return CMD_STATUS_FAIL;
    }

    btmg_get_connnected_dev_list(&connected_devices);
    CMD_DBG("get paired device_list:\n");
    for (int i = 0; i < device_num; i++) {
        dev_node = btmg_dev_list_find_device(&connected_devices, paired_list[i].address);
        if (dev_node == NULL) {
            connected_state = false;
        }else {
            connected_state = true;
        }
        CMD_DBG("bond_device: addr[%s] \t name[%s] \tconnected_state[%d]\n", paired_list[i].address, paired_list[i].name, connected_state);
    }

    return CMD_STATUS_OK;
}

/* btcli unpair device */
enum cmd_status btcli_unpair_dev(char *cmd)
{
    int device_num = -1;

    btmg_get_paired_device_num(&device_num);
    if (device_num <= 0) {
        CMD_DBG("no device is bonded!\n");
        return CMD_STATUS_OK;
    }

    int argc;
    char *argv[1];
    argc = cmd_parse_argv(cmd, argv, 1);
    if (argc != 1) {
        CMD_ERR("invalid param number %d\n", argc);
        return CMD_STATUS_INVALID_ARG;
    }

    btmg_adapter_unpair_device(argv[0]);

    return CMD_STATUS_OK;
}

/* btcli scan_mode <0~2> */
enum cmd_status btcli_set_scanmode(char *cmd)
{
    int argc;
    char *argv[1];
    int mode;

    argc = cmd_parse_argv(cmd, argv, 1);
    if (argc != 1) {
        CMD_ERR("invalid param number %d\n", argc);
        return CMD_STATUS_INVALID_ARG;
    }
    mode = cmd_atoi(argv[0]);

    btmg_adapter_set_scanmode(mode);

    return CMD_STATUS_OK;
}

/* btcli scan <on/off> */
enum cmd_status btcli_scan(char *cmd)
{
    int argc;
    char *argv[1];

    argc = cmd_parse_argv(cmd, argv, cmd_nitems(argv));
    if (argc != 1) {
        CMD_ERR("invalid param number %d\n", argc);
        return CMD_STATUS_INVALID_ARG;
    }

    if (!strcmp(argv[0], "on")) {
        if (discovered_devices != NULL) {
            btmg_dev_list_free(discovered_devices);
            discovered_devices = NULL;
        }
        discovered_devices = btmg_dev_list_new();
        if (discovered_devices == NULL)
            return CMD_STATUS_FAIL;
        btmg_adapter_start_scan();
    } else if (!strcmp(argv[0], "off")) {
        btmg_adapter_stop_scan();
    }

    return CMD_STATUS_OK;
}

/* btcli io_cap <0~4> */
enum cmd_status btcli_set_io_cap(char *cmd)
{
    int argc;
    char *argv[1];
    int io_cap;

    argc = cmd_parse_argv(cmd, argv, 1);
    if (argc != 1) {
        CMD_ERR("invalid param number %d\n", argc);
        return CMD_STATUS_INVALID_ARG;
    }
    io_cap = cmd_atoi(argv[0]);

    btmg_adapter_set_io_capability(io_cap);

    return CMD_STATUS_OK;
}

/* btcli get_dev_name */
enum cmd_status btcli_get_device_name(char *cmd)
{
    int argc;
    char *argv[1];

    argc = cmd_parse_argv(cmd, argv, 1);
    if (argc != 1) {
        CMD_ERR("invalid param number %d\n", argc);
        return CMD_STATUS_INVALID_ARG;
    }

    btmg_device_get_name(argv[0]);

    return CMD_STATUS_OK;
}

/* btcli get_name */
enum cmd_status btcli_get_adapter_name(char *cmd)
{
    btmg_adapter_get_name();

    return CMD_STATUS_OK;
}

/* btcli set_name <name> */
enum cmd_status btcli_set_adapter_name(char *cmd)
{
    int argc;
    char *argv[1];

    argc = cmd_parse_argv(cmd, argv, 1);
    if (argc != 1) {
        CMD_ERR("invalid param number %d\n", argc);
        return CMD_STATUS_INVALID_ARG;
    }

    btmg_adapter_set_name(argv[0]);

    return CMD_STATUS_OK;
}

/* btcli get_mac */
enum cmd_status btcli_get_adapter_mac(char *cmd)
{
    char mac[18] = { 0 };

    btmg_adapter_get_address(mac);
    CMD_DBG("adapter mac is:%s\n", mac);

    return CMD_STATUS_OK;
}

enum cmd_status btcli_debug(char *cmd)
{
    int argc;
    char *argv[1];
    char log_level;

    argc = cmd_parse_argv(cmd, argv, 1);
    if (argc != 1) {
        CMD_ERR("invalid param number %d\n", argc);
        return CMD_STATUS_INVALID_ARG;
    }

    log_level = cmd_atoi(argv[0]);
    btmg_set_loglevel((btmg_log_level_t)log_level);

    return CMD_STATUS_OK;
}

enum cmd_status btcli_ex_debug(char *cmd)
{
    int argc;
    char *argv[1];
    char mask;

    argc = cmd_parse_argv(cmd, argv, 1);
    if (argc != 1) {
        CMD_ERR("invalid param number %d\n", argc);
        return CMD_STATUS_INVALID_ARG;
    }

    mask = cmd_atoi(argv[0]);
    btmg_set_ex_debug(mask);

    return CMD_STATUS_OK;
}

static int profile = 0;
/* btcli init or btcli init -p a2dp-source -d 4 */
enum cmd_status btcli_init(char *cmd)
{
    int i = 0;
    int argc;
    char *argv[10];
    int cmd_profile = 0;
    btmg_log_level_t debug;

    argc = cmd_parse_argv(cmd, argv, cmd_nitems(argv));

    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0) {
            CMD_DBG("Usage:\n"
                   "  [OPTION]...\n"
                   "\nOptions:\n"
                   "  -h, --help\t\tprint this help and exit\n"
                   "  -d, --debug\t open debug :-d [0~5]\n"
                   "  -p, --profile=NAME\tenable bluetooth profile\n"
                   "  [supported profile name]:\n"
                   "  - a2dp-source\tAdvanced Audio Source\n"
                   "  - a2dp-sink\tAdvanced Audio Sink\n"
                   "  - hfp-hf\tHands-Free\n"
                   "  - spp-server\tSerial Port Profile-Server role\n"
                   "  - spp-client\tSerial Port Profile-Client role\n"
                   "  - gatt-server\tble gatt server\n"
                   "  - gatt-client\tble gatt client\n");
            return CMD_STATUS_OK;
        }

        if (strcmp(argv[i], "-p") == 0) {
            if (i + 1 < argc) {
                int c;
                struct {
                    const char *name;
                    int value;
                } map[] = {
                    { "a2dp-sink",      BTMG_A2DP_SINK },
                    { "a2dp-source",    BTMG_A2DP_SOURCE },
                    { "hfp-hf",         BTMG_HFP_HF},
                    { "hfp-ag",         BTMG_HFP_AG},
                    { "spp-server",     BTMG_SPP_SERVER },
                    { "spp-client",     BTMG_SPP_CLIENT },
                    { "gatt-server",    BTMG_GATT_SERVER },
                    { "gatt-client",    BTMG_GATT_CLIENT},
                };

                for (c = 0; c < ARRAY_SIZE(map); c++) {
                    if (strcasecmp(argv[i+1], map[c].name) == 0) {
                        cmd_profile |= map[c].value;
                        break;
                    }
                }
                if (c == ARRAY_SIZE(map)) {
                    CMD_ERR("Invalid BT profile name: %s\n", argv[i+1]);
                }
            }else {
                CMD_ERR("invalid parameter, please specify a profile\n");
                break;
            }
        }

        if (strcmp(argv[i], "-d") == 0) {
            if (i + 1 < argc) {
                debug = (btmg_log_level_t)atoi(argv[i+1]);
                CMD_DBG("debug loglevel :%d\n", (int)debug);
                btmg_set_loglevel(debug);
            } else {
                CMD_ERR("invalid parameter, please specify a level\n");
                break;
            }
        }
    }

    if (profile == 0 && cmd_profile == 0) {
        CMD_DBG("use default profile\n");
        cmd_profile |= BTMG_A2DP_SINK;
    } else if (profile != 0 && cmd_profile == 0) {
        CMD_ERR("btmanager already init, please use '-p' add new profile\n");
        return CMD_STATUS_OK;
    }

    btmg_cbs.btmg_adapter_cb.state_cb = btcli_adapter_status_cb;
    btmg_cbs.btmg_adapter_cb.scan_status_cb = btcli_scan_status_cb;
    btmg_cbs.btmg_device_cb.device_add_cb = btcli_scan_dev_add_cb;
    btmg_cbs.btmg_adapter_cb.bonded_device_cb = btcli_bonded_device_cb;
    btmg_cbs.btmg_adapter_cb.get_name_cb = btcli_adapter_name_cb;
    btmg_cbs.btmg_device_cb.get_name_cb = btcli_device_name_cb;
    btmg_cbs.btmg_device_cb.pindcoe_request_cb = btcli_device_pindcoe_request_cb;
    btmg_cbs.btmg_device_cb.passkey_request_cb = btcli_device_passkey_request_cb;
    btmg_cbs.btmg_device_cb.passkey_confirm_cb = btcli_device_passkey_confirm_cb;
    btmg_cbs.btmg_device_cb.pairing_confirm_cb = btcli_device_pairing_confirm_cb;
#ifdef CONFIG_BT_A2DP_ENABLE
    if (cmd_profile & BTMG_A2DP_SINK) {
        btmg_cbs.btmg_a2dp_sink_cb.conn_state_cb = btcli_a2dp_sink_connection_state_cb;
        btmg_cbs.btmg_a2dp_sink_cb.audio_state_cb = btcli_a2dp_sink_audio_state_cb;
        btmg_cbs.btmg_a2dp_sink_cb.stream_cb = btcli_a2dp_sink_stream_cb;
    }
    if (cmd_profile & BTMG_A2DP_SOURCE) {
        btmg_cbs.btmg_a2dp_source_cb.conn_state_cb = btcli_a2dp_source_connection_state_cb;
        btmg_cbs.btmg_a2dp_source_cb.audio_state_cb = btcli_a2dp_source_audio_state_cb;
    }
    btmg_cbs.btmg_avrcp_cb.avrcp_ct_play_state_cb = btcli_avrcp_ct_play_state_cb;
    btmg_cbs.btmg_avrcp_cb.avrcp_ct_play_position_cb = btcli_avrcp_ct_play_position_cb;
    btmg_cbs.btmg_avrcp_cb.avrcp_ct_track_changed_cb = btcli_avrcp_ct_track_changed_cb;
    btmg_cbs.btmg_avrcp_cb.avrcp_tg_play_state_cb = btcli_avrcp_tg_play_state_cb;
    btmg_cbs.btmg_avrcp_cb.avrcp_audio_volume_cb = btcli_avrcp_audio_volume_cb;
#endif
#ifdef CONFIG_BT_SPP_ENABLED
    if (cmd_profile & BTMG_SPP_CLIENT) {
        btmg_cbs.btmg_sppc_cb.conn_state_cb = btcli_sppc_conn_status_cb;
        btmg_cbs.btmg_sppc_cb.recvdata_cb = btcli_sppc_recvdata_cb;
    }
    if (cmd_profile & BTMG_SPP_SERVER) {
        btmg_cbs.btmg_spps_cb.conn_state_cb = btcli_spps_conn_status_cb;
        btmg_cbs.btmg_spps_cb.recvdata_cb = btcli_spps_recvdata_cb;
    }
#endif
#ifdef CONFIG_BT_HFP_CLIENT_ENABLE
    if (cmd_profile & BTMG_HFP_HF) {
        btmg_cbs.btmg_hfp_hf_cb.conn_state_cb = btcli_hfp_hf_connection_state_cb;
        btmg_cbs.btmg_hfp_hf_cb.event_cb = btcli_hfp_hf_event_cb;
    }
#endif
#ifdef CONFIG_BT_HFP_AG_ENABLE
    if (cmd_profile & BTMG_HFP_AG) {
        btmg_cbs.btmg_hfp_ag_cb.conn_state_cb = btcli_hfp_ag_connection_state_cb;
        btmg_cbs.btmg_hfp_ag_cb.event_cb = btcli_hfp_ag_event_cb;
        btmg_cbs.btmg_hfp_ag_cb.audio_incoming_cb = btcli_hfp_ag_audio_incoming_cb;
        btmg_cbs.btmg_hfp_ag_cb.audio_outgoing_cb = btcli_hfp_ag_audio_outgoing_cb;
    }
#endif
    if (cmd_profile & BTMG_GATT_SERVER) {
        btmg_cbs.btmg_gatts_cb.conn_cb = btcli_ble_connection_cb;
        btmg_cbs.btmg_gatts_cb.get_db_cb = btcli_gatts_get_db_cb;
        btmg_cbs.btmg_gatts_cb.char_read_req_cb = btcli_gatts_char_read_req_cb;
        btmg_cbs.btmg_gatts_cb.char_write_req_cb = btcli_gatts_char_write_req_cb;
        btmg_cbs.btmg_gatts_cb.ccc_cfg_cb = btcli_gatts_ccc_cfg_cb;
        btmg_cbs.btmg_gatts_cb.indicate_cb = btcli_gatts_indicate_cb;
    }
    if (cmd_profile & BTMG_GATT_CLIENT) {
        btmg_cbs.btmg_gattc_cb.le_scan_cb = btcli_ble_scan_cb;
        btmg_cbs.btmg_gattc_cb.conn_cb = btcli_ble_connection_cb;
        btmg_cbs.btmg_gattc_cb.dis_att_cb = btcli_gattc_dis_att_cb;
        btmg_cbs.btmg_gattc_cb.read_cb = btcli_gattc_read_cb;
        btmg_cbs.btmg_gattc_cb.write_cb = btcli_gattc_write_cb;
        btmg_cbs.btmg_gattc_cb.notify_indicate_cb = btcli_gattc_notify_indicate_cb;
    }

    btmg_core_init();
    btmg_register_callback(&btmg_cbs);
    btmg_set_profile(cmd_profile);
    btmg_adapter_enable(true);
    profile |= cmd_profile;
    if (cmd_profile & BTMG_GATT_SERVER) {
        ble_gatt_register_service();
#ifdef CONFIG_BT_EXT_ADV
        ble_ext_advertise_on();
#else
        ble_advertise_on();
#endif
    }

    return CMD_STATUS_OK;
}

extern void btcli_gatts_all_attributedata_delete();

enum cmd_status btcli_deinit(char *cmd)
{
    if (profile & BTMG_GATT_SERVER) {
        ble_gatt_unregister_service();
        btcli_gatts_all_attributedata_delete();
        btmg_le_enable_adv(false);
        btmg_gatt_attr_destory(db);
        db  = NULL;
    }
    profile = 0;
    btmg_adapter_enable(false);
    btmg_core_deinit();
    btmg_unregister_callback();

    if (discovered_devices != NULL) {
        btmg_dev_list_free(discovered_devices);
        discovered_devices = NULL;
    }

    return CMD_STATUS_OK;
}
