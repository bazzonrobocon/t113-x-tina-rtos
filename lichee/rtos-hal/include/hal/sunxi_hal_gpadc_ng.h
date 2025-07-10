/*
 * drivers/input/sensor/sunxi_gpadc.h
 *
 * Copyright (C) 2016 Allwinner.
 * fuzhaoke <fuzhaoke@allwinnertech.com>
 *
 * SUNXI GPADC Controller Driver Header
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#ifndef HAL_GPADC_NG_H
#define HAL_GPADC_NG_H

#include <hal_clk.h>
#include <hal_reset.h>
#include "sunxi_hal_common.h"
#include <hal_log.h>
#include <hal_mutex.h>
#include <interrupt.h>
#include <gpadc-ng/platform_gpadc.h>
#include <gpadc-ng/common_gpadc.h>

#ifdef __cplusplus
extern "C" {
#endif

/* define this macro when debugging is required */
/* #define CONFIG_DRIVERS_GPADC_DEBUG */
#ifdef CONFIG_DRIVERS_GPADC_DEBUG
#define GPADC_INFO(fmt, arg...) hal_log_info(fmt, ##arg)
#else
#define GPADC_INFO(fmt, arg...) do {}while(0)
#endif

#define GPADC_ERR(fmt, arg...) hal_log_err(fmt, ##arg)

enum
{
    GPADC_DOWN,
    GPADC_UP
};

typedef enum
{
    GP_CH_0,
    GP_CH_1,
    GP_CH_2,
    GP_CH_3,
    GP_CH_4,
    GP_CH_5,
    GP_CH_6,
    GP_CH_7,
    GP_CH_8,
    GP_CH_9,
    GP_CH_A,
    GP_CH_B,
    GP_CH_C,
    GP_CH_D,
    GP_CH_E,
    GP_CH_F,_
} hal_gpadc_channel_t;

typedef enum
{
    GPADC_IRQ_ERROR = -4,
    GPADC_CHANNEL_ERROR = -3,
    GPADC_CLK_ERROR = -2,
    GPADC_ERROR = -1,
    GPADC_OK = 0,
} hal_gpadc_status_t;

typedef enum gp_select_mode
{
    GP_SINGLE_MODE = 0,
    GP_SINGLE_CYCLE_MODE,
    GP_CONTINUOUS_MODE,
    GP_BURST_MODE,
} hal_gpadc_mode_t;

typedef int (*gpadc_callback_t)(uint32_t data_type, uint32_t data);

static uint32_t hal_gpadc_regs_offset[] = {
    GP_SR_REG,
    GP_CTRL_REG,
    GP_CS_EN_REG,
    GP_FIFO_INTC_REG,
    GP_FIFO_DATA_REG,
    GP_CB_DATA_REG,
    GP_DATAL_INTC_REG,
    GP_DATAH_INTC_REG,
    GP_DATA_INTC_REG,
    GP_CH0_CMP_DATA_REG,
    GP_CH1_CMP_DATA_REG,
    GP_CH2_CMP_DATA_REG,
    GP_CH3_CMP_DATA_REG,
    GP_CH4_CMP_DATA_REG,
    GP_CH5_CMP_DATA_REG,
    GP_CH6_CMP_DATA_REG,
    GP_CH7_CMP_DATA_REG,
    GP_CH8_CMP_DATA_REG,
    GP_CH9_CMP_DATA_REG,
    GP_CHA_CMP_DATA_REG,
    GP_CHB_CMP_DATA_REG,
    GP_CHC_CMP_DATA_REG,
    GP_CHD_CMP_DATA_REG,
    GP_CHE_CMP_DATA_REG,
};

typedef struct
{
    uint32_t already_init;
    uint32_t reg_base;
    uint32_t channel_num;
    uint32_t irq_num;
    uint32_t sample_rate;
    uint32_t port;
    struct reset_control *reset;
    hal_clk_id_t bus_clk;
    hal_clk_id_t rst_clk;
    hal_clk_id_t mod_clk;
    hal_clk_t mbus_clk;
    hal_clk_t clk;
    hal_gpadc_mode_t mode;
    gpadc_callback_t callback[CHANNEL_MAX_NUM];
    uint32_t regs_backup[ARRAY_SIZE(hal_gpadc_regs_offset)];
    hal_mutex_t lock;
} hal_gpadc_t;

int hal_gpadc_init(int port);
hal_gpadc_status_t hal_gpadc_deinit(int port);
hal_gpadc_status_t hal_gpadc_channel_init(int port, hal_gpadc_channel_t channal, bool keyadc);
hal_gpadc_status_t hal_gpadc_channel_exit(int port, hal_gpadc_channel_t channal, bool keyadc);
uint32_t gpadc_read_channel_data(int port, hal_gpadc_channel_t channal);
hal_gpadc_status_t hal_gpadc_register_callback(int port, hal_gpadc_channel_t channal,
        gpadc_callback_t user_callback);
void gpadc_channel_enable_highirq(int port, hal_gpadc_channel_t channal);
void gpadc_channel_disable_highirq(int port, hal_gpadc_channel_t channal);
void hal_gpadc_fifo_config(int port);

#ifdef __cplusplus
}
#endif

#endif
