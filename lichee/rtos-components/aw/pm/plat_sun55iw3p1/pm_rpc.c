#include <stdlib.h>
#include <string.h>
#include <osal/hal_interrupt.h>
#include <errno.h>

#include <pm_adapt.h>
#include <pm_debug.h>
#include <pm_suspend.h>
#include <pm_wakesrc.h>
#include <pm_platops.h>
#include <pm_subsys.h>
#include <pm_rpcfunc.h>

int pm_set_wakesrc(int id, int core, int status)
{
	return 0;
}

int pm_subsys_soft_wakeup(int affinity, int irq, int action)
{
	return 0;
}
