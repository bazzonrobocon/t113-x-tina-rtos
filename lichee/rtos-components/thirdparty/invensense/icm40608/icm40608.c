/**
 * @file ICM40608.c
 * @author
 * @brief
 * @version 0.1
 * @date 2021-12-27
 *
 * @copyright Copyright (c) 2019, Dreame Inc.
 *
 */

#ifndef _ICM40608_C
#define _ICM40608_C

/* Includes ------------------------------------------------------------------*/
#include <hal_log.h>
#include <hal_mem.h>
#include <hal_cmd.h>
#include <sunxi_hal_spi.h>

// #include "imu/imu.h"
#include "icm40608.h"

typedef struct imu_acc_gyro {
    uint16_t x;
    uint16_t y;
    uint16_t z;
} Axis_sTypeDef;

struct icm_device {
    uint8_t WorkMode;
    uint8_t AccConfig0;
    uint8_t GyroConfig0;
    uint8_t AccGyroConfig0;
    uint8_t AccConfig1;
    uint8_t GyroConfig1;
};

struct icm_imu {
    Axis_sTypeDef acc_ori;
    Axis_sTypeDef gyro_ori;
    struct icm_device IcmDeviceVal;
    uint16_t gyro_z;
};

static struct icm_imu imu;

#define ICM40608_SPI_READ   (1)
#define ICM40608_SPI_WRITE  (0)
#define SPI_DEV_IMU_GENERAL (2)

int Icm40608_SpiInit(int port)
{
    hal_spi_master_config_t cfg = { 0 };

    cfg.bus_mode = HAL_SPI_BUS_BIT;
    cfg.cs_mode = HAL_SPI_CS_SOFT;
    cfg.clock_frequency = 24000000;
	cfg.chipselect = 0;
	cfg.mode = SPI_MODE_0 | SPI_3WIRE;

    return hal_spi_init(port, &cfg);
}

int Icm40608_SpiExit(int port)
{
    return hal_spi_deinit(port);
}

void Imu_GeneralReadBuf(int port, uint8_t reg_addr, uint8_t *reg_val, int num)
{
    uint8_t wbuf = reg_addr | (1 << 7);
    hal_spi_master_transfer_t *tr = NULL;
    int i = 0;
    hal_spi_master_status_t ret;

    tr = hal_malloc(sizeof(*tr) * (num + 1));
    memset(tr, 0, sizeof(*tr) * (num + 1));

    tr[0].tx_buf = &wbuf;
    tr[0].tx_len = 1;
    tr[0].rx_buf = NULL;
    tr[0].rx_len = 0;
    tr[0].bits_per_word = 8;

    for (i = 0; i < num; i++)
    {
        tr[i + 1].tx_buf = NULL;
        tr[i + 1].tx_len = 0;
        tr[i + 1].rx_buf = &reg_val[i];
        tr[i + 1].rx_len = 1;
        tr[i + 1].bits_per_word = 8;
    }

    ret = hal_spi_xfer(port, tr, num + 1);
    if (ret < 0)
        hal_log_err("imu spi general read %d failed %d\n", num + 1, ret);

    hal_free(tr);
}

void Imu_GeneralWriteBuf(int port, uint8_t reg_addr, uint8_t *reg_val, int num)
{
    uint8_t wbuf = reg_addr & ~(1 << 7);
    hal_spi_master_transfer_t *tr = NULL;
    int i = 0;
    hal_spi_master_status_t ret;

    tr = hal_malloc(sizeof(*tr) * (num + 1));
    memset(tr, 0, sizeof(*tr) * (num + 1));

    tr[0].tx_buf = &wbuf;
    tr[0].tx_len = 1;
    tr[0].rx_buf = NULL;
    tr[0].rx_len = 0;
    tr[0].bits_per_word = 8;

    for (i = 0; i < num; i++)
    {
        tr[i + 1].tx_buf = &reg_val[i];
        tr[i + 1].tx_len = 1;
        tr[i + 1].rx_buf = NULL;
        tr[i + 1].rx_len = 0;
        tr[i + 1].bits_per_word = 8;
    }

    ret = hal_spi_xfer(port, tr, num + 1);
    if (ret < 0)
        hal_log_err("imu spi general write %d failed %d\n", num + 1, ret);

    hal_free(tr);
}

/**
 * @brief icm40608 读取一个字节
 *
 * @param reg_addr 寄存器地址
 * @return uint8_t 读出的字节
 */
uint8_t Icm40608_ReadByte(uint8_t reg_addr)
{
    uint8_t byte = 0;

    Imu_GeneralReadBuf(SPI_DEV_IMU_GENERAL, reg_addr, &byte, 1);

    return byte;
}

/**
 * @brief icm40608 写入一个字节
 *
 * @param reg_addr 寄存器地址
 * @param reg_val 写入的字节
 */
