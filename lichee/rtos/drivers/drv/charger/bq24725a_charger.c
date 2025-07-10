#include <stdio.h>
#include <console.h>
#include <hal_workqueue.h>
#include <sunxi_hal_twi.h>
#include <sunxi_hal_gpadc_ng.h>

//#define BQ_DEBUG
#ifdef BQ_DEBUG
#define BQ_DBG(fmt, arg...) printf("[BQ24725A DBG] "fmt, ##arg)
#else
#define BQ_DBG(fmt, arg...)
#endif

#define BQ_ERR(fmt, arg...) printf("[BQ24725A ERR] "fmt, ##arg)

#define CHARGE_OPTION   0x12
#define CHARGE_CURRENT  0x14
#define CHARGE_VOLTAGE  0x15
#define INPUT_CURRENT   0x3f
#define MANUFACTURER_ID 0xfe
#define DEVICE_ID       0xff

struct bq24725a_pdata {
	int port;
	int addr;
	int int_io;
	int irq_no;
	int adc_port;
	int charge_adc_ch; /* use to get charge current */
	int current_type;  /* set current type, ac current or charge current */
	int bat_adc_ch;    /* use to get battery's voltage */
	int adc_port0;
	int ntc_adc_ch;	   /* use to get battery temperature */
	hal_workqueue *init_workqueue;
	hal_work init_workqueue_task;
	hal_workqueue *ntc_workqueue;
	hal_work ntc_workqueue_task;
};
struct bq24725a_pdata *gdata;

static int i2c_init(struct bq24725a_pdata *pdata)
{
	int port = pdata->port;

	hal_twi_init(port);
	return 0;
}

static int i2c_byte_read(struct bq24725a_pdata *pdata, u8 reg, u8 *reg_val)
{
	int port = pdata->port;
	int addr = pdata->addr;
	int ret = 0;

	ret = hal_twi_read(port, I2C_SLAVE, addr, reg, reg_val, 1);
	if (ret < 0) {
		BQ_ERR("i2c byte read err, ret=%d\n", ret);
		return ret;
	}
	return 0;
}

/*
static int i2c_byte_write(struct bq24725a_pdata *pdata, u8 reg, u8 *reg_val)
{
	int port = pdata->port;
	int addr = pdata->addr;
	twi_msg_t msg;
	uint8_t buf[2];

	buf[0] = reg;
	buf[1] = reg_val;

	msg.flags = 0;
	msg.addr = addr;
	msg.len = 2;
	msg.buf = buf;

	return hal_twi_write(port, &msg, 1);
}*/

static int i2c_bulk_read(struct bq24725a_pdata *pdata, u8 reg, u8 *reg_val)
{
	int port = pdata->port;
	int addr = pdata->addr;
	int ret = 0;

	ret = hal_twi_read(port, I2C_SLAVE, addr, reg, reg_val, 2);
	if (ret < 0) {
		BQ_ERR("i2c bulk read err, ret=%d\n", ret);
		return ret;
	}
	return 0;
}

static int i2c_bulk_write(struct bq24725a_pdata *pdata, u8 reg, u8 h_val, u8 l_val)
{
	int port = pdata->port;
	int addr = pdata->addr;
	twi_msg_t msg;
	uint8_t buf[3];

	buf[0] = reg;
	buf[1] = l_val;
	buf[2] = h_val;
	msg.flags = 0;
	msg.addr = addr;
	msg.len = 3;
	msg.buf = buf;

	return hal_twi_write(port, &msg, 1);
}

static int bq24725a_check_id(struct bq24725a_pdata *pdata)
{
	int ret = 0;
	u8 reg_val = 0;

	ret = i2c_byte_read(pdata, MANUFACTURER_ID, &reg_val);
	if (!ret) {
		if (reg_val != 0x40) {
			BQ_ERR("check ManufacturerID failed\n");
			return -1;
		}
		BQ_DBG("get ManufacturerID: 0x%x\n", reg_val);
	} else {
		return -1;
	}

	ret = i2c_byte_read(pdata, DEVICE_ID , &reg_val);
	if (!ret) {
		if (reg_val != 0x0b) {
			BQ_ERR("check DeviceID failed\n");
			return -1;
		}
		BQ_DBG("get DeviceID: 0x%x\n", reg_val);
	} else {
		return -1;
	}

	return 0;
}

