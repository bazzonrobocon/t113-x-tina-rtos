#ifndef __PM_PLAT__
#define __PM_PLAT__

#include <pm_base.h>

int pm_plat_platops_init(void);
int pm_plat_platops_deinit(void);

void pm_systeminit(void);
int pm_client_init(void);

int pm_trigger_suspend(suspend_mode_t mode);

int pm_client_init(void);

void plat_suspend(void);
void plat_resume(void);

#endif
