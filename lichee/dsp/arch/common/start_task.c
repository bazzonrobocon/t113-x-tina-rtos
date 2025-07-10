#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#ifdef CONFIG_DRIVERS_GPIO
#include <hal_gpio.h>
#endif
#ifdef CONFIG_COMPONENTS_FREERTOS_CLI
#include <console.h>
#endif
#ifdef CONFIG_DRIVERS_DMA
#include <hal_dma.h>
#endif
#ifdef CONFIG_DRIVERS_MSGBOX_AMP
#include <hal_msgbox.h>
#endif
#ifdef CONFIG_COMPONENTS_AW_ALSA_RPAF
#include <aw_rpaf/common.h>
#endif
#ifdef CONFIG_COMPONENTS_OPENAMP
#include <hal_thread.h>
extern int openamp_init(void);
#endif
#ifdef CONFIG_COMPONENTS_RPBUF
extern int rpbuf_init(void);
#endif
#ifdef CONFIG_COMPONENTS_OPENAMP_VIRTUAL_DRIVER
#include <openamp/virtual_driver/virt_uart.h>
#include <openamp/virtual_driver/virt_rproc_srm.h>
#endif
#ifdef CONFIG_RPMSG_CLIENT
#include <openamp/sunxi_helper/rpmsg_master.h>
#endif
#ifdef CONFIG_RPMSG_HEARBEAT
extern int rpmsg_heart_init(void);
#endif
#ifdef CONFIG_COMPONENTS_CEDARSE
#include <sfx_mgmt.h>
#endif

__attribute__((weak)) void print_banner(void)
{

}

__attribute__((weak)) void app_init(void)
{

}

#ifdef CONFIG_COMPONENTS_OPENAMP
static void openamp_init_thread(void *param)
{
        (void)param;

        openamp_init();

#ifdef CONFIG_RPMSG_CLIENT
        rpmsg_ctrldev_create();
#endif

#ifdef CONFIG_RPMSG_HEARBEAT
        rpmsg_heart_init();
#endif

#ifdef CONFIG_MULTI_CONSOLE
        extern int multiple_console_init(void);
        multiple_console_init();
#endif

#ifdef CONFIG_COMPONENTS_RPBUF
	rpbuf_init();
#endif

#ifdef CONFIG_COMPONENTS_CEDARSE
	sfxmgmt_init();
#endif

        hal_thread_stop(NULL);
}
#endif

void cpu_app_entry(void *param)
{
#ifdef CONFIG_COMPONENTS_OPENAMP
        void *thread;
        thread = hal_thread_create(openamp_init_thread, NULL,
				"amp_init", 8 * 1024, HAL_THREAD_PRIORITY_SYS);
        if (thread != NULL)
            hal_thread_start(thread);
#endif

#if defined(CONFIG_COMPONENTS_FREERTOS_CLI) && !defined(CONFIG_COMPONENTS_OPENAMP_VIRTUAL_DRIVER)
	setbuf(stdout, 0);
	setbuf(stdin, 0);
	setvbuf(stdin, NULL, _IONBF, 0);

	if (console_uart != UART_UNVALID) {
		vUARTCommandConsoleStart(0x1000, 1);
	}
#endif

	app_init();

	vTaskDelete(NULL);
}

void start_task(void)
{
	portBASE_TYPE ret;

#ifdef CONFIG_MULTI_CONSOLE
	extern int multiple_console_early_init(void);
	multiple_console_early_init();
#endif

	print_banner();

#ifdef CONFIG_LINUX_DEBUG
	pr_info_thread("dsp0 debug init ok\n");
	log_mutex_init();
#endif


#ifdef CONFIG_DRIVERS_MSGBOX_AMP
	hal_msgbox_init();
#endif
#ifdef CONFIG_COMPONENTS_OPENAMP_VIRTUAL_DRIVER
	virt_uart_init();
	virt_rproc_srm_init();
#endif

#ifdef CONFIG_DRIVERS_DMA
	/* dma init */
	hal_dma_init();
#endif

	ret = xTaskCreate(cpu_app_entry, (const char *) "init-thread", 1024, NULL, 3, NULL);
	if (ret != pdPASS) {
		printf("Error creating task, status was %d\n", ret);
	}
}
