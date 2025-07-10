/* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.

 * Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
 * the the People's Republic of China and other countries.
 * All Allwinner Technology Co.,Ltd. trademarks are used with permission.

 * DISCLAIMER
 * THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
 * IF YOU NEED TO INTEGRATE THIRD PARTY¡¯S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
 * IN ALLWINNERS¡¯SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
 * ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
 * ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
 * COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
 * YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY¡¯S TECHNOLOGY.


 * THIS SOFTWARE IS PROVIDED BY ALLWINNER"AS IS" AND TO THE MAXIMUM EXTENT
 * PERMITTED BY LAW, ALLWINNER EXPRESSLY DISCLAIMS ALL WARRANTIES OF ANY KIND,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING WITHOUT LIMITATION REGARDING
 * THE TITLE, NON-INFRINGEMENT, ACCURACY, CONDITION, COMPLETENESS, PERFORMANCE
 * OR MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 * IN NO EVENT SHALL ALLWINNER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS, OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <sunxi_hal_pwm_ng.h>
#include <sunxi_hal_gpadc_ng.h>
#include <sunxi_hal_htimer.h>
#include <hal_gpio.h>
#include <hal_cmd.h>
#include <hal_sem.h>
#include <hal_time.h>
#include <hal_thread.h>
#include <hal_queue.h>

#define TIMER_TICK_US (1 * 1000)
#define PWM_PERIOD 36000
#define PWM_POSITIVE 25000
#define PWM_REVERSE 11000
#define PWM_QUARTER 9000
#define PWM_EIGHTHT 4500
#define PWM_SIXTEENTH 2250
#define PWM_STOP 18000
#define GPIO_COL_MR GPIO_PH15
#define GPIO_COL_ML GPIO_PH16
#define GPIO_TEST GPIO_PB0
#define GPIO_ADC_DOWNVIEW_ENABLE GPIO_PD18

#define ADC_DOWNVIEW_THRESHOLD_MV 400
#define ADC_DOWNVIEW_THRESHOLD_MV 400
#define ADC_DOWNVIEW_ML		3
#define ADC_BAT_TEMP		4
#define ADC_FRONTVIEW_MR	5
#define ADC_VIEW_SIDE_L		6
#define ADC_VIEW_SIDE_R		7
#define ADC_DOWNVIEW_MR		8
#define ADC_FRONTVIEW_ML	9
#define ADC_FRONTVIEW_M		10
#define ADC_MOTOR_L			11
#define ADC_ROLLER_I		12
#define ADC_BQ_IOUT			13
#define ADC_DOWNVIEW_L		14
#define ADC_DOWNVIEW_R		15
#define ADC_MOTOR_R			16
#define ADC_FAN_I			17
#define ADC_SIDEBRUSH		18
#define ADC_COMSUME_L		19
#define ADC_FRONTVIEW_R		20
#define ADC_FRONTVIEW_L		21
#define ADC_BAT_VOLTAGE		22
#define ADC_PUMP_I			23

#define EVENT_QUEUE_SIZE	32

static uint32_t adc_downview_ml_vol;
//static uint32_t adc_bat_temp_vol;
//static uint32_t adc_frontview_mr_vol;
//static uint32_t adc_view_side_l_vol;
//static uint32_t adc_view_side_r_vol;
static uint32_t adc_downview_mr_vol;
//static uint32_t adc_frontview_ml_vol;
//static uint32_t adc_frontview_m_vol;
static uint32_t adc_motor_l_vol;
static uint32_t adc_roller_i_vol;
//static uint32_t adc_bq_iout_vol;
static uint32_t adc_downview_l_vol;
static uint32_t adc_downview_r_vol;
static uint32_t adc_motor_r_vol;
static uint32_t adc_fan_i_vol;
static uint32_t adc_sidebrush_vol;
//static uint32_t adc_comsume_vol;
//static uint32_t adc_frontview_r_vol;
//static uint32_t adc_frontview_l_vol;
//static uint32_t adc_bat_voltage_vol;
//static uint32_t adc_pump_i_vol;

enum robot_event {
	TRIGGER_RUN = 0,
	TRIGGER_DOWNVIEW_ML,
	TRIGGER_DOWNVIEW_MR,
	TRIGGER_DOWNVIEW_L,
	TRIGGER_DOWNVIEW_R,
	TRIGGER_COL_ML,
	TRIGGER_COL_MR,
};

struct timer_handle
{
	int timer;
	uint32_t tick_count;
};

struct _motor_state
{
	uint32_t left_pwm_channel;
	struct pwm_config left_pwm_config;
	uint32_t right_pwm_channel;
	struct pwm_config right_pwm_config;
};

struct _fan_state
{
	uint32_t pwm_channel;
	struct pwm_config fan_pwm_config;
};

static hal_sem_t adc_downview_enable_sem;
static hal_sem_t gpio_col_sem;
//static uint32_t gpadc0_3_vol_data, gpadc0_8_vol_data, gpadc1_2_vol_data, gpadc1_3_vol_data;
static uint32_t motor_code_r, motor_code_l;
static hal_queue_t event_queue;
static int robot_direction;
struct _motor_state motor_state;
struct _fan_state fan_state;

// 设置左右轮四路pwm
void setMotorPwm(uint32_t left_pwm_duty_ns, uint32_t right_pwm_duty_ns)
{

	motor_state.left_pwm_channel = 2;
	motor_state.left_pwm_config.duty_ns = left_pwm_duty_ns;
	motor_state.left_pwm_config.period_ns = PWM_PERIOD;
	motor_state.left_pwm_config.polarity  = PWM_POLARITY_NORMAL;

	motor_state.right_pwm_channel = 4;
	motor_state.right_pwm_config.duty_ns = right_pwm_duty_ns;
	motor_state.right_pwm_config.period_ns = PWM_PERIOD;
	motor_state.right_pwm_config.polarity  = PWM_POLARITY_NORMAL;

	hal_pwm_init(HAL_DSP_PWM);
	hal_pwm_control(HAL_DSP_PWM, motor_state.left_pwm_channel, &motor_state.left_pwm_config);
	hal_pwm_control(HAL_DSP_PWM, motor_state.right_pwm_channel, &motor_state.right_pwm_config);
}


// 设置风机pwm
void setFanPwm(uint32_t pwm_duty_ns)
{
	fan_state.pwm_channel = 5;
	fan_state.fan_pwm_config.duty_ns = pwm_duty_ns;
	fan_state.fan_pwm_config.period_ns = PWM_PERIOD;
	fan_state.fan_pwm_config.polarity  = PWM_POLARITY_NORMAL;

	hal_pwm_init(HAL_CPUX_PWM0);
	hal_pwm_control(HAL_CPUX_PWM0, fan_state.pwm_channel, &fan_state.fan_pwm_config);
}

static hal_irqreturn_t gpio_irq_test(void *data)
{
    hal_log_info("fake gpio interrupt handler");

    return 0;
}

static int gpio_init(void)
{
	uint32_t irq;
	int ret;

    hal_gpio_set_pull(GPIO_TEST, GPIO_PULL_UP);
    hal_gpio_set_direction(GPIO_TEST, GPIO_DIRECTION_OUTPUT);

    ret = hal_gpio_to_irq(GPIO_TEST, &irq);
    if (ret < 0)
    {
        hal_log_err("gpio to irq error, irq num:%d error num: %d", irq, ret);
        goto failed;
    }

    ret = hal_gpio_irq_request(irq, gpio_irq_test, IRQ_TYPE_EDGE_FALLING, NULL);
    if (ret < 0)
    {
        hal_log_err("request irq error, irq num:%d error num: %d", irq, ret);
        goto failed;
    }

    ret = hal_gpio_irq_enable(irq);
    if (ret < 0)
    {
        hal_log_err("request irq error, error num: %d", ret);
        goto failed;
    }

	return 0;

failed:
	hal_log_err("gpio failed!");
	return -1;
}

static hal_irqreturn_t col_gpio_irq_func(void *data)
{
	hal_sem_post(gpio_col_sem);

    return 0;
}

void col_gpio_function_thread(void *param)
{
	gpio_data_t col_mr_value, col_ml_value;
	enum robot_event event;
	(void)param;

	while (1) {
		hal_sem_wait(gpio_col_sem);
		hal_gpio_get_data(GPIO_COL_MR, &col_mr_value);
		hal_gpio_get_data(GPIO_COL_ML, &col_ml_value);

		if (col_mr_value == 1) {
			event = TRIGGER_COL_MR;
			hal_queue_send(event_queue, &event);
		}

		if (col_ml_value == 1) {
			event = TRIGGER_COL_ML;
			hal_queue_send(event_queue, &event);
		}
	}
	hal_thread_stop(NULL);
}

static int col_gpio_init(void)
{
	uint32_t col_mr_irq;
	uint32_t col_ml_irq;
	void *col_thread;
	int ret;

	gpio_col_sem = hal_sem_create(0);
    hal_gpio_set_pull(GPIO_COL_MR, GPIO_PULL_UP);
    hal_gpio_set_direction(GPIO_COL_MR, GPIO_DIRECTION_OUTPUT);
    hal_gpio_set_pull(GPIO_COL_ML, GPIO_PULL_UP);
    hal_gpio_set_direction(GPIO_COL_ML, GPIO_DIRECTION_OUTPUT);

    ret = hal_gpio_to_irq(GPIO_COL_MR, &col_mr_irq);
    if (ret < 0)
    {
        hal_log_err("gpio to col_mr_irq error, col_mr_irq num:%d error num: %d", col_mr_irq, ret);
        goto failed;
    }
    ret = hal_gpio_to_irq(GPIO_COL_ML, &col_ml_irq);
    if (ret < 0)
    {
        hal_log_err("gpio to col_ml_irq error, col_ml_irq num:%d error num: %d", col_ml_irq, ret);
        goto failed;
    }

    ret = hal_gpio_irq_request(col_mr_irq, col_gpio_irq_func, IRQ_TYPE_EDGE_RISING, NULL);
    if (ret < 0)
    {
        hal_log_err("request col_mr_irq error, col_mr_irq num:%d error num: %d", col_mr_irq, ret);
        goto failed;
    }
    ret = hal_gpio_irq_request(col_ml_irq, col_gpio_irq_func, IRQ_TYPE_EDGE_RISING, NULL);
    if (ret < 0)
    {
        hal_log_err("request col_ml_irq error, col_ml_irq num:%d error num: %d", col_ml_irq, ret);
        goto failed;
    }

    ret = hal_gpio_irq_enable(col_mr_irq);
    if (ret < 0)
    {
        hal_log_err("request col_mr_irq error, error num: %d", ret);
        goto failed;
    }
    ret = hal_gpio_irq_enable(col_ml_irq);
    if (ret < 0)
    {
        hal_log_err("request col_ml_irq error, error num: %d", ret);
        goto failed;
    }

	col_thread = hal_thread_create(col_gpio_function_thread, NULL,
							"col_gpio_function", 8 * 1024, HAL_THREAD_PRIORITY_SYS);
	if (col_thread)
		 hal_thread_start(col_thread);

	return 0;

failed:
	hal_log_err("gpio failed!");
	return -1;
}

static void hal_htimer_irq_callback(void *param)
{
	struct timer_handle *timer_hdl = (struct timer_handle *)param;

	timer_hdl->tick_count++;

	switch(timer_hdl->tick_count % 10) {
		case 0:
			/* disable DOWNVIEW IR */
			hal_gpio_set_data(GPIO_ADC_DOWNVIEW_ENABLE, 1);
			break;
		case 1:
			/* check motor current */
			adc_motor_l_vol = gpadc_read_channel_data(ADC_MOTOR_L/CHANNEL_MAX_NUM, ADC_MOTOR_L%CHANNEL_MAX_NUM);
			adc_motor_r_vol = gpadc_read_channel_data(ADC_MOTOR_R/CHANNEL_MAX_NUM, ADC_MOTOR_R%CHANNEL_MAX_NUM);
			adc_fan_i_vol = gpadc_read_channel_data(ADC_FAN_I/CHANNEL_MAX_NUM, ADC_FAN_I%CHANNEL_MAX_NUM);
			adc_roller_i_vol = gpadc_read_channel_data(ADC_ROLLER_I/CHANNEL_MAX_NUM, ADC_ROLLER_I%CHANNEL_MAX_NUM);
			adc_sidebrush_vol = gpadc_read_channel_data(ADC_SIDEBRUSH/CHANNEL_MAX_NUM, ADC_SIDEBRUSH%CHANNEL_MAX_NUM);
			if (timer_hdl->tick_count % 31 == 0)
			printf("adc_motor_l_vol %d adc_motor_r_vol %d adc_fan_i_vol %d adc_roller_i_vol %d adc_sidebrush_vol %d\n",
						adc_motor_l_vol, adc_motor_r_vol, adc_fan_i_vol, adc_roller_i_vol, adc_sidebrush_vol);
			break;
		case 2:
			/* report motor code */
			if (timer_hdl->tick_count % 32 == 0)
			printf("motor_code_r %u motor_code_l %u\n", motor_code_r, motor_code_l);
			break;
		case 9:
			/* enable DOWNVIEW IR */
			hal_gpio_set_data(GPIO_ADC_DOWNVIEW_ENABLE, 0);
			hal_sem_post(adc_downview_enable_sem); /* */
			break;
		default:
			break;
	}
}