void Icm40608_WriteByte(uint8_t reg_addr, uint8_t reg_val)
{
    Imu_GeneralWriteBuf(SPI_DEV_IMU_GENERAL, reg_addr, &reg_val, 1);
}

/**
 * @brief icm40608 中断配置 使用gyro中断
 *
 */
void Icm40608_IntConfig(void)
{
    uint8_t data = 0x00;

    /*! config interrupt
     *  1.use INT2,routed to gyro interrupt
     *  2.interrupt polarity: active high
     *  3.interrupt drive circuit: push pull
     *  4.interrupt mdoe: pulse mode(not latched mode)
     */
    data = 0x18;
    Icm40608_WriteByte(ICM40608_REG_INT_CONFIG, data);

    /*! UI data ready interrupt routed to INT2 */
    data = 0x08;
    Icm40608_WriteByte(ICM40608_REG_INT_SOURCE3, data);
}

/**
 * @brief icm40608 init
 *
 * @return uint8_t 0:正常,非0:异常
 */
uint8_t Icm40608_Init(void)
{
    uint8_t data = 0x00, temp = 0x00, ret = 0x00;

    // Enable SPI 3-Wire mode
    data = 0x01;
    Icm40608_WriteByte(ICM40608_MPUREG_REG_BANK_SEL, data);
    data = 0x01;
    Icm40608_WriteByte(ICM40608_MPUREG_INTF_CONFIG4, data);
    data = 0x00;
    Icm40608_WriteByte(ICM40608_MPUREG_REG_BANK_SEL, data);

    //设置角速度计和陀螺仪为低噪声模式
    Icm40608_WriteByte(ICM40608_REG_PWR_MGMT_0, ICM40608_DATA_GYRO_ACC_LNMODE_TEMP_ON);
    // When transitioning from OFF to any of the other modes, do not issue any register writes for 200μs.
    vTaskDelay(100);
    temp = Icm40608_ReadByte(ICM40608_REG_PWR_MGMT_0);
    temp &= 0x2f;     // 0b00101111
    if (temp == 0x0f) // 0b00001111
        imu.IcmDeviceVal.WorkMode = Icm40608_ReadByte(ICM40608_REG_PWR_MGMT_0);
    else
        ret = 1;

    //设置加速度计量程2G/100hz输出
    Icm40608_WriteByte(ICM40608_REG_ACCEL_CONFIG0, ICM40608_DATA_ACC_2G_DOR100HZ);
    temp = Icm40608_ReadByte(ICM40608_REG_ACCEL_CONFIG0);
    temp &= 0xef;     // 0b11101111
    if (temp == 0x68) // 0b01101000
        imu.IcmDeviceVal.AccConfig0 = Icm40608_ReadByte(ICM40608_REG_ACCEL_CONFIG0);
    else
        ret = 1;

    //设置陀螺仪量程1000dps/100hz输出
    Icm40608_WriteByte(ICM40608_REG_GYRO_CONFIG0, ICM40608_DATA_GYRO_1000DPS_DOR100HZ);
    temp = Icm40608_ReadByte(ICM40608_REG_GYRO_CONFIG0);
    temp &= 0xef;     // 0b11101111
    if (temp == 0x28) // 0b00101000
        imu.IcmDeviceVal.GyroConfig0 = Icm40608_ReadByte(ICM40608_REG_GYRO_CONFIG0);
    else
        ret = 1;

    //设置陀螺仪和加速度计低通滤波带宽ODR/4
    Icm40608_WriteByte(ICM40608_REG_GYRO_ACCEL_CONFIG0, ICM40608_DATA_ACC_GYRO_BW_25HZ);
    temp = Icm40608_ReadByte(ICM40608_REG_GYRO_ACCEL_CONFIG0);
    if (temp == ICM40608_DATA_ACC_GYRO_BW_25HZ)
        imu.IcmDeviceVal.AccGyroConfig0 = Icm40608_ReadByte(ICM40608_REG_GYRO_ACCEL_CONFIG0);
    else
        ret = 1;

    //三阶UI-Filter DEC2_M2 Filter
    data = 0x15;
    Icm40608_WriteByte(ICM40608_REG_ACCEL_CONFIG1, data);
    temp = Icm40608_ReadByte(ICM40608_REG_ACCEL_CONFIG1);
    temp &= 0x1e;     // 0b00011110
    if (temp == 0x14) // 0b00010100
        imu.IcmDeviceVal.AccConfig1 = Icm40608_ReadByte(ICM40608_REG_ACCEL_CONFIG1);
    else
        ret = 1;

    data = 0x1a;
    Icm40608_WriteByte(ICM40608_REG_GYRO_CONFIG1, data);
    temp = Icm40608_ReadByte(ICM40608_REG_GYRO_CONFIG1);
    temp &= 0xef;     // 0b11101111
    if (temp == 0x0a) // 0b00001010
        imu.IcmDeviceVal.GyroConfig1 = Icm40608_ReadByte(ICM40608_REG_GYRO_CONFIG1);
    else
        ret = 1;

    // AAF 84hz
    data = 0x02;
    Icm40608_WriteByte(ICM40608_MPUREG_REG_BANK_SEL, data);
    data = 0x04;
    Icm40608_WriteByte(ICM40608_MPUREG_ACCEL_CONFIG_STATIC2_B2, data);
    data = 0x04;
    Icm40608_WriteByte(ICM40608_MPUREG_ACCEL_CONFIG_STATIC3_B2, data);
    data = 0xD0;
    Icm40608_WriteByte(ICM40608_MPUREG_ACCEL_CONFIG_STATIC4_B2, data);
    data = 0x01;
    Icm40608_WriteByte(ICM40608_MPUREG_REG_BANK_SEL, data);
    data = 0x02;
    Icm40608_WriteByte(ICM40608_MPUREG_GYRO_CONFIG_STATIC3_B1, data);
    data = 0x04;
    Icm40608_WriteByte(ICM40608_MPUREG_GYRO_CONFIG_STATIC4_B1, data);
    data = 0xD0;
    Icm40608_WriteByte(ICM40608_MPUREG_GYRO_CONFIG_STATIC5_B1, data);
    data = 0x00;
    Icm40608_WriteByte(ICM40608_MPUREG_REG_BANK_SEL, data);

    // Gyroscope needs to be kept ON for a minimum of 45ms
    vTaskDelay(100);

    return ret;
}

