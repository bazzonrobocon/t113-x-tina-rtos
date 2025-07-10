/* all about esmt factory */

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <hal_timer.h>

#include "inter.h"

static struct nor_info idt_esmt[] =
{
    {
        .model = "en25qh128",
        .id = {0x1c, 0x70, 0x18},
        .total_size = SZ_16M,
        .flag = SUPPORT_ALL_ERASE_BLK,
    },
    {
        .model = "pn25f64b",
        .id = {0x1c, 0x70, 0x17},
        .total_size = SZ_8M,
        .flag = SUPPORT_ALL_ERASE_BLK,
    },
    {
        .model = "en25qh128A",
        .id = {0x1c, 0x71, 0x18},
        .total_size = SZ_16M,
        .flag = SUPPORT_ALL_ERASE_BLK,
    },
};

int esmt_read_uuid(struct nor_flash *nor,void *uuid)//EN25QH128A获取uuid
{
	int ret;
	char cmd[4] = {0x5A,0x00,0x00,0x80};
	if (nor_wait_ready(0, 500)) {
		SPINOR_ERR("nor is busy before read id");
		return -EBUSY;
	}
	ret = nor_transfer(4, cmd, 4, uuid, 12);
	if (ret)
		SPINOR_ERR("read uuid failed - %d", ret);
	return ret;
}

static struct nor_factory nor_esmt = {
    .factory = FACTORY_ESMT,
    .idt = idt_esmt,
    .idt_cnt = sizeof(idt_esmt),

    .init = NULL,
    .deinit = NULL,
    .init_lock = NULL,
    .deinit_lock = NULL,
    .lock = NULL,
    .unlock = NULL,
    .islock = NULL,
    .set_quad_mode = NULL,
    .set_4bytes_addr = NULL,
    .read_uuid = esmt_read_uuid,
};

int nor_register_factory_esmt(void)
{
    return nor_register_factory(&nor_esmt);
}