static void read_adc_downview_data_thread(void *data)
{
	enum robot_event event;

	while (1) {
		/* waiting sem */
		hal_sem_wait(adc_downview_enable_sem);
		hal_udelay(50);

		adc_downview_ml_vol = gpadc_read_channel_data(ADC_DOWNVIEW_ML/CHANNEL_MAX_NUM, ADC_DOWNVIEW_ML%CHANNEL_MAX_NUM);
		adc_downview_mr_vol = gpadc_read_channel_data(ADC_DOWNVIEW_MR/CHANNEL_MAX_NUM, ADC_DOWNVIEW_MR%CHANNEL_MAX_NUM);
		adc_downview_l_vol = gpadc_read_channel_data(ADC_DOWNVIEW_L/CHANNEL_MAX_NUM, ADC_DOWNVIEW_L%CHANNEL_MAX_NUM);
		adc_downview_r_vol = gpadc_read_channel_data(ADC_DOWNVIEW_R/CHANNEL_MAX_NUM, ADC_DOWNVIEW_R%CHANNEL_MAX_NUM);

		if (adc_downview_ml_vol < ADC_DOWNVIEW_THRESHOLD_MV ||
				adc_downview_mr_vol < ADC_DOWNVIEW_THRESHOLD_MV ||
				adc_downview_l_vol < ADC_DOWNVIEW_THRESHOLD_MV ||
				adc_downview_r_vol < ADC_DOWNVIEW_THRESHOLD_MV) {
			event = TRIGGER_DOWNVIEW_ML;	/* stop */
			hal_queue_send(event_queue, &event);
		}else if (adc_downview_ml_vol > ADC_DOWNVIEW_THRESHOLD_MV &&
				adc_downview_mr_vol > ADC_DOWNVIEW_THRESHOLD_MV &&
				adc_downview_l_vol > ADC_DOWNVIEW_THRESHOLD_MV &&
				adc_downview_r_vol > ADC_DOWNVIEW_THRESHOLD_MV) {
			event = TRIGGER_RUN;			/* start */
			hal_queue_send(event_queue, &event);
		}
	}
}