/**
 * @brief icm40608 配置检查
 *
 * @return uint8_t 0:正常,非0:异常
 */
uint8_t Icm40608_Check(void)
{
    uint8_t temp = 0;
    // id
    temp = Icm40608_ReadByte(ICM_REG_WHO_AM_I);
    if (temp != ICM40608_DATA_WHO_AM_I)
        return 1;
    // config
    temp = Icm40608_ReadByte(ICM40608_REG_ACCEL_CONFIG0);
    if (temp != imu.IcmDeviceVal.AccConfig0)
        return 2;
    temp = Icm40608_ReadByte(ICM40608_REG_GYRO_CONFIG0);
    if (temp != imu.IcmDeviceVal.GyroConfig0)
        return 2;
    temp = Icm40608_ReadByte(ICM40608_REG_GYRO_ACCEL_CONFIG0);
    if (temp != imu.IcmDeviceVal.AccGyroConfig0)
        return 2;
    temp = Icm40608_ReadByte(ICM40608_REG_ACCEL_CONFIG1);
    if (temp != imu.IcmDeviceVal.AccConfig1)
        return 2;
    temp = Icm40608_ReadByte(ICM40608_REG_GYRO_CONFIG1);
    if (temp != imu.IcmDeviceVal.GyroConfig1)
        return 2;
    // work mode
    temp = Icm40608_ReadByte(ICM40608_REG_PWR_MGMT_0);
    if (temp != imu.IcmDeviceVal.WorkMode)
        return 3;

    return 0;
}

/**
 * @brief icm40608 温度获取
 *
 * @return int16_t 温度值
 */
int16_t Icm40608_GetTemp(void)
{
    uint8_t bufer[2];
    int16_t temp_ori = 0, temp = 0;

    Imu_GeneralReadBuf(SPI_DEV_IMU_GENERAL, ICM40608_REG_TEMP_OUT_H, bufer, 2);

    temp_ori = (int16_t)(((uint16_t)bufer[0] << 8) | bufer[1]);

    temp = (int16_t)((temp_ori * 10 / 132) + 250);

    return temp;
}

/**
 * @brief icm40608 原始数据获取
 *
 */
