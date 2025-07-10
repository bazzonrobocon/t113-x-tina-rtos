#include <osal/hal_interrupt.h>
#include <io.h>
#include <pm_plat.h>

extern void clic_suspend(void);
extern void clic_resume(void);
extern void irq_suspend(void);
extern void irq_resume(void);

static void pm_SetupHardware(void)
{

}

void pm_systeminit(void)
{
#ifdef CONFIG_ARCH_HAVE_ICACHE
	extern void hal_icache_init(void);
	hal_icache_init();
#endif

#ifdef CONFIG_ARCH_HAVE_DCACHE
	extern void hal_dcache_init(void);
	hal_dcache_init();
#endif

	pm_SetupHardware();
}

void plat_suspend(void)
{
	irq_suspend();
	clic_suspend();
}

void plat_resume(void)
{
	clic_resume();
	irq_resume();
}
