/*
* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
*
* Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
* the the People's Republic of China and other countries.
* All Allwinner Technology Co.,Ltd. trademarks are used with permission.
*
* DISCLAIMER
* THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
* IF YOU NEED TO INTEGRATE THIRD PARTY¡¯S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
* IN ALLWINNERS¡¯SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
* ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
* ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
* COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
* YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY¡¯S TECHNOLOGY.
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

#ifndef __HAL_IOMMU_H__
#define __HAL_IOMMU_H__

#include <stdbool.h>

#include <aw_list.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PAGE_SIZE (4096)

struct iommu_vm_struct
{
    int size;
    unsigned long start_addr;
    struct list_head i_list;
};

struct ion_handle
{
    int size;
    int npages;
    void **pages;

    struct iommu_vm_struct *kernel_vma;
    struct iommu_vm_struct *dma_vma;
    struct list_head i_list;
};

struct ion_cmd_args
{
    int size;
    struct ion_handle *handle;
};

typedef enum _ion_cmd
{
    ION_CMD_ALLOC = 0,
    ION_CMD_FREE,
    ION_CMD_MAP_KERNEL,
    ION_CMD_UNMAP_KERNEL,
    ION_CMD_MAP_DMA,
    ION_CMD_UNMAP_DMA,
} ion_cmd_t;

int hal_iommu_init(void);
struct ion_handle *ion_alloc(int size);
void ion_free(struct ion_handle *handle);
void *ion_map_kernel(struct ion_handle *handle);
void ion_unmap_kernel(struct ion_handle *handle);
void *ion_map_dma(struct ion_handle *handle);
void ion_unmap_dma(struct ion_handle *handle);

void sunxi_enable_device_iommu(unsigned int master_id, bool flag);
void sunxi_reset_device_iommu(unsigned int master_id);

struct ion_handle *find_ion_handle_by_dma_addr(void *dma_addr);
struct ion_handle *find_ion_handle_by_kernel_addr(void *kernel_addr);

#ifdef __cplusplus
}
#endif
#endif
