#include <stdio.h>
#include <stdlib.h>

#include "xr_bt_defs.h"
#include "btmg_common.h"
#include "btmg_log.h"
#include "btmg_a2dp_sink.h"
#include "btmg_a2dp_source.h"
#include "btmg_spp_client.h"
#include "btmg_spp_server.h"
#include "btmg_hfp_hf.h"

__attribute__((weak)) btmg_err bt_hfp_hf_init(void){ return 0; }
__attribute__((weak)) btmg_err bt_hfp_hf_deinit(void){ return 0; }
__attribute__((weak)) btmg_err bt_hfp_hf_disconnect(const char *addr){ return 0; }
__attribute__((weak)) btmg_err bt_hfp_hf_disconnect_audio(const char *addr){ return 0; }
__attribute__((weak)) btmg_err bt_hfp_hf_start_voice_recognition(void){ return 0; }
__attribute__((weak)) btmg_err bt_hfp_hf_stop_voice_recognition(void){ return 0; }
__attribute__((weak)) btmg_err bt_hfp_hf_spk_vol_update(int volume){ return 0; }
__attribute__((weak)) btmg_err bt_hfp_hf_mic_vol_update(int volume){ return 0; }
__attribute__((weak)) btmg_err bt_hfp_hf_dial(const char *number){ return 0; }
__attribute__((weak)) btmg_err bt_hfp_hf_dial_memory(int location){ return 0; }
__attribute__((weak)) btmg_err bt_hfp_hf_send_chld_cmd(btmg_hf_chld_type_t chld, int idx){ return 0; }
__attribute__((weak)) btmg_err bt_hfp_hf_send_btrh_cmd(btmg_hf_btrh_cmd_t btrh){ return 0; }
__attribute__((weak)) btmg_err bt_hfp_hf_answer_call(void){ return 0; }
__attribute__((weak)) btmg_err bt_hfp_hf_reject_call(void){ return 0; }
__attribute__((weak)) btmg_err bt_hfp_hf_query_calls(void){ return 0; }
__attribute__((weak)) btmg_err bt_hfp_hf_query_operator(void){ return 0; }
__attribute__((weak)) btmg_err bt_hfp_hf_query_number(void){ return 0; }
__attribute__((weak)) btmg_err bt_hfp_hf_send_dtmf(char code){ return 0; }
__attribute__((weak)) btmg_err bt_hfp_hf_request_last_voice_tag_number(void){ return 0; }
__attribute__((weak)) btmg_err bt_hfp_hf_send_nrec(void){ return 0; }

__attribute__((weak)) btmg_err bt_hfp_ag_init(void){ return 0; }
__attribute__((weak)) btmg_err bt_hfp_ag_deinit(void){ return 0; }
__attribute__((weak)) btmg_err bt_hfp_ag_connect(const char *addr){ return 0; }
__attribute__((weak)) btmg_err bt_hfp_ag_disconnect(const char *addr){ return 0; }
__attribute__((weak)) btmg_err bt_hfp_ag_connect_audio(const char *addr){ return 0; }
__attribute__((weak)) btmg_err bt_hfp_ag_disconnect_audio(const char *addr){ return 0; }
__attribute__((weak)) btmg_err bt_hfp_ag_spk_vol_update(const char *addr, int volume){ return 0; }
