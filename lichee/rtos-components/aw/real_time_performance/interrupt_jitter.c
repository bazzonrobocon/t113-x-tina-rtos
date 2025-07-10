#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <console.h>
#include <hal_time.h>
#include <hal_mem.h>
#include <hal_thread.h>
#ifdef CONFIG_DRIVERS_HTIMER
#include <sunxi_hal_htimer.h>
#endif

static int htimer_freq = 0;
static int htimer0_old_value = 0;
static int htimer0_new_value = 0;
static int test_htimer0 = HAL_HRTIMER0;

static int test_htimer1 = HAL_HRTIMER1;
static int htimer1_irq_flag = 0;
static int htimer1_vlaue = 0;

static void hal_htimer0_irq_callback(void *param)
{

}

static void hal_htimer1_irq_callback(void *param)
{
	htimer0_new_value = hal_htimer_read_cntlow(test_htimer0);
	htimer1_irq_flag = 1;
}

static void test_thread0(void *param)
{
	float timer_value = 0;
	int timer_cnt[5] = {1, 10, 100, 1000, 2000};
	int i = 0;

	for(i = 0; i < 5; i++) {
		timer_value = 0;
		hal_htimer_base_init();
		htimer_freq = 0;
		htimer0_old_value = 0;
		htimer0_new_value = 0;
		test_htimer0 = HAL_HRTIMER0;
		test_htimer1 = HAL_HRTIMER1;
		htimer1_irq_flag = 0;
		htimer1_vlaue = timer_cnt[i];
		hal_htimer_init(test_htimer0);
		hal_htimer_set_oneshot(test_htimer0, (10*1000*1000), hal_htimer0_irq_callback, NULL);
		hal_htimer_init(test_htimer1);
		hal_htimer_set_oneshot(test_htimer1, (htimer1_vlaue*1000), hal_htimer1_irq_callback, NULL);
		htimer0_old_value = hal_htimer_read_cntlow(test_htimer0);
		htimer_freq = hal_htimer_freq();
		/*wait for timer1 interrupt */
		for(;;) {
			if (htimer1_irq_flag == 1) {
				htimer1_irq_flag = 0;
				break;
			}
			hal_msleep(1);
		}
		timer_value = 1.0f*(htimer0_old_value - htimer0_new_value)/htimer_freq * 1000 - htimer1_vlaue;
		printf("\ntest[%d], htimer1_vlaue = %d ms\n", i, htimer1_vlaue);
		printf("interrupt jitter value %0.8f ms\n", timer_value);
		/* clsoe htimer */
		hal_htimer_deinit(test_htimer0);
		hal_htimer_deinit(test_htimer1);
		hal_htimer_base_deinit();
	}
	hal_thread_stop(NULL);
}

static int cmd_interrupt_jitter_test(int argc, char *argv[])
{
	void *thread;
	thread = hal_thread_create(test_thread0, NULL, "test thread0", 1 * 1024, 15);
	if (thread != NULL)
        hal_thread_start(thread);

	return 0;
}

FINSH_FUNCTION_EXPORT_CMD(cmd_interrupt_jitter_test, interrupt_jitter_test, interrupt jitter test for os);