static void bq24725a_dump_reg(struct bq24725a_pdata *pdata)
{
	u8 reg_val[2];

	BQ_DBG("Dump reg:\n");
	i2c_bulk_read(pdata, CHARGE_OPTION, reg_val);
	BQ_DBG("0x%x: 0x%02x%02x\n", CHARGE_OPTION, reg_val[1], reg_val[0]);
	i2c_bulk_read(pdata, CHARGE_CURRENT, reg_val);
	BQ_DBG("0x%x: 0x%02x%02x\n", CHARGE_CURRENT, reg_val[1], reg_val[0]);
	i2c_bulk_read(pdata, CHARGE_VOLTAGE, reg_val);
	BQ_DBG("0x%x: 0x%02x%02x\n", CHARGE_VOLTAGE, reg_val[1], reg_val[0]);
	i2c_bulk_read(pdata, INPUT_CURRENT, reg_val);
	BQ_DBG("0x%x: 0x%02x%02x\n", INPUT_CURRENT, reg_val[1], reg_val[0]);
}

static int bq24725a_register_init(struct bq24725a_pdata *pdata)
{
	int ret = 0;
	u8 reg_val[2];

	bq24725a_check_id(pdata);

	//0x9912 ac current, 0x9932 charge current
	reg_val[0] = 0x32;
	reg_val[1] = 0x99;
	ret = i2c_bulk_write(pdata, CHARGE_OPTION, reg_val[1], reg_val[0]);
	if (ret < 0) {
		BQ_ERR("i2c write 0x%x failed\n", CHARGE_OPTION);
		return -1;
	}

	//charge current 1536mA, 0x04 1A 0x06 1.5A
	reg_val[0] = 0x00;
	reg_val[1] = 0x04;
	ret = i2c_bulk_write(pdata, CHARGE_CURRENT, reg_val[1], reg_val[0]);
	if (ret < 0) {
		BQ_ERR("i2c write 0x%x failed\n", CHARGE_CURRENT);
		return -1;
	}

	//charge voltage 16384+256mA
	reg_val[0] = 0x00;
	reg_val[1] = 0x41;
	ret = i2c_bulk_write(pdata, CHARGE_VOLTAGE, reg_val[1], reg_val[0]);
	if (ret < 0) {
		BQ_ERR("i2c write 0x%x failed\n", CHARGE_VOLTAGE);
		return -1;
	}

	//input current limit 2A
	reg_val[0] = 0x00;
	reg_val[1] = 0x10;
	ret = i2c_bulk_write(pdata, INPUT_CURRENT, reg_val[1], reg_val[0]);
	if (ret < 0) {
		BQ_ERR("i2c write 0x%x failed\n", INPUT_CURRENT);
		return -1;
	}

	bq24725a_dump_reg(pdata);

	hal_sleep(1);

	return 0;
}

static int bq24725a_get_para(struct bq24725a_pdata *pdata)
{
	pdata->port          = 0;
	pdata->addr          = 0x09;
	pdata->int_io        = GPIOH(13);
	pdata->adc_port      = 1;
	pdata->adc_port0     = 0;
	pdata->charge_adc_ch = 1;  /* gpadc 1 */
	pdata->current_type  = 0;
	pdata->bat_adc_ch    = 10; /* gpadc 1 */
	pdata->ntc_adc_ch    = 4;  /* gpadc 0 */
	return 0;
}

static void __read_current(struct bq24725a_pdata *pdata, int type)
{
	int data = 0, ret = 0;
	u8 reg_val[2];

	if (pdata->current_type != type) {
		//0x9912 ac current, 0x9932 charge current
		if (type == 0)
			reg_val[0] = 0x32;
		else if (type == 1)
			reg_val[0] = 0x12;
		reg_val[1] = 0x99;
		ret = i2c_bulk_write(pdata, CHARGE_OPTION, reg_val[1], reg_val[0]);
		if (ret < 0) {
			BQ_ERR("i2c write 0x%x failed\n", CHARGE_OPTION);
			return;
		}
	}

	data = gpadc_read_channel_data(pdata->adc_port, pdata->charge_adc_ch);
	BQ_DBG("gpadc%d ch:%d data is %d\n", pdata->adc_port, pdata->charge_adc_ch, data);

	data = data * 100 / 20;
	printf("[BQ24725A]:%s current is %dmA\n", type ? "ac" : "charge", data);
}