static int htimer_init(struct timer_handle *timer_hdl)
{
	int ret;

	timer_hdl->timer = HAL_HRTIMER5;

    hal_gpio_set_pull(GPIO_ADC_DOWNVIEW_ENABLE, GPIO_PULL_UP);
    hal_gpio_set_direction(GPIO_ADC_DOWNVIEW_ENABLE, GPIO_DIRECTION_OUTPUT);
	hal_htimer_init(timer_hdl->timer);
	ret = hal_htimer_set_periodic(timer_hdl->timer, TIMER_TICK_US, hal_htimer_irq_callback, timer_hdl);

	return ret;
}

static int gpadc_init(void)
{
	hal_gpadc_init(0);
	hal_gpadc_channel_init(ADC_DOWNVIEW_ML/CHANNEL_MAX_NUM, ADC_DOWNVIEW_ML%CHANNEL_MAX_NUM, 0);
	hal_gpadc_channel_init(ADC_BAT_TEMP/CHANNEL_MAX_NUM, ADC_BAT_TEMP%CHANNEL_MAX_NUM, 0);
	hal_gpadc_channel_init(ADC_FRONTVIEW_MR/CHANNEL_MAX_NUM, ADC_FRONTVIEW_MR%CHANNEL_MAX_NUM, 0);
	hal_gpadc_channel_init(ADC_VIEW_SIDE_L/CHANNEL_MAX_NUM, ADC_VIEW_SIDE_L%CHANNEL_MAX_NUM, 0);
	hal_gpadc_channel_init(ADC_VIEW_SIDE_R/CHANNEL_MAX_NUM, ADC_VIEW_SIDE_R%CHANNEL_MAX_NUM, 0);
	hal_gpadc_channel_init(ADC_DOWNVIEW_MR/CHANNEL_MAX_NUM, ADC_DOWNVIEW_MR%CHANNEL_MAX_NUM, 0);
	hal_gpadc_channel_init(ADC_FRONTVIEW_ML/CHANNEL_MAX_NUM, ADC_FRONTVIEW_ML%CHANNEL_MAX_NUM, 0);
	hal_gpadc_channel_init(ADC_FRONTVIEW_M/CHANNEL_MAX_NUM, ADC_FRONTVIEW_M%CHANNEL_MAX_NUM, 0);
	hal_gpadc_channel_init(ADC_MOTOR_L/CHANNEL_MAX_NUM, ADC_MOTOR_L%CHANNEL_MAX_NUM, 0);

	hal_gpadc_init(1);
	hal_gpadc_channel_init(ADC_ROLLER_I/CHANNEL_MAX_NUM, ADC_ROLLER_I%CHANNEL_MAX_NUM, 0);
	hal_gpadc_channel_init(ADC_BQ_IOUT/CHANNEL_MAX_NUM, ADC_BQ_IOUT%CHANNEL_MAX_NUM, 0);
	hal_gpadc_channel_init(ADC_DOWNVIEW_L/CHANNEL_MAX_NUM, ADC_DOWNVIEW_L%CHANNEL_MAX_NUM, 0);
	hal_gpadc_channel_init(ADC_DOWNVIEW_R/CHANNEL_MAX_NUM, ADC_DOWNVIEW_R%CHANNEL_MAX_NUM, 0);
	hal_gpadc_channel_init(ADC_MOTOR_R/CHANNEL_MAX_NUM, ADC_MOTOR_R%CHANNEL_MAX_NUM, 0);
	hal_gpadc_channel_init(ADC_FAN_I/CHANNEL_MAX_NUM, ADC_FAN_I%CHANNEL_MAX_NUM, 0);
	hal_gpadc_channel_init(ADC_SIDEBRUSH/CHANNEL_MAX_NUM, ADC_SIDEBRUSH%CHANNEL_MAX_NUM, 0);
	hal_gpadc_channel_init(ADC_COMSUME_L/CHANNEL_MAX_NUM, ADC_COMSUME_L%CHANNEL_MAX_NUM, 0);
	hal_gpadc_channel_init(ADC_FRONTVIEW_R/CHANNEL_MAX_NUM, ADC_FRONTVIEW_R%CHANNEL_MAX_NUM, 0);
	hal_gpadc_channel_init(ADC_FRONTVIEW_L/CHANNEL_MAX_NUM, ADC_FRONTVIEW_L%CHANNEL_MAX_NUM, 0);
	hal_gpadc_channel_init(ADC_BAT_VOLTAGE/CHANNEL_MAX_NUM, ADC_BAT_VOLTAGE%CHANNEL_MAX_NUM, 0);
	hal_gpadc_channel_init(ADC_PUMP_I/CHANNEL_MAX_NUM, ADC_PUMP_I%CHANNEL_MAX_NUM, 0);

	return 0;
}

