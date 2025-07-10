#ifndef __RVDEC_MSG_CONFIG_H__
#define __RVDEC_MSG_CONFIG_H__

#define RPMSG_VE_NAME "sunxi,rpmsg_ve"
#define RPMSG_VE_DST_ADDR RPMSG_ADDR_ANY
#define RPMSG_VE_SRC_ADDR RPMSG_ADDR_ANY
#define VE_PACKET_MAGIC 0x9656787

typedef struct ve_packet {
    uint32_t magic;
    uint32_t type;

}ve_packet;

enum ve_packet_type {
	VE_RV_START,
	VE_RV_START_ACK,
	VE_RV_STOP,
	VE_RV_STOP_ACK,
	VE_LINUX_INIT_FINISH,
};

void cmd_dec2ve(void);
void cmd_destroy_rpmsg_ve(void);
int cmd_dec_report(void);

#endif
