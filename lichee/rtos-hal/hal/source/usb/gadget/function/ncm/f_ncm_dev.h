#ifndef __F_NCM_DEV_H__
#define __F_NCM_DEV_H__
#include <rtdef.h>
#include "function_ncm.h"

int sunxi_ncm_net_init(struct f_ncm *ncm);
int sunxi_ncm_net_deinit(struct f_ncm *ncm);
#endif