static void motor_code_r_cap_callback(void* param)
{
	hal_pwm_cap_info *info = (hal_pwm_cap_info *)param;

	motor_code_r++;
	printf("pwm%d capture callback, cnt is %d, period is %d, duty is %d\n", info->channel, info->cnt, info->period, info->duty);
}

static void motor_code_l_cap_callback(void* param)
{
	hal_pwm_cap_info *info = (hal_pwm_cap_info *)param;

	motor_code_l++;
	printf("pwm%d capture callback, cnt is %d, period is %d, duty is %d\n", info->channel, info->cnt, info->period, info->duty);
}

static int motor_code_init(void)
{
	motor_code_r = 0;
	motor_code_l = 0;
	hal_pwm_cap_enable(HAL_CPUX_PWM0, 12, motor_code_r_cap_callback);
	hal_pwm_cap_enable(HAL_CPUX_PWM0, 13, motor_code_l_cap_callback);

	return 0;
}

static int event_handle(int event)
{
	switch(event) {
		case TRIGGER_RUN:
			/* start */
			if (robot_direction) {
				motor_state.left_pwm_config.duty_ns = PWM_POSITIVE;
				motor_state.right_pwm_config.duty_ns = PWM_POSITIVE;
			} else {
				motor_state.left_pwm_config.duty_ns = PWM_REVERSE;
				motor_state.right_pwm_config.duty_ns = PWM_REVERSE;
			}
			setMotorPwm(motor_state.left_pwm_config.duty_ns, motor_state.right_pwm_config.duty_ns);
			break;
		case TRIGGER_DOWNVIEW_ML:
		case TRIGGER_DOWNVIEW_MR:
		case TRIGGER_DOWNVIEW_L:
		case TRIGGER_DOWNVIEW_R:
			/* turn round */
			setMotorPwm(PWM_STOP, PWM_STOP);
			printf("TRIGGER_DOWNVIEW\n");
			break;
		case TRIGGER_COL_ML:
		case TRIGGER_COL_MR:
			/* turn round */
			if (robot_direction) {
				motor_state.left_pwm_config.duty_ns = PWM_REVERSE;
				motor_state.right_pwm_config.duty_ns = PWM_REVERSE;
				robot_direction = 0;
			} else {
				motor_state.left_pwm_config.duty_ns = PWM_POSITIVE;
				motor_state.right_pwm_config.duty_ns = PWM_POSITIVE;
				robot_direction = 1;
			}
			setMotorPwm(motor_state.left_pwm_config.duty_ns, motor_state.right_pwm_config.duty_ns);
			break;
		default:
			break;
	}

	return 0;
}

