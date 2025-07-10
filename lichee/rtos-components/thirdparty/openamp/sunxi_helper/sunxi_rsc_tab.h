#ifndef OPENAMP_SUNXI_RSC_TAB_H_
#define OPENAMP_SUNXI_RSC_TAB_H_

#include <hal_osal.h>
#include <stddef.h>
#include <openamp/open_amp.h>

/**
 * resource_table_init
 *
 * Intialize resource table.
 *
 * @rsc_id: resource table index
 * @table_ptr(out): pointer to store resource table
 * @length(out): length of resource table
 *
 * Return 0 on success, otherwise a negative number on failure.
 */
int resource_table_init(int rsc_id, void **table_ptr, int *length);

struct fw_rsc_carveout *
resource_table_get_rvdev_shm_entry(void *rsc_table, int rvdev_id);

struct fw_rsc_vdev *resource_table_get_vdev(int index);

struct fw_rsc_carveout *
resource_table_get_share_irq_entry(void *rsc_table, int rvdev_id);

#ifdef CONFIG_ARCH_SUN20IW3
#include "sunxi/sun20iw3/rsc_tab.h"
#endif

#ifdef CONFIG_ARCH_SUN55IW3
#include "sunxi/sun55iw3/rsc_tab.h"
#endif

#endif
