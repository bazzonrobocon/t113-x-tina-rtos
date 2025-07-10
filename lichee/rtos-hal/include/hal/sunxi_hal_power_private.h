/*
 * Copyright (c) 2020 Allwinner Technology Co., Ltd. ALL rights reserved.
 */

#ifndef __SUNXI_HAL_REGULATOR_PRI_H__
#define __SUNXI_HAL_REGULATOR_PRI_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <sunxi_hal_common.h>

int hal_power_byte_read(struct power_dev *rdev, u8 reg, u8 *reg_val);
int hal_power_byte_write(struct power_dev *rdev, u8 reg, u8 reg_val);
int hal_power_byte_update(struct power_dev *rdev, u8 reg, u8 val, u8 mask);
int hal_power_byte_bulk_read(struct power_dev *rdev, u8 reg, void *reg_val, int val_count);

#ifdef __cplusplus
}
#endif

#endif
