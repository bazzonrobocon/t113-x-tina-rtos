#ifndef __SUNXI_HAL_TIMESTAMP_H__
#define __SUNXI_HAL_TIMESTAMP_H__

#include <stdint.h>
#include <stdio.h>
#include <interrupt.h>

#ifdef __cplusplus
	extern "C" {
#endif


u64 hal_timestamp_read(void);
void hal_timestamp_init(void);
void hal_timestamp_reset(void);
void hal_timestamp_unint(void);
void hal_timestamp_printf_runtime(void);
#ifdef __cplusplus
}
#endif

#endif
