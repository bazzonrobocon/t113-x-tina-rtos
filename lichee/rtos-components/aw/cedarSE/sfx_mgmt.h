/*
 * Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
 *
 * Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
 * the the people's Republic of China and other countries.
 * All Allwinner Technology Co.,Ltd. trademarks are used with permission.
 *
 * sfx_mgmt.h -- sfx for mgmt api
 *
 * Dby <dby@allwinnertech.com>
 * huhaoxin <huhaoxin@allwinnertech.com>
 */

#ifndef __SFX_MGMT_H
#define __SFX_MGMT_H

#include <stdbool.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a)	(sizeof(a) / sizeof(a[0]))
#endif

#define SECTIONA(x)	__attribute__((used,section(x)))

#define REGISTER_SFX_MGMT_AP(ap) \
	const audio_plugin_t sfx_mgmt_ap_##ap SECTIONA("sfx_mgmt_ap") = &(ap)

int sfxmgmt_init(void);
void sfxmgmt_exit(void);

#endif /* __SFX_MGMT_H */
