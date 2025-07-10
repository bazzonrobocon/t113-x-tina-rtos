#include <serial.h>
#include <interrupt.h>
#include <stdio.h>
#include <stdint.h>
#include <FreeRTOS.h>
#include <string.h>
#include <task.h>
#include <aw_version.h>
#include <irqs.h>
#include <platform.h>
#include <memory.h>
#include <hal_gpio.h>
#include <hal_uart.h>
#include <hal_msgbox.h>
#include <hal_clk.h>
#include <hal_cache.h>
#ifdef CONFIG_DRIVERS_DMA
#include <hal_dma.h>
#endif

#ifdef CONFIG_DRIVERS_TWI
#include <sunxi_hal_twi.h>
#endif

#include "excep.h"

#ifdef CONFIG_COMPONENTS_AW_SYS_CONFIG_SCRIPT
#include <hal_cfg.h>
#endif

#include <compiler.h>

#ifdef CONFIG_DRIVERS_SPINOR
#include <sunxi_hal_spinor.h>
#endif

#ifdef CONFIG_COMPONENTS_PM
#include "pm_init.h"
#endif

#ifdef CONFIG_DRIVERS_MSGBOX
#include <hal_msgbox.h>
#endif

#ifdef CONFIG_PROJECT_T113_C906
#define RV_PLATFORM       "T113_C906"
#elif CONFIG_PROJECT_T113_I_C906
#define RV_PLATFORM       "T113_I_C906"
#elif defined(CONFIG_PROJECT_T113_S3P_C906)
#define RV_PLATFORM       "T113_S3P_C906"
#elif defined(CONFIG_PROJECT_T113_S4P_C906)
#define RV_PLATFORM       "T113_S4P_C906"
#elif defined(CONFIG_PROJECT_T113_S4_C906)
#define RV_PLATFORM       "T113_S4_C906"
#endif

static void print_banner()
{
    printf("\r\n");
    printf(" *******************************************\r\n");
    printf(" ** Welcome to %s FreeRTOS %-10s**\r\n", RV_PLATFORM, TINA_RT_VERSION_NUMBER);
    printf(" ** Copyright (C) 2019-2021 AllwinnerTech **\r\n");
    printf(" **                                       **\r\n");
    printf(" **      starting riscv FreeRTOS          **\r\n");
    printf(" *******************************************\r\n");
    printf("\r\n");
    printf("Date:%s, Time:%s\n", __DATE__, __TIME__);
}

void system_tick_init(void);
void enter_interrupt_handler(void);
void exit_interrupt_handler(void);
int clic_driver_init(void);
void plic_init(void);
void timekeeping_init(void);
void handle_arch_irq(irq_regs_t *regs);

void C906_Default_IRQHandler(void)
{
    enter_interrupt_handler();
    handle_arch_irq(NULL);
    exit_interrupt_handler();
}

__weak void sunxi_dma_init(void)
{
    return;
}

__weak void sunxi_gpio_init(void)
{
    return;
}

__weak int sunxi_soundcard_init(void)
{
    return 0;
}

__weak void heap_init(void)
{
    return;
}

static void prvSetupHardware(void)
{
    timekeeping_init();
    plic_init();
    irq_init();
    hal_clock_init();
    hal_gpio_init();
#ifdef CONFIG_DRIVERS_MSGBOX_AMP
    hal_msgbox_init();
#endif

#if !defined(CONFIG_DISABLE_ALL_UART_LOG)
    serial_init();

#if defined(CONFIG_COMPONENT_CLI)
    hal_uart_init_for_amp_cli(CONFIG_CLI_UART_PORT);
#endif
#endif

    clic_driver_init();
    system_tick_init();
#ifdef CONFIG_HPSRAM_INIT_IN_OS
    hpsram_init();
#endif
#ifdef CONFIG_DRIVERS_DMA
    hal_dma_init();
#endif

#ifdef CONFIG_DRIVERS_TWI
	hal_twi_init(CONFIG_SUNXI_TWI_PORT);
#endif

}

void systeminit(void)
{
}

void start_kernel(void)
{

    void cache_init(void);
    cache_init();
#ifdef CONFIG_ARCH_HAVE_ICACHE
    hal_icache_init();
#endif

#ifdef CONFIG_ARCH_HAVE_DCACHE
    hal_dcache_init();
#endif

#ifdef CONFIG_DOUBLE_FREE_CHECK
    void memleak_double_free_check_init(void);
    memleak_double_free_check_init();
#endif

#ifdef CONFIG_COMPONENTS_STACK_PROTECTOR
    void stack_protector_init(void);
    stack_protector_init();
#endif

    systeminit();
    /* Init heap */
    heap_init();

#ifdef CONFIG_COMPONENTS_AW_SYS_CONFIG_SCRIPT
	hal_cfg_init();
#endif
#ifdef CONFIG_COMPONENTS_PM
    pm_wakecnt_init();
    pm_wakesrc_init();
    pm_devops_init();
    pm_syscore_init();
#endif
    /* Init hardware devices */
    prvSetupHardware();

#ifdef CONFIG_MULTI_CONSOLE
	extern int multiple_console_early_init(void);
	multiple_console_early_init();
#endif
    print_banner();
    setbuf(stdout, 0);
    setbuf(stdin, 0);
    setvbuf(stdin, NULL, _IONBF, 0);
#ifdef CONFIG_CHECK_ILLEGAL_FUNCTION_USAGE
    void __cyg_profile_func_init(void);
    __cyg_profile_func_init();
#endif

#ifdef CONFIG_DRIVERS_MSGBOX
	hal_msgbox_init();
#endif

#ifdef CONFIG_NON_OS
	printf("enter non os system!");
	while(1) {
	}
#else
    /* Setup kernel components */
    portBASE_TYPE ret;
    void cpu0_app_entry(void *);
    ret = xTaskCreate(cpu0_app_entry, "init-thread-0", 1024, NULL, 31, NULL);
    if (ret != pdPASS)
    {
        printf("Error creating task, status was %ld\n", ret);
        while (1);
    }

    vTaskStartScheduler();
#endif
}