static void __set_current(struct bq24725a_pdata *pdata, uint32_t current)
{
	u8 reg_val[2];
	int ret = 0;

	if (current > 4096)
		current = 4096;

	current /= 64;

	printf("[BQ24725A]:set charge current to %d\n", current * 64);

	current = current << 6;

	reg_val[0] = current & 0xff;
	reg_val[1] = (current >> 8) & 0xff;
	ret = i2c_bulk_write(pdata, CHARGE_CURRENT, reg_val[1], reg_val[0]);
	if (ret < 0) {
		BQ_ERR("i2c write 0x%x failed\n", CHARGE_CURRENT);
		return;
	}
}

static void __read_capacity(struct bq24725a_pdata *pdata)
{
	int data = 0;

	data = gpadc_read_channel_data(pdata->adc_port, pdata->bat_adc_ch);

	data = data * 1231 / 1000;

	BQ_DBG("gpadc%d ch:%d data is %d\n", pdata->adc_port, pdata->bat_adc_ch, data);

        if (data < 1300)
                data = 0;
        else if (data > 1300 && data < 1336)
                data = 10;
        else if (data > 1336 && data < 1372)
                data = 20;
        else if (data > 1372 && data < 1408)
                data = 30;
        else if (data > 1408 && data < 1444)
                data = 40;
        else if (data > 1444 && data < 1480)
                data = 50;
        else if (data > 1480 && data < 1516)
                data = 60;
        else if (data > 1516 && data < 1552)
                data = 70;
        else if (data > 1552 && data < 1588)
                data = 80;
        else if (data > 1588 && data < 1650)
                data = 90;
        else
                data = 100;

	printf("[BQ24725A]:bat capacity is %d%%\n", data);
}

static int read_ntc(struct bq24725a_pdata *pdata)
{
	int data = 0;

	data = gpadc_read_channel_data(pdata->adc_port0, pdata->ntc_adc_ch);

	return data;
}

static void __read_ntc(struct bq24725a_pdata *pdata)
{
	int data = 0;

	data = read_ntc(pdata);

	printf("[BQ24725A]:bat temp adc is %d\n", data);
}

static void __init_work_func(hal_work *work, void *work_data)
{
	struct bq24725a_pdata *pdata = (struct bq24725a_pdata*)work_data;

	BQ_DBG("-----%s-------\n", __func__);

	bq24725a_register_init(pdata);

	__read_capacity(pdata);
	__read_current(pdata, 0);
	__read_ntc(gdata);
}

static void __ntc_work_func(hal_work *work, void *work_data)
{
	struct bq24725a_pdata *pdata = (struct bq24725a_pdata*)work_data;
	int data = 0, ret = 0, is_stop = 0;
	u8 reg_val[2];

	BQ_DBG("-----%s-------\n", __func__);

	while (1) {
		data = read_ntc(gdata);
		if (data < 220) {
			is_stop = 1;
			printf("[BQ24725A]: battery temp exceedes 60,"
					" stop charging\n");
			//charge current 0mA
			reg_val[0] = 0x00;
			reg_val[1] = 0x00;
			ret = i2c_bulk_write(pdata, CHARGE_CURRENT, reg_val[1], reg_val[0]);
			if (ret < 0) {
				BQ_ERR("i2c write 0x%x failed\n", CHARGE_CURRENT);
			}
		} else if (data > 220 && is_stop == 1) {
			//charge current 1536mA, 0x04 1A 0x06 1.5A
			reg_val[0] = 0x00;
			reg_val[1] = 0x04;
			ret = i2c_bulk_write(pdata, CHARGE_CURRENT, reg_val[1], reg_val[0]);
			if (ret < 0) {
				BQ_ERR("i2c write 0x%x failed\n", CHARGE_CURRENT);
			}
		}
		hal_sleep(1);
	}
}

static hal_irqreturn_t bq24725a_handler(void *dev)
{
	struct bq24725a_pdata *pdata = (struct bq24725a_pdata*)dev;
	gpio_data_t data;


	hal_gpio_get_data(pdata->int_io, &data);

	BQ_DBG("-----%s-------data:%d\n", __func__, data);

	if (data)
		hal_workqueue_dowork(pdata->init_workqueue,
			&pdata->init_workqueue_task);

	return HAL_IRQ_OK;
}

