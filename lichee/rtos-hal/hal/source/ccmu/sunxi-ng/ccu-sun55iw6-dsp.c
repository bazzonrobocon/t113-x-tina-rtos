// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 Allwinnertech
 */
#include "ccu.h"
#include "ccu_common.h"
#include "ccu_reset.h"
#include "ccu_div.h"
#include "ccu_gate.h"
#include "ccu_mp.h"
#include "ccu_mult.h"
#include "ccu_nk.h"
#include "ccu_nkm.h"
#include "ccu_nkmp.h"
#include "ccu_nm.h"

#include "ccu-sun55iw6-dsp.h"
#include <hal_clk.h>
#include <hal_reset.h>

__attribute__((weak)) int sunxi_rtc_ccu_init(void)
{
	return 0;
}

int sunxi_dsp_init(void)
{
	return 0;
}

