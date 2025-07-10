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
static int test_htimer0 = HAL_HRTIMER0;
static int delay_ms_value = 1;
static void os_thread0(void *param)
{
	int old_value = 0;
	int new_value = 0;
	int diff_value = 0;
	int os_test_step = 0;
	float timer_value = 0;
	float timer_value_max = 0;
	float timer_value_min = 55555;
	for(;;) {
		if (++os_test_step < 10) {
			old_value = hal_htimer_read_cntlow(test_htimer0);
			hal_msleep(delay_ms_value);
			new_value = hal_htimer_read_cntlow(test_htimer0);
			diff_value = old_value - new_value;
			timer_value = diff_value*1.0f/htimer_freq * 1000;
			if (timer_value > timer_value_max)
				timer_value_max = timer_value;
			if (timer_value < timer_value_min)
				timer_value_min = timer_value;

#if 0
			printf("\n--- os_thread0[%d] ---\n", os_test_step);
			printf("old_value %d, new_value %d, htimer_freq %d, diff_value %d\n", \
					old_value, new_value, htimer_freq, diff_value);
			printf("timer_value %0.8f ms\n", timer_value);
#endif
		}else {
			hal_msleep(1);
			break;
		}
	}
	printf("delay_ms_value %d ms\n", delay_ms_value);
	printf("timer_value_min %0.8f ms, diff %0.8f ms\n", timer_value_min, (timer_value_min - delay_ms_value*1.0f));
	printf("timer_value_max %0.8f ms, diff %0.8f ms\n", timer_value_max, (timer_value_max - delay_ms_value*1.0f));
	/* clsoe htimer */
	hal_htimer_deinit(test_htimer0);
	hal_htimer_base_deinit();
	hal_thread_stop(NULL);
}

static void hal_htimer_irq_callback(void *param)
{

}

static int cmd_scheduler_jitter_test(int argc, char *argv[])
{
	void *thread;

	int priority_os = 0;
	if (argc >= 2) {
		if (strcmp("-h", argv[1]) == 0) {
			printf("usage: scheduler_jitter_test <priority_os> <delay_ms_value>\n");
			return 0;
		}
	}

	if (atoi(argv[1]) < 32)
		priority_os = atoi(argv[1]);

	if (atoi(argv[2]) < 1000)
		delay_ms_value = atoi(argv[2]);

	hal_htimer_base_init();

	hal_htimer_init(test_htimer0);

	hal_htimer_set_oneshot(test_htimer0, (20*1000*1000), hal_htimer_irq_callback, NULL);

	htimer_freq = hal_htimer_freq();

	thread = hal_thread_create(os_thread0, NULL, \
								"os_thread0", 1 * 1024, priority_os);
	if (thread != NULL)
        hal_thread_start(thread);

	return 0;
}

FINSH_FUNCTION_EXPORT_CMD(cmd_scheduler_jitter_test, scheduler_jitter_test, scheduler jitter test for os);