int bq24725a_init(void)
{
	struct bq24725a_pdata *pdata;
	gpio_data_t data;

	pdata = malloc(sizeof(struct bq24725a_pdata));
	if (!pdata) {
		BQ_ERR("malloc failed\n");
		return -1;
	}

	bq24725a_get_para(pdata);

	/* enable i2c */
	i2c_init(pdata);

	/* enable gpio irq */
	if (hal_gpio_to_irq(pdata->int_io, (uint32_t *)&pdata->irq_no) < 0) {
		BQ_ERR("get io irq failed\n");
		return -1;
	}
	if (hal_gpio_irq_request(pdata->irq_no, bq24725a_handler,
				IRQ_TYPE_EDGE_BOTH, pdata) < 0) {
		BQ_ERR("request irq failed\n");
		return -1;
	}
	hal_gpio_irq_enable(pdata->irq_no);

	/* enable gpadc1 */
	if (hal_gpadc_init(pdata->adc_port)) {
		BQ_ERR("init gpadc%d failed\n", pdata->adc_port);
		return -1;
	}
	if (hal_gpadc_channel_init(pdata->adc_port, pdata->charge_adc_ch, 0)) {
		BQ_ERR("init gpadc%d channel%d failed\n",
				pdata->adc_port, pdata->charge_adc_ch);
		return -1;
	}
	if (hal_gpadc_channel_init(pdata->adc_port, pdata->bat_adc_ch, 0)) {
		BQ_ERR("init gpadc%d channel%d failed\n",
				pdata->adc_port, pdata->bat_adc_ch);
		return -1;
	}

	/* enable gpadc0 */
	if (hal_gpadc_init(pdata->adc_port0)) {
		BQ_ERR("init gpadc%d failed\n", pdata->adc_port0);
		return -1;
	}
	if (hal_gpadc_channel_init(pdata->adc_port0, pdata->ntc_adc_ch, 0)) {
		BQ_ERR("init gpadc%d channel%d failed\n",
				pdata->adc_port0, pdata->ntc_adc_ch);
		return -1;
	}

	/* enable init queue */
	hal_work_init(&pdata->init_workqueue_task, __init_work_func, pdata);
	pdata->init_workqueue = hal_workqueue_create("bq24725a_workqueue",
			256, HAL_THREAD_PRIORITY_SYS);
	if (!(pdata->init_workqueue)) {
		BQ_ERR("request init workqueue failed\n");
		return -1;
	}

	/* enable loop ntc queue */
	hal_work_init(&pdata->ntc_workqueue_task, __ntc_work_func, pdata);
	pdata->ntc_workqueue = hal_workqueue_create("bq24725a_ntc",
			256, HAL_THREAD_PRIORITY_SYS);
	if (!(pdata->ntc_workqueue)) {
		BQ_ERR("request ntc workqueue failed\n");
		return -1;
	}
	hal_workqueue_dowork(pdata->ntc_workqueue, &pdata->ntc_workqueue_task);

	gdata = pdata;

	hal_gpio_get_data(pdata->int_io, &data);
	if (data)
		hal_workqueue_dowork(pdata->init_workqueue,
			&pdata->init_workqueue_task);

	return 0;
}

static int cmd_bq24725a_init(int argc, const char **argv)
{
	bq24725a_init();
	return 0;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_bq24725a_init, bq24725a_init, bq24725a_init)

static int cmd_get_bq24725a_current(int argc, const char **argv)
{
	__read_capacity(gdata);
	__read_current(gdata, 1);
	__read_current(gdata, 0);
	__read_ntc(gdata);
	return 0;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_get_bq24725a_current, get_bq24725a_info, get_bq24725a_info)

static int cmd_set_bq24725a_current(int argc, const char **argv)
{
	uint32_t current;

	if (argc != 2) {
		printf("Error parameter\n");
		return -1;
	}
	current = strtol(argv[1], NULL, 0);

	__set_current(gdata, (uint32_t)current);
	return 0;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_set_bq24725a_current, set_bq24725a_cur, set_bq24725a_cur)