int cmd_robot_test(int argc, char** argv)
{
	struct timer_handle timer_hdl;
	void *adc_downview_thread;
	enum robot_event event;

	robot_direction = 1; /* forward */
	adc_downview_enable_sem = hal_sem_create(0);
	event_queue = hal_queue_create("robot_event_queue", sizeof(uint32_t), EVENT_QUEUE_SIZE);
	memset(&timer_hdl, 0, sizeof(timer_hdl));
	memset(&motor_state, 0, sizeof(motor_state));

	if (robot_direction) {
		motor_state.left_pwm_config.duty_ns = PWM_POSITIVE;
		motor_state.right_pwm_config.duty_ns = PWM_POSITIVE;
	} else {
		motor_state.left_pwm_config.duty_ns = PWM_REVERSE;
		motor_state.right_pwm_config.duty_ns = PWM_REVERSE;
	}
	setMotorPwm(motor_state.left_pwm_config.duty_ns, motor_state.right_pwm_config.duty_ns);;

	adc_downview_thread = hal_thread_create(read_adc_downview_data_thread, NULL,
							"adc_downview_function", 8 * 1024, HAL_THREAD_PRIORITY_SYS);
	if (adc_downview_thread)
		 hal_thread_start(adc_downview_thread);

	gpio_init();
	col_gpio_init();
	gpadc_init();
	htimer_init(&timer_hdl);
	hal_gpio_set_data(GPIO_TEST, 0);
	motor_code_init();
	setFanPwm(PWM_POSITIVE);

	while (1) {
		hal_queue_recv(event_queue, &event, HAL_WAIT_FOREVER);
		event_handle(event);
	}

	return 0;
}

FINSH_FUNCTION_EXPORT_CMD(cmd_robot_test, robot, robot test)