void Icm40608_GetData(void)
{
    Axis_sTypeDef acce, gyro;
    uint8_t bufer[12];

    Imu_GeneralReadBuf(SPI_DEV_IMU_GENERAL, ICM40608_REG_ACCEL_XOUT_1, bufer, 12);

    acce.x = (int16_t)(((uint16_t)bufer[0] << 8) | bufer[1]);
    acce.y = (int16_t)(((uint16_t)bufer[2] << 8) | bufer[3]);
    acce.z = (int16_t)(((uint16_t)bufer[4] << 8) | bufer[5]);
    gyro.x = (int16_t)(((uint16_t)bufer[6] << 8) | bufer[7]);
    gyro.y = (int16_t)(((uint16_t)bufer[8] << 8) | bufer[9]);
    gyro.z = (int16_t)(((uint16_t)bufer[10] << 8) | bufer[11]);

#if (0 == IMU_ICM40608_ROTATE_ANGLE)
    /*!< nothing need to do */
#elif (90 == IMU_ICM40608_ROTATE_ANGLE)
    Vector_rotate(ROTATION_YAW_90, &acce.x, &acce.y, &acce.z);
    Vector_rotate(ROTATION_YAW_90, &gyro.x, &gyro.y, &gyro.z);
#elif (180 == IMU_ICM40608_ROTATE_ANGLE)
    Vector_rotate(ROTATION_YAW_180, &acce.x, &acce.y, &acce.z);
    Vector_rotate(ROTATION_YAW_180, &gyro.x, &gyro.y, &gyro.z);
#elif (270 == IMU_ICM40608_ROTATE_ANGLE)
    Vector_rotate(ROTATION_YAW_270, &acce.x, &acce.y, &acce.z);
    Vector_rotate(ROTATION_YAW_270, &gyro.x, &gyro.y, &gyro.z);
#endif

    imu.acc_ori.x = acce.x;
    imu.acc_ori.y = acce.y;
    imu.acc_ori.z = acce.z;

    imu.gyro_ori.x = gyro.x;
    imu.gyro_ori.y = gyro.y;
    imu.gyro_ori.z = gyro.z;

    imu.gyro_z = imu.gyro_ori.z;
}

static int cmd_icm40608_test(int argc, char **argv)
{
    int ret;
    uint16_t temp;

    ret = Icm40608_SpiInit(SPI_DEV_IMU_GENERAL);
    if (ret < 0)
    {
        hal_log_err("icm40608 spi init failed %d\n", ret);
        return -1;
    }

    ret = Icm40608_Init();
    if (ret)
    {
        hal_log_err("icm40608 init failed %d\n", ret);
        goto exit;
    }

    Icm40608_IntConfig();

    ret = Icm40608_Check();
    if (ret)
    {
        hal_log_err("icm40608 check failed %d\n", ret);
        goto exit;
    }

    temp = Icm40608_GetTemp();
    hal_log_info("icm40608 get temperature %d\n", temp);

    Icm40608_GetData();
    hal_log_info("icm40608 get acc x=%d y=%d z=%d\n", imu.acc_ori.x, imu.acc_ori.y, imu.acc_ori.z);
    hal_log_info("icm40608 get gyro x=%d y=%d z=%d\n", imu.gyro_ori.x, imu.gyro_ori.y, imu.gyro_ori.z);

exit:
    ret = Icm40608_SpiExit(SPI_DEV_IMU_GENERAL);
    if (ret)
    {
        hal_log_err("icm40608 spi exit failed %d\n", ret);
        return -1;
    }

    return 0;
}

FINSH_FUNCTION_EXPORT_CMD(cmd_icm40608_test, hal_icm40608_test, icm40608 hal APIs tests)

static int cmd_icm40608_dump(int argc, char **argv)
{
    int bank = atoi(argv[1]);
    int reg = strtol(argv[2], NULL, 16);
    int data;
    int ret;

    ret = Icm40608_SpiInit(SPI_DEV_IMU_GENERAL);
    if (ret < 0)
    {
        hal_log_err("icm40608 spi init failed %d\n", ret);
        return -1;
    }

    Icm40608_WriteByte(ICM40608_MPUREG_REG_BANK_SEL, bank);
    data = Icm40608_ReadByte(reg);
    hal_log_info("icm40608 dump bank %d reg %#x data %#x\n", bank, reg, data);

    ret = Icm40608_SpiExit(SPI_DEV_IMU_GENERAL);
    if (ret)
    {
        hal_log_err("icm40608 spi exit failed %d\n", ret);
        return -1;
    }

    return 0;
}

FINSH_FUNCTION_EXPORT_CMD(cmd_icm40608_dump, hal_icm40608_dump, icm40608 hal APIs tests)

static int cmd_icm40608_set(int argc, char **argv)
{
    int bank = atoi(argv[1]);
    int reg = strtol(argv[2], NULL, 16);
    int data = strtol(argv[3], NULL, 16);
    int ret;

    ret = Icm40608_SpiInit(SPI_DEV_IMU_GENERAL);
    if (ret < 0)
    {
        hal_log_err("icm40608 spi init failed %d\n", ret);
        return -1;
    }

    Icm40608_WriteByte(ICM40608_MPUREG_REG_BANK_SEL, bank);
    Icm40608_WriteByte(reg, data);
    hal_log_info("icm40608 set bank %d reg %#x data %#x\n", bank, reg, data);

    ret = Icm40608_SpiExit(SPI_DEV_IMU_GENERAL);
    if (ret)
    {
        hal_log_err("icm40608 spi exit failed %d\n", ret);
        return -1;
    }

    return 0;
}

FINSH_FUNCTION_EXPORT_CMD(cmd_icm40608_set, hal_icm40608_set, icm40608 hal APIs tests)

#endif
