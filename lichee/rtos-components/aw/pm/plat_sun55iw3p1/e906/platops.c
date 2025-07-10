/*
* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
*
* Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
* the the people's Republic of China and other countries.
* All Allwinner Technology Co.,Ltd. trademarks are used with permission.
*
* DISCLAIMER
* THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
* IF YOU NEED TO INTEGRATE THIRD PARTY’S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
* IN ALLWINNERS’SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
* ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
* ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
* COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
* YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY’S TECHNOLOGY.
*
*
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

#include <errno.h>
#include <hal_osal.h>
#include <osal/hal_interrupt.h>
#include <io.h>

#include <pm_suspend.h>
#include <pm_debug.h>
#include <pm_wakecnt.h>
#include <pm_wakesrc.h>
#include <pm_subsys.h>
#include <pm_syscore.h>
#include <pm_platops.h>
#include <pm_testlevel.h>

#include <pm_e906_client.h>
#include <pm_plat.h>

extern void cpu_suspend(void);
extern void cpu_resume(void);
extern long g_sleep_flag;

static int pm_e906_valid(suspend_mode_t mode)
{
	int ret = 0;

	switch (mode) {
	case PM_MODE_SLEEP:
	case PM_MODE_STANDBY:
	case PM_MODE_HIBERNATION:
		break;
	case PM_MODE_ON:
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int pm_e906_prepare_late(suspend_mode_t mode)
{
	int ret = 0;

	switch (mode) {
	case PM_MODE_SLEEP:
	case PM_MODE_STANDBY:
	case PM_MODE_HIBERNATION:
		/* ret = pm_suspend_assert(); */
		ret = pm_common_syscore_suspend(mode);
		break;
	case PM_MODE_ON:
	default:
		ret = -EINVAL;
		break;
	}

	return ret;

}

static int set_finished_dummy(void)
{
	writel(0x16aa0000 | PM_FINISH_FLAG, PM_STANDBY_STAGE_REC_REG);

	return 0;
}

static int clear_finished_dummy(void)
{
	writel(0x0, PM_STANDBY_STAGE_REC_REG);

	return 0;
}

static int suspend_is_finished(void)
{
	uint32_t reg_val;

	reg_val = readl(PM_STANDBY_STAGE_REC_REG);
	if ((reg_val & 0xffff0000) == 0x16aa0000) {
		if (!(reg_val & PM_FINISH_FLAG))
			return 1;
		else
			return 0;
	}

	return 0;
}

extern void hal_icache_init(void);
extern void hal_dcache_init(void);
static int pm_e906_enter(suspend_mode_t mode)
{
	int ret = 0;

	switch (mode) {
	case PM_MODE_SLEEP:
	case PM_MODE_STANDBY:
	case PM_MODE_HIBERNATION:
		plat_suspend();
		cpu_suspend();
		if (g_sleep_flag) {
			pm_dbg("suspened\n");
			hal_dcache_clean_all();
			set_finished_dummy();
			while (!suspend_is_finished()) {
				__asm__ __volatile__("wfi");
			}
		}
		clear_finished_dummy();
		plat_resume();
		pm_dbg("resume\n");
		break;
	case PM_MODE_ON:
	default:
		ret = -EINVAL;
		break;
	}

	if (mode != PM_MODE_SLEEP)
		pm_systeminit();

	return ret;
}

static int pm_e906_wake(suspend_mode_t mode)
{
	int ret = 0;

	switch (mode) {
	case PM_MODE_SLEEP:
	case PM_MODE_STANDBY:
	case PM_MODE_HIBERNATION:
		ret = pm_common_syscore_resume(mode);
		break;
	case PM_MODE_ON:
	default:
		ret = -EINVAL;
		break;
	}

	return ret;

}

static int pm_e906_recover(suspend_mode_t mode)
{
	int ret = 0;

	switch (mode) {
	case PM_MODE_SLEEP:
		break;
	case PM_MODE_STANDBY:
		break;
	case PM_MODE_HIBERNATION:
		break;
	case PM_MODE_ON:
	default:
		ret = -EINVAL;
		break;
	}

	return ret;

}

static suspend_ops_t pm_e906_suspend_ops = {
	.name  = "pm_e906_suspend_ops",
	.valid = pm_e906_valid,
	.prepare_late = pm_e906_prepare_late,
	.enter = pm_e906_enter,
	.wake = pm_e906_wake,
	.recover = pm_e906_recover,
};

int pm_plat_platops_init(void)
{
	return pm_platops_register(&pm_e906_suspend_ops);
}

int pm_plat_platops_deinit(void)
{
	return pm_platops_register(NULL);
}



