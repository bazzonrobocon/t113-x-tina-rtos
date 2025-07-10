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
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include <hal_mem.h>
#include <hal_atomic.h>
#include <hal_mutex.h>
#include <hal_time.h>
#include <hal_interrupt.h>
#include <hal_status.h>
#include <hal_cache.h>

#include <sunxi_hal_common.h>

#include <hal_clk.h>

#include <aw_list.h>
#include "sunxi-iommu.h"

#include "platform_iommu.h"

#include <hal_iommu.h>

#define IOMMU_READ  (1 << 0)
#define IOMMU_WRITE (1 << 1)
#define IOMMU_CACHE (1 << 2) /*  DMA cache coherency */
#define IOMMU_NOEXEC (1 << 3)
#define IOMMU_MMIO  (1 << 4) /*  e.g. things like MSI doorbells */

static struct sunxi_iommu_dev *global_iommu_dev;
static struct sunxi_iommu_domain *global_sunxi_iommu_domain;
static bool iommu_hw_init_flag;

typedef void (*sunxi_iommu_fault_cb)(void);
static sunxi_iommu_fault_cb sunxi_iommu_fault_notify_cbs[7];

#ifdef CONFIG_DRIVERS_AW_IOMMU_TRACE

static struct sunxi_iommu_iova_info *info_head, *info_tail;
static struct sunxi_iommu_iova_info *info_head_free, *info_tail_free;

static int iova_show_on_irq(void)
{
    struct sunxi_iommu_iova_info *info_p;

    printf("device              iova_start     iova_end      size(byte)    timestamp(s)\n");
    for (info_p = info_head; info_p != NULL; info_p = info_p->next)
    {
        printk("%-16s    %8lx       %8lx      %-8d      %-lu\n", info_p->dev_name,
               info_p->iova, info_p->iova + info_p->size, info_p->size, info_p->timestamp);
    }
    printf("\n");

    printf("iova that has been freed===================================================\n");
    printf("device              iova_start     iova_end      size(byte)    timestamp(s)\n");
    for (info_p = info_head_free; info_p != NULL; info_p = info_p->next)
    {
        printf("%-16s    %8lx       %8lx      %-8d      %-lu\n", info_p->dev_name,
               info_p->iova, info_p->iova + info_p->size, info_p->size, info_p->timestamp);
    }
    printf("\n");

    return 0;
}

static void sunxi_iommu_alloc_iova(void *p, char *dev_name,
                                   unsigned long iova, size_t size)
{
    struct sunxi_iommu_iova_info *info_p;

    if (info_head == NULL)
    {
        info_head = info_p = hal_malloc(sizeof(*info_head));
        info_tail = info_head;
    }
    else
    {
        info_p = hal_malloc(sizeof(*info_head));
        info_tail->next = info_p;
        info_tail = info_p;
    }
    strcpy(info_p->dev_name, dev_name);
    info_p->iova = iova;
    info_p->size = size;
}

static void sunxi_iommu_free_iova(void *p, char *dev_name,
                                  unsigned long iova, size_t size)
{
    struct sunxi_iommu_iova_info *info_p, *info_prev, *info_p_free;

    info_p = info_prev = info_head;
    for (; info_p != NULL; info_p = info_p->next)
    {
        if (info_p->iova == iova && info_p->size == size)
        {
            break;
        }
        info_prev = info_p;
    }
    hal_assert(info_p != NULL);

    if (info_head_free == NULL)
    {
        info_head_free = hal_malloc(sizeof(*info_head_free));
        info_p_free = info_tail_free = info_head_free;
    }
    else
    {
        info_p_free = hal_malloc(sizeof(*info_head_free));
        info_tail_free->next = info_p_free;
        info_tail_free = info_p_free;
    }
    strcpy(info_p_free->dev_name, dev_name);
    info_p_free->iova = info_p->iova;
    info_p_free->size = info_p->size;

    if (info_p == info_head)                // free head
    {
        info_head = info_p->next;
    }
    else if (info_p == info_tail)           // free tail
    {
        info_tail = info_prev;
        info_tail->next = NULL;
    }
    else                                    // free middle
    {
        info_prev->next = info_p->next;
    }

    hal_free(info_p);
    info_p = NULL;
}

#endif

static inline uint32_t *iopde_offset(uint32_t *iopd, unsigned int iova)
{
    return iopd + IOPDE_INDEX(iova);
}

static inline uint32_t *iopte_offset(uint32_t *ent, unsigned int iova)
{
    unsigned long iopte_base = 0;

    iopte_base = IOPTE_BASE(*ent);

    return (uint32_t *)__pa_to_va(iopte_base) + IOPTE_INDEX(iova);
}

static inline uint32_t sunxi_iommu_read(struct sunxi_iommu_dev *iommu,
                                        uint32_t offset)
{
    return readl(iommu->base + offset);
}

static inline void sunxi_iommu_write(struct sunxi_iommu_dev *iommu,
                                     uint32_t offset, uint32_t value)
{
    writel(value, iommu->base + offset);
}

void sunxi_reset_device_iommu(unsigned int master_id)
{
    unsigned int regval;
    struct sunxi_iommu_dev *iommu = global_iommu_dev;

    regval = sunxi_iommu_read(iommu, IOMMU_RESET_REG);
    sunxi_iommu_write(iommu, IOMMU_RESET_REG, regval & (~(1 << master_id)));
    regval = sunxi_iommu_read(iommu, IOMMU_RESET_REG);
    if (!(regval & ((1 << master_id))))
    {
        sunxi_iommu_write(iommu, IOMMU_RESET_REG, regval | ((1 << master_id)));
    }
}

void sunxi_enable_device_iommu(unsigned int master_id, bool flag)
{
    struct sunxi_iommu_dev *iommu = global_iommu_dev;
    unsigned long mflag;

    mflag = hal_spin_lock_irqsave(&iommu->iommu_lock);
    if (flag)
    {
        iommu->bypass &= ~(master_id_bitmap[master_id]);
    }
    else
    {
        iommu->bypass |= master_id_bitmap[master_id];
    }
    sunxi_iommu_write(iommu, IOMMU_BYPASS_REG, iommu->bypass);
    hal_spin_unlock_irqrestore(&iommu->iommu_lock, mflag);
}

static int sunxi_tlb_flush(struct sunxi_iommu_dev *iommu)
{
    int ret;

    /* enable the maximum number(7) of master to fit all platform */
    sunxi_iommu_write(iommu, IOMMU_TLB_FLUSH_ENABLE_REG, 0x0003007f);
    ret = sunxi_wait_when(
              (sunxi_iommu_read(iommu, IOMMU_TLB_FLUSH_ENABLE_REG)), 2);
    if (ret)
    {
        printf("Enable flush all request timed out\n");
    }

    return ret;
}

static int sunxi_iommu_hw_init(struct sunxi_iommu_domain *sunxi_domain)
{
    int ret = 0;
    int iommu_enable = 0;
    unsigned long dte_addr;
    unsigned long mflag;
    struct sunxi_iommu_dev *iommu = global_iommu_dev;
    const struct sunxi_iommu_plat_data *plat_data = iommu->plat_data;

    mflag = hal_spin_lock_irqsave(&iommu->iommu_lock);
    dte_addr = __va_to_pa((unsigned long)sunxi_domain->pgtable);
    sunxi_iommu_write(iommu, IOMMU_TTB_REG, dte_addr);

    /*
     * set preftech functions, including:
     * master prefetching and only prefetch valid page to TLB/PTW
     */
    sunxi_iommu_write(iommu, IOMMU_TLB_PREFETCH_REG, plat_data->tlb_prefetch);
    /* new TLB invalid function: use range(start, end) to invalid TLB, started at version V12 */
    if (plat_data->version >= IOMMU_VERSION_V12)
    {
        sunxi_iommu_write(iommu, IOMMU_TLB_IVLD_MODE_SEL_REG, plat_data->tlb_invalid_mode);
    }
    /* new PTW invalid function: use range(start, end) to invalid PTW, started at version V14 */
    if (plat_data->version >= IOMMU_VERSION_V14)
    {
        sunxi_iommu_write(iommu, IOMMU_PC_IVLD_MODE_SEL_REG, plat_data->ptw_invalid_mode);
    }

    /* disable interrupt of prefetch */
    sunxi_iommu_write(iommu, IOMMU_INT_ENABLE_REG, 0x3003f);
    sunxi_iommu_write(iommu, IOMMU_BYPASS_REG, iommu->bypass);

    ret = sunxi_tlb_flush(iommu);
    if (ret)
    {
        printf("Enable flush all request timed out\n");
        goto out;
    }
    sunxi_iommu_write(iommu, IOMMU_AUTO_GATING_REG, 0x1);
    sunxi_iommu_write(iommu, IOMMU_ENABLE_REG, IOMMU_ENABLE);
    iommu_enable = sunxi_iommu_read(iommu, IOMMU_ENABLE_REG);
    if (iommu_enable != 0x1)
    {
        iommu_enable = sunxi_iommu_read(iommu, IOMMU_ENABLE_REG);
        if (iommu_enable != 0x1)
        {
            printf("iommu enable failed! No iommu in bitfile!\n");
            ret = -ENODEV;
            goto out;
        }
    }
    iommu_hw_init_flag = true;

out:
    hal_spin_unlock_irqrestore(&iommu->iommu_lock, mflag);

    return ret;
}

static int sunxi_tlb_invalid(unsigned long iova, unsigned long iova_mask)
{
    struct sunxi_iommu_dev *iommu = global_iommu_dev;
    const struct sunxi_iommu_plat_data *plat_data = iommu->plat_data;
    unsigned long iova_end = iova_mask;
    int ret = 0;
    unsigned long mflag;

    mflag = hal_spin_lock_irqsave(&iommu->iommu_lock);
    /* new TLB invalid function: use range(start, end) to invalid TLB page */
    if (plat_data->version >= IOMMU_VERSION_V12)
    {
        hal_log_debug("iommu: TLB invalid:0x%x-0x%x\n", (unsigned int)iova,
                      (unsigned int)iova_end);
        sunxi_iommu_write(iommu, IOMMU_TLB_IVLD_START_ADDR_REG, iova);
        sunxi_iommu_write(iommu, IOMMU_TLB_IVLD_END_ADDR_REG, iova_end);
    }
    else
    {
        /* old TLB invalid function: only invalid 4K at one time */
        sunxi_iommu_write(iommu, IOMMU_TLB_IVLD_ADDR_REG, iova);
        sunxi_iommu_write(iommu, IOMMU_TLB_IVLD_ADDR_MASK_REG, iova_mask);
    }
    sunxi_iommu_write(iommu, IOMMU_TLB_IVLD_ENABLE_REG, 0x1);

    ret = sunxi_wait_when(
              (sunxi_iommu_read(iommu, IOMMU_TLB_IVLD_ENABLE_REG) & 0x1), 2);
    if (ret)
    {
        printf("TLB cache invalid timed out\n");
    }
    hal_spin_unlock_irqrestore(&iommu->iommu_lock, mflag);

    return ret;
}

static int sunxi_ptw_cache_invalid(unsigned long iova_start, unsigned long iova_end)
{
    struct sunxi_iommu_dev *iommu = global_iommu_dev;
    const struct sunxi_iommu_plat_data *plat_data = iommu->plat_data;
    int ret = 0;
    unsigned long mflag;

    mflag = hal_spin_lock_irqsave(&iommu->iommu_lock);
    /* new PTW invalid function: use range(start, end) to invalid PTW page */
    if (plat_data->version >= IOMMU_VERSION_V14)
    {
        hal_log_debug("iommu: PTW invalid:0x%x-0x%x\n", (unsigned int)iova_start,
                      (unsigned int)iova_end);
        WARN_ON(iova_end == 0);
        sunxi_iommu_write(iommu, IOMMU_PC_IVLD_START_ADDR_REG, iova_start);
        sunxi_iommu_write(iommu, IOMMU_PC_IVLD_END_ADDR_REG, iova_end);
    }
    else
    {
        /* old ptw invalid function: only invalid 1M at one time */
        hal_log_debug("iommu: PTW invalid:0x%x\n", (unsigned int)iova_start);
        sunxi_iommu_write(iommu, IOMMU_PC_IVLD_ADDR_REG, iova_start);
    }
    sunxi_iommu_write(iommu, IOMMU_PC_IVLD_ENABLE_REG, 0x1);

    ret = sunxi_wait_when(
              (sunxi_iommu_read(iommu, IOMMU_PC_IVLD_ENABLE_REG) & 0x1), 2);
    if (ret)
    {
        printf("PTW cache invalid timed out\n");
        goto out;
    }

out:
    hal_spin_unlock_irqrestore(&iommu->iommu_lock, mflag);

    return ret;
}

static int sunxi_alloc_iopte(uint32_t *sent, int prot)
{
    uint32_t *pent;
    uint32_t flags = 0;

    flags |= (prot & IOMMU_READ) ? DENT_READABLE : 0;
    flags |= (prot & IOMMU_WRITE) ? DENT_WRITABLE : 0;

    pent = hal_malloc_align(PT_SIZE, PT_SIZE);
    WARN_ON((unsigned long)pent & (PT_SIZE - 1));
    if (!pent)
    {
        printf("%s, %d, kmalloc failed!\n", __func__, __LINE__);
        return 0;
    }
    memset(pent, 0, PT_SIZE);
    hal_dcache_clean_invalidate(ALIGN_DOWN((unsigned long)pent, CACHELINE_LEN), PT_SIZE);
    hal_dcache_clean_invalidate(ALIGN_DOWN((unsigned long)sent, CACHELINE_LEN), sizeof(*sent));
    *sent = __va_to_pa((unsigned long)pent) | DENT_VALID;
    hal_dcache_clean_invalidate(ALIGN_DOWN((unsigned long)sent, CACHELINE_LEN), sizeof(*sent));

    return 1;
}

static void sunxi_free_iopte(uint32_t *pent)
{
    hal_free_align(pent);
}

void sunxi_zap_tlb(unsigned long iova, size_t size)
{
    const struct sunxi_iommu_plat_data *plat_data = global_iommu_dev->plat_data;

    if (plat_data->version <= IOMMU_VERSION_V11)
    {
        sunxi_tlb_invalid(iova, (uint32_t)IOMMU_PT_MASK);
        sunxi_tlb_invalid(iova + SPAGE_SIZE, (uint32_t)IOMMU_PT_MASK);
        sunxi_tlb_invalid(iova + size, (uint32_t)IOMMU_PT_MASK);
        sunxi_tlb_invalid(iova + size + SPAGE_SIZE, (uint32_t)IOMMU_PT_MASK);
        sunxi_ptw_cache_invalid(iova, 0);
        sunxi_ptw_cache_invalid(iova + SPD_SIZE, 0);
        sunxi_ptw_cache_invalid(iova + size, 0);
        sunxi_ptw_cache_invalid(iova + size + SPD_SIZE, 0);
    }
    else if (plat_data->version <= IOMMU_VERSION_V13)
    {
        sunxi_tlb_invalid(iova, iova + 2 * SPAGE_SIZE);
        sunxi_tlb_invalid(iova + size - SPAGE_SIZE, iova + size + 8 * SPAGE_SIZE);
        sunxi_ptw_cache_invalid(iova, 0);
        sunxi_ptw_cache_invalid(iova + size, 0);

        sunxi_ptw_cache_invalid(iova + SPD_SIZE, 0);
        sunxi_ptw_cache_invalid(iova + size + SPD_SIZE, 0);
        sunxi_ptw_cache_invalid(iova + size + 2 * SPD_SIZE, 0);
    }
    else
    {
        sunxi_tlb_invalid(iova, iova + 2 * SPAGE_SIZE);
        sunxi_tlb_invalid(iova + size - SPAGE_SIZE, iova + size + 8 * SPAGE_SIZE);
        sunxi_ptw_cache_invalid(iova, iova + SPD_SIZE);
        sunxi_ptw_cache_invalid(iova + size - SPD_SIZE, iova + size);
    }

    return;
}

static inline uint32_t sunxi_mk_pte(uint32_t page, int prot)
{
    uint32_t flags = 0;

    flags |= (prot & IOMMU_READ) ? SUNXI_PTE_PAGE_READABLE : 0;
    flags |= (prot & IOMMU_WRITE) ? SUNXI_PTE_PAGE_WRITABLE : 0;
    page &= IOMMU_PT_MASK;

    return page | flags | SUNXI_PTE_PAGE_VALID;
}

static int sunxi_iommu_map(unsigned long iova,
                           unsigned long paddr, size_t size, int prot)
{
    struct sunxi_iommu_domain *sunxi_domain;
    size_t iova_start, iova_end, paddr_start, s_iova_start;
    uint32_t *dent, *pent;
    int i;
    uint32_t iova_tail_count, iova_tail_size;
    uint32_t pent_val;

    sunxi_domain = global_sunxi_iommu_domain;
    WARN_ON(sunxi_domain->pgtable == NULL);
    iova_start = iova & IOMMU_PT_MASK;
    paddr_start = paddr & IOMMU_PT_MASK;
    iova_end = SPAGE_ALIGN(iova + size);
    s_iova_start = iova_start;

    hal_mutex_lock(&sunxi_domain->dt_lock);
    for (; iova_start <= iova_end; iova_start += SPD_SIZE)
    {
        dent = iopde_offset(sunxi_domain->pgtable, iova_start);
        if (!IS_VALID(*dent) && !sunxi_alloc_iopte(dent, prot))
        {
            hal_mutex_unlock(&sunxi_domain->dt_lock);
            return -ENOMEM;
        }
    }

    iova_start = s_iova_start;
    for (; iova_start < iova_end;)
    {
        iova_tail_count = NUM_ENTRIES_PTE - IOPTE_INDEX(iova_start);
        iova_tail_size = iova_tail_count * SPAGE_SIZE;
        if (iova_start + iova_tail_size > iova_end)
        {
            iova_tail_size = iova_end - iova_start;
            iova_tail_count = iova_tail_size / SPAGE_SIZE;
        }

        dent = iopde_offset(sunxi_domain->pgtable, iova_start);
        pent = iopte_offset(dent, iova_start);
        pent_val = sunxi_mk_pte(paddr_start, prot);
        for (i = 0; i < iova_tail_count; i++)
        {
            WARN_ON(*pent);
            *pent = pent_val +  SPAGE_SIZE * i;
            pent++;
        }

        hal_dcache_clean_invalidate(ALIGN_DOWN((unsigned long)iopte_offset(dent, iova_start), CACHELINE_LEN), (iova_tail_count << 2) + 64);
        iova_start += iova_tail_size;
        paddr_start += iova_tail_size;
    }
    hal_mutex_unlock(&sunxi_domain->dt_lock);

    return 0;
}

static size_t sunxi_iommu_unmap(unsigned long iova,
                                size_t size)
{
    struct sunxi_iommu_domain *sunxi_domain;
    const struct sunxi_iommu_plat_data *plat_data;
    size_t iova_start, iova_end;
    uint32_t *dent, *pent;
    uint32_t iova_tail_count, iova_tail_size;

    sunxi_domain = global_sunxi_iommu_domain;
    plat_data = global_iommu_dev->plat_data;
    WARN_ON(sunxi_domain->pgtable == NULL);
    iova_start = iova & IOMMU_PT_MASK;
    iova_end = SPAGE_ALIGN(iova + size);

    hal_mutex_lock(&sunxi_domain->dt_lock);
    /* Invalid TLB and PTW */
    if (plat_data->version >= IOMMU_VERSION_V12)
    {
        sunxi_tlb_invalid(iova_start, iova_end);
    }
    if (plat_data->version >= IOMMU_VERSION_V14)
    {
        sunxi_ptw_cache_invalid(iova_start, iova_end);
    }

    for (; iova_start < iova_end;)
    {
        iova_tail_count = NUM_ENTRIES_PTE - IOPTE_INDEX(iova_start);
        iova_tail_size = iova_tail_count * SPAGE_SIZE;
        if (iova_start + iova_tail_size > iova_end)
        {
            iova_tail_size = iova_end - iova_start;
            iova_tail_count = iova_tail_size / SPAGE_SIZE;
        }

        dent = iopde_offset(sunxi_domain->pgtable, iova_start);
        if (!IS_VALID(*dent))
        {
            return -EINVAL;
        }
        pent = iopte_offset(dent, iova_start);
        memset(pent, 0, iova_tail_count * sizeof(uint32_t));

        hal_dcache_clean_invalidate(ALIGN_DOWN((unsigned long)iopte_offset(dent, iova_start), CACHELINE_LEN), iova_tail_count << 2);
        if (iova_tail_size == SPD_SIZE)
        {
            *dent = 0;
            hal_dcache_clean_invalidate(ALIGN_DOWN((unsigned long)dent, CACHELINE_LEN), sizeof(*dent));
            sunxi_free_iopte(pent);
        }

        if (plat_data->version < IOMMU_VERSION_V14)
        {
            sunxi_ptw_cache_invalid(iova_start, 0);
        }
        iova_start += iova_tail_size;
    }
    hal_mutex_unlock(&sunxi_domain->dt_lock);

    return size;
}

void sunxi_iommu_iotlb_sync_map(
    unsigned long iova, size_t size)
{
    struct sunxi_iommu_domain *sunxi_domain = global_sunxi_iommu_domain;

    hal_mutex_lock(&sunxi_domain->dt_lock);
    sunxi_zap_tlb(iova, size);
    hal_mutex_unlock(&sunxi_domain->dt_lock);

    return;
}

static unsigned long sunxi_iommu_iova_to_phys(unsigned long iova)
{
    struct sunxi_iommu_domain *sunxi_domain = global_sunxi_iommu_domain;
    uint32_t *dent, *pent;
    unsigned long ret = 0;

    hal_assert(sunxi_domain->pgtable != NULL);
    hal_mutex_lock(&sunxi_domain->dt_lock);
    dent = iopde_offset(sunxi_domain->pgtable, iova);
    if (IS_VALID(*dent))
    {
        pent = iopte_offset(dent, iova);
        ret = IOPTE_TO_PFN(pent) + IOVA_PAGE_OFT(iova);

        if (ret < SUNXI_PHYS_OFFSET)
        {
            ret += SUNXI_4G_PHYS_OFFSET;
        }

    }
    hal_mutex_unlock(&sunxi_domain->dt_lock);

    return ret;
}

static char sunxi_iommu_pgtable[PD_SIZE] __attribute__((aligned(16384)));

static struct sunxi_iommu_domain *sunxi_iommu_domain_alloc(void)
{
    struct sunxi_iommu_domain *sunxi_domain;

    /* we just use one domain */
    if (global_sunxi_iommu_domain)
    {
        return global_sunxi_iommu_domain;
    }

    sunxi_domain = hal_malloc(sizeof(*sunxi_domain));

    if (!sunxi_domain)
    {
        return NULL;
    }

    memset(sunxi_domain, 0, sizeof(*sunxi_domain));
    sunxi_domain->pgtable = (unsigned int *) sunxi_iommu_pgtable;
    if (!sunxi_domain->pgtable)
    {
        printf("sunxi domain get pgtable failed\n");
        goto err_page;
    }
    memset(sunxi_domain->pgtable, 0, PD_SIZE);

    sunxi_domain->sg_buffer = (unsigned int *)hal_malloc(MAX_SG_TABLE_SIZE);
    if (!sunxi_domain->sg_buffer)
    {
        printf("sunxi domain get sg_buffer failed\n");
        goto err_sg_buffer;
    }
    memset(sunxi_domain->sg_buffer, 0, MAX_SG_TABLE_SIZE);

    hal_mutex_init(&sunxi_domain->dt_lock);
    global_sunxi_iommu_domain = sunxi_domain;

    if (!iommu_hw_init_flag)
    {
        if (sunxi_iommu_hw_init(sunxi_domain))
        {
            printf("sunxi iommu hardware init failed\n");
        }
    }

    return sunxi_domain;

err_sg_buffer:
    sunxi_domain->pgtable = NULL;
err_page:
    hal_free(sunxi_domain);

    return NULL;
}

static void sunxi_iommu_domain_free(struct iommu_domain *domain)
{
    int i = 0;
    size_t iova;
    uint32_t *dent, *pent;

    struct sunxi_iommu_domain *sunxi_domain = global_sunxi_iommu_domain;

    hal_mutex_lock(&sunxi_domain->dt_lock);
    for (i = 0; i < NUM_ENTRIES_PDE; ++i)
    {
        dent = sunxi_domain->pgtable + i;
        iova = i << IOMMU_PD_SHIFT;
        if (IS_VALID(*dent))
        {
            pent = iopte_offset(dent, iova);
            hal_dcache_clean_invalidate(ALIGN_DOWN((unsigned long)pent, CACHELINE_LEN), PT_SIZE);
            memset(pent, 0, PT_SIZE);
            hal_dcache_clean_invalidate(ALIGN_DOWN((unsigned long)pent, CACHELINE_LEN), PT_SIZE);
            hal_dcache_clean_invalidate(ALIGN_DOWN((unsigned long)dent, CACHELINE_LEN), PT_SIZE);
            *dent = 0;
            hal_dcache_clean_invalidate(ALIGN_DOWN((unsigned long)dent, CACHELINE_LEN), sizeof(*dent));
            sunxi_free_iopte(pent);
        }
    }
    sunxi_tlb_flush(global_iommu_dev);
    hal_mutex_unlock(&sunxi_domain->dt_lock);
    sunxi_domain->pgtable = NULL;
    hal_free(sunxi_domain->sg_buffer);
    sunxi_domain->sg_buffer = NULL;
    hal_free(sunxi_domain);
}

static int sunxi_iommu_attach_dev(void)
{
    return 0;
}

static void sunxi_iommu_detach_dev(void)
{
    return;
}

static int sunxi_iommu_probe_device(void)
{
    sunxi_enable_device_iommu(0, 0);
    return 0;
}

static void sunxi_iommu_release_device(void)
{
    sunxi_enable_device_iommu(0, false);
}

/* set dma params for master devices */
int sunxi_iommu_set_dma_parms(void *nb,
                              unsigned long action, void *data)
{
    return 0;
}

void sunxi_set_debug_mode(void)
{
    struct sunxi_iommu_dev *iommu = global_iommu_dev;

    sunxi_iommu_write(iommu,
                      IOMMU_VA_CONFIG_REG, 0x80000000);
}

void sunxi_set_prefetch_mode(void)
{
    struct sunxi_iommu_dev *iommu = global_iommu_dev;

    sunxi_iommu_write(iommu,
                      IOMMU_VA_CONFIG_REG, 0x00000000);
}

int sunxi_iova_test_write(unsigned long iova, uint32_t val)
{
    struct sunxi_iommu_dev *iommu = global_iommu_dev;
    int retval;

    sunxi_iommu_write(iommu, IOMMU_VA_REG, iova);
    sunxi_iommu_write(iommu, IOMMU_VA_DATA_REG, val);
    sunxi_iommu_write(iommu,
                      IOMMU_VA_CONFIG_REG, 0x80000100);
    sunxi_iommu_write(iommu,
                      IOMMU_VA_CONFIG_REG, 0x80000101);
    retval = sunxi_wait_when((sunxi_iommu_read(iommu,
                              IOMMU_VA_CONFIG_REG) & 0x1), 1);
    if (retval)
    {
        printf("write VA address request timed out\n");
    }
    return retval;
}

unsigned long sunxi_iova_test_read(unsigned long iova)
{
    struct sunxi_iommu_dev *iommu = global_iommu_dev;
    unsigned long retval;

    sunxi_iommu_write(iommu, IOMMU_VA_REG, iova);
    sunxi_iommu_write(iommu,
                      IOMMU_VA_CONFIG_REG, 0x80000000);
    sunxi_iommu_write(iommu,
                      IOMMU_VA_CONFIG_REG, 0x80000001);
    retval = sunxi_wait_when((sunxi_iommu_read(iommu,
                              IOMMU_VA_CONFIG_REG) & 0x1), 1);
    if (retval)
    {
        printf("read VA address request timed out\n");
        retval = 0;
        goto out;
    }
    retval = sunxi_iommu_read(iommu,
                              IOMMU_VA_DATA_REG);

out:
    return retval;
}

static int sunxi_iova_invalid_helper(unsigned long iova)
{
    struct sunxi_iommu_domain *sunxi_domain = global_sunxi_iommu_domain;
    uint32_t *pte_addr, *dte_addr;

    dte_addr = iopde_offset(sunxi_domain->pgtable, iova);
    if ((*dte_addr & 0x3) != 0x1)
    {
        printf("0x%lx is not mapped!\n", iova);
        return 1;
    }
    pte_addr = iopte_offset(dte_addr, iova);
    if ((*pte_addr & 0x2) == 0)
    {
        printf("0x%lx is not mapped!\n", iova);
        return 1;
    }
    printf("0x%lx is mapped!\n", iova);

    return 0;
}

static hal_irqreturn_t sunxi_iommu_irq(void *dev_id)
{
    uint32_t inter_status_reg = 0;
    uint32_t addr_reg = 0;
    uint32_t int_masterid_bitmap = 0;
    uint32_t data_reg = 0;
    uint32_t l1_pgint_reg = 0;
    uint32_t l2_pgint_reg = 0;
    uint32_t master_id = 0;
    unsigned long mflag;
    struct sunxi_iommu_dev *iommu = dev_id;
    const struct sunxi_iommu_plat_data *plat_data = iommu->plat_data;

    mflag = hal_spin_lock_irqsave(&iommu->iommu_lock);
    inter_status_reg = sunxi_iommu_read(iommu, IOMMU_INT_STA_REG) & 0x3ffff;
    l1_pgint_reg = sunxi_iommu_read(iommu, IOMMU_L1PG_INT_REG);
    l2_pgint_reg = sunxi_iommu_read(iommu, IOMMU_L2PG_INT_REG);
    int_masterid_bitmap = inter_status_reg | l1_pgint_reg | l2_pgint_reg;

    if (inter_status_reg & MICRO_TLB0_INVALID_INTER_MASK)
    {
        printf("%s Invalid Authority\n", plat_data->master[0]);
        addr_reg = sunxi_iommu_read(iommu, IOMMU_INT_ERR_ADDR_REG0);
        data_reg = sunxi_iommu_read(iommu, IOMMU_INT_ERR_DATA_REG0);
    }
    else if (inter_status_reg & MICRO_TLB1_INVALID_INTER_MASK)
    {
        printf("%s Invalid Authority\n", plat_data->master[1]);
        addr_reg = sunxi_iommu_read(iommu, IOMMU_INT_ERR_ADDR_REG1);
        data_reg = sunxi_iommu_read(iommu, IOMMU_INT_ERR_DATA_REG1);
    }
    else if (inter_status_reg & MICRO_TLB2_INVALID_INTER_MASK)
    {
        printf("%s Invalid Authority\n", plat_data->master[2]);
        addr_reg = sunxi_iommu_read(iommu, IOMMU_INT_ERR_ADDR_REG2);
        data_reg = sunxi_iommu_read(iommu, IOMMU_INT_ERR_DATA_REG2);
    }
    else if (inter_status_reg & MICRO_TLB3_INVALID_INTER_MASK)
    {
        printf("%s Invalid Authority\n", plat_data->master[3]);
        addr_reg = sunxi_iommu_read(iommu, IOMMU_INT_ERR_ADDR_REG3);
        data_reg = sunxi_iommu_read(iommu, IOMMU_INT_ERR_DATA_REG3);
    }
    else if (inter_status_reg & MICRO_TLB4_INVALID_INTER_MASK)
    {
        printf("%s Invalid Authority\n", plat_data->master[4]);
        addr_reg = sunxi_iommu_read(iommu, IOMMU_INT_ERR_ADDR_REG4);
        data_reg = sunxi_iommu_read(iommu, IOMMU_INT_ERR_DATA_REG4);
    }
    else if (inter_status_reg & MICRO_TLB5_INVALID_INTER_MASK)
    {
        printf("%s Invalid Authority\n", plat_data->master[5]);
        addr_reg = sunxi_iommu_read(iommu, IOMMU_INT_ERR_ADDR_REG5);
        data_reg = sunxi_iommu_read(iommu, IOMMU_INT_ERR_DATA_REG5);
    }
    else if (inter_status_reg & MICRO_TLB6_INVALID_INTER_MASK)
    {
        printf("%s Invalid Authority\n", plat_data->master[6]);
        addr_reg = sunxi_iommu_read(iommu, IOMMU_INT_ERR_ADDR_REG6);
        data_reg = sunxi_iommu_read(iommu, IOMMU_INT_ERR_DATA_REG6);
    }
    else if (inter_status_reg & L1_PAGETABLE_INVALID_INTER_MASK)
    {
        /*It's OK to prefetch an invalid page, no need to print msg for debug.*/
        if (!(int_masterid_bitmap & (1U << 31)))
        {
            printf("L1 PageTable Invalid\n");
        }
        addr_reg = sunxi_iommu_read(iommu, IOMMU_INT_ERR_ADDR_REG7);
        data_reg = sunxi_iommu_read(iommu, IOMMU_INT_ERR_DATA_REG7);
    }
    else if (inter_status_reg & L2_PAGETABLE_INVALID_INTER_MASK)
    {
        if (!(int_masterid_bitmap & (1U << 31)))
        {
            printf("L2 PageTable Invalid\n");
        }
        addr_reg = sunxi_iommu_read(iommu, IOMMU_INT_ERR_ADDR_REG8);
        data_reg = sunxi_iommu_read(iommu, IOMMU_INT_ERR_DATA_REG8);
    }
    else
    {
        printf("sunxi iommu int error!!!\n");
    }

    if (!(int_masterid_bitmap & (1U << 31)))
    {
        sunxi_iova_invalid_helper(addr_reg);
        int_masterid_bitmap &= 0xffff;
        master_id = ffs(int_masterid_bitmap) - 1;
        printf("Bug is in %s module, invalid address: 0x%x, data:0x%x, id:0x%x\n",
               plat_data->master[master_id], addr_reg, data_reg,
               int_masterid_bitmap);

#ifdef CONFIG_DRIVERS_AW_IOMMU_TRACE
        iova_show_on_irq();
#endif
        /* master debug callback */
        if (sunxi_iommu_fault_notify_cbs[master_id])
        {
            sunxi_iommu_fault_notify_cbs[master_id]();
        }
    }

    /* invalid TLB */
    if (plat_data->version <= IOMMU_VERSION_V11)
    {
        sunxi_iommu_write(iommu, IOMMU_TLB_IVLD_ADDR_REG, addr_reg);
        sunxi_iommu_write(iommu, IOMMU_TLB_IVLD_ADDR_MASK_REG, (uint32_t)IOMMU_PT_MASK);
        sunxi_iommu_write(iommu, IOMMU_TLB_IVLD_ENABLE_REG, 0x1);
        while (sunxi_iommu_read(iommu, IOMMU_TLB_IVLD_ENABLE_REG) & 0x1)
            ;
        sunxi_iommu_write(iommu, IOMMU_TLB_IVLD_ADDR_REG, addr_reg + 0x2000);
        sunxi_iommu_write(iommu, IOMMU_TLB_IVLD_ADDR_MASK_REG, (uint32_t)IOMMU_PT_MASK);
    }
    else
    {
        sunxi_iommu_write(iommu, IOMMU_TLB_IVLD_START_ADDR_REG, addr_reg);
        sunxi_iommu_write(iommu, IOMMU_TLB_IVLD_END_ADDR_REG, addr_reg + 4 * SPAGE_SIZE);
    }
    sunxi_iommu_write(iommu, IOMMU_TLB_IVLD_ENABLE_REG, 0x1);
    while (sunxi_iommu_read(iommu, IOMMU_TLB_IVLD_ENABLE_REG) & 0x1)
        ;

    /* invalid PTW */
    if (plat_data->version <= IOMMU_VERSION_V13)
    {
        sunxi_iommu_write(iommu, IOMMU_PC_IVLD_ADDR_REG, addr_reg);
        sunxi_iommu_write(iommu, IOMMU_PC_IVLD_ENABLE_REG, 0x1);
        while (sunxi_iommu_read(iommu, IOMMU_PC_IVLD_ENABLE_REG) & 0x1)
            ;
        sunxi_iommu_write(iommu, IOMMU_PC_IVLD_ADDR_REG, addr_reg + 0x200000);
    }
    else
    {
        sunxi_iommu_write(iommu, IOMMU_PC_IVLD_START_ADDR_REG, addr_reg);
        sunxi_iommu_write(iommu, IOMMU_PC_IVLD_END_ADDR_REG, addr_reg + 2 * SPD_SIZE);
    }
    sunxi_iommu_write(iommu, IOMMU_PC_IVLD_ENABLE_REG, 0x1);
    while (sunxi_iommu_read(iommu, IOMMU_PC_IVLD_ENABLE_REG) & 0x1)
        ;

    sunxi_iommu_write(iommu, IOMMU_INT_CLR_REG, inter_status_reg);
    inter_status_reg |= (l1_pgint_reg | l2_pgint_reg);
    inter_status_reg &= 0xffff;
    sunxi_iommu_write(iommu, IOMMU_RESET_REG, ~inter_status_reg);
    sunxi_iommu_write(iommu, IOMMU_RESET_REG, 0xffffffff);
    hal_spin_unlock_irqrestore(&iommu->iommu_lock, mflag);

    return 0;
}

static int sunxi_iommu_suspend(void)
{
    return 0;
}

static int sunxi_iommu_resume(void)
{
    int err = -1;
    return err;
}

static int sunxi_iommu_clk_init(void)
{
    hal_clk_status_t ret;

    hal_clk_t bus_clk = hal_clock_get(HAL_SUNXI_CCU, CLK_BUS_IOMMU);

    ret = hal_clock_enable(bus_clk);
    if (ret)
    {
        return -1;
    }

    return 0;
}

static LIST_HEAD(g_kernel_vm_list);
static LIST_HEAD(g_dma_vm_list);
static LIST_HEAD(g_ion_handle_list);
static hal_spinlock_t iommu_vma_lock;

static struct iommu_vm_struct init_kernel_vma =
{
    .start_addr = CONFIG_DRIVERS_IOMMU_KERNEL_START_ADDRESS,
    .size = CONFIG_DRIVERS_IOMMU_KERNEL_AREA_SIZE,
};

static struct iommu_vm_struct init_dma_vma =
{
    .start_addr = CONFIG_DRIVERS_IOMMU_DMA_START_ADDRESS,
    .size = CONFIG_DRIVERS_IOMMU_KERNEL_AREA_SIZE,
};

static struct iommu_vm_struct init_dma_vma;

void unmap_vmap_area(unsigned long vmaddr, unsigned long free_sz, void *page);
int map_vm_area(unsigned long size, unsigned long prot, void *pages[], unsigned long target_vmaddr);

static struct iommu_vm_struct *alloc_vm(int size, struct list_head *head)
{
    unsigned long cpsr;
    struct iommu_vm_struct *tmp;
    struct iommu_vm_struct *vma;

    struct list_head *pos;
    struct list_head *q;

    vma = hal_malloc(sizeof(struct iommu_vm_struct));
    if (!vma)
    {
        return NULL;
    }
    memset(vma, 0, sizeof(struct iommu_vm_struct));
    INIT_LIST_HEAD(&vma->i_list);

    cpsr = hal_spin_lock_irqsave(&iommu_vma_lock);
    list_for_each_safe(pos, q, head)
    {
        tmp = list_entry(pos, struct iommu_vm_struct, i_list);
        if (tmp->size == size)
        {
            list_del(pos);
            hal_spin_unlock_irqrestore(&iommu_vma_lock, cpsr);
            hal_free(vma);
            return tmp;
        }
        else if (tmp->size > size)
        {
            vma->size = size;
            vma->start_addr = tmp->start_addr;
            tmp->size -= size;
            tmp->start_addr += size;
            hal_spin_unlock_irqrestore(&iommu_vma_lock, cpsr);
            return vma;
        }
    }
    hal_spin_unlock_irqrestore(&iommu_vma_lock, cpsr);
    return NULL;
}

static void free_vm(struct iommu_vm_struct *vma, struct list_head *head)
{
    unsigned long cpsr;
    struct iommu_vm_struct *tmp;

    struct list_head *pos;
    struct list_head *q;

    cpsr = hal_spin_lock_irqsave(&iommu_vma_lock);
    list_for_each_safe(pos, q, head)
    {
        tmp = list_entry(pos, struct iommu_vm_struct, i_list);
        if (tmp->start_addr == (vma->start_addr + vma->size))
        {
            tmp->start_addr = vma->start_addr;
            tmp->size += vma->size;
            hal_spin_unlock_irqrestore(&iommu_vma_lock, cpsr);
            hal_free(vma);
            return;
        }
        else if (tmp->start_addr > (vma->start_addr + vma->size))
        {
            list_add(&vma->i_list, pos);
            hal_spin_unlock_irqrestore(&iommu_vma_lock, cpsr);
            return;
        }
        else if (tmp->start_addr < (vma->start_addr + vma->size))
        {
            list_add_tail(&vma->i_list, pos);
            hal_spin_unlock_irqrestore(&iommu_vma_lock, cpsr);
            return;
        }
    }
    hal_spin_unlock_irqrestore(&iommu_vma_lock, cpsr);
}

static struct iommu_vm_struct *alloc_kernel_vm(int size)
{
    return alloc_vm(size, &g_kernel_vm_list);
}

static void free_kernel_vm(struct iommu_vm_struct *vma)
{
    free_vm(vma, &g_kernel_vm_list);
}

static struct iommu_vm_struct *alloc_dma_vm(int size)
{
    return alloc_vm(size, &g_dma_vm_list);
}

static void free_dma_vm(struct iommu_vm_struct *vma)
{
    free_vm(vma, &g_dma_vm_list);
}

struct ion_handle *ion_alloc(int size)
{
    int i;
    unsigned long mflag;
    struct ion_handle *handle;
    struct sunxi_iommu_dev *iommu = global_iommu_dev;

    handle = hal_malloc(sizeof(struct ion_handle));
    if (!handle)
    {
        return NULL;
    }
    memset(handle, 0, sizeof(struct ion_handle));
    handle->size = ALIGN_UP(size, PAGE_SIZE);
    handle->npages = handle->size / PAGE_SIZE;

    handle->pages = hal_malloc(handle->npages * sizeof(void *));
    if (!handle->pages)
    {
        hal_free(handle);
        return NULL;
    }
    memset(handle->pages, 0, handle->npages * sizeof(void *));

    for (i = 0; i < handle->npages; i++)
    {
        handle->pages[i] = hal_alloc_page(1);
        if (!handle->pages[i])
        {
            goto fail;
        }
    }

    INIT_LIST_HEAD(&handle->i_list);
    mflag = hal_spin_lock_irqsave(&iommu->iommu_lock);
    list_add(&handle->i_list, &g_ion_handle_list);
    hal_spin_unlock_irqrestore(&iommu->iommu_lock, mflag);

    return handle;
fail:
    for (i = 0; i < handle->npages; i++)
    {
        if (handle->pages[i])
        {
            hal_free_page(handle->pages[i], 1);
        }
    }
    hal_free(handle->pages);
    hal_free(handle);
    return NULL;
}

void ion_free(struct ion_handle *handle)
{
    int i;
    unsigned long mflag;
    struct sunxi_iommu_dev *iommu = global_iommu_dev;

    for (i = 0; i < handle->npages; i++)
    {
        if (handle->pages[i])
        {
            hal_free_page(handle->pages[i], 1);
        }
    }
    hal_free(handle->pages);

    mflag = hal_spin_lock_irqsave(&iommu->iommu_lock);
    list_del(&handle->i_list);
    hal_spin_unlock_irqrestore(&iommu->iommu_lock, mflag);

    hal_free(handle);
}

struct ion_handle *find_ion_handle_by_dma_addr(void *dma_addr)
{
    unsigned long mflag;
    struct ion_handle *tmp;
    struct list_head *pos;
    struct list_head *q;

    struct ion_handle *handle = NULL;
    struct sunxi_iommu_dev *iommu = global_iommu_dev;

    mflag = hal_spin_lock_irqsave(&iommu->iommu_lock);

    list_for_each_safe(pos, q, &g_ion_handle_list)
    {
        tmp = list_entry(pos, struct ion_handle, i_list);
        if (tmp->dma_vma->start_addr == (unsigned long)dma_addr)
        {
            handle = tmp;
            break;
        }
    }
    hal_spin_unlock_irqrestore(&iommu->iommu_lock, mflag);
    return handle;
}

struct ion_handle *find_ion_handle_by_kernel_addr(void *kernel_addr)
{
    unsigned long mflag;
    struct ion_handle *tmp;
    struct list_head *pos;
    struct list_head *q;

    struct ion_handle *handle = NULL;
    struct sunxi_iommu_dev *iommu = global_iommu_dev;

    mflag = hal_spin_lock_irqsave(&iommu->iommu_lock);

    list_for_each_safe(pos, q, &g_ion_handle_list)
    {
        tmp = list_entry(pos, struct ion_handle, i_list);
        if (tmp->kernel_vma->start_addr == (unsigned long)kernel_addr)
        {
            handle = tmp;
            break;
        }
    }
    hal_spin_unlock_irqrestore(&iommu->iommu_lock, mflag);
    return handle;
}

void *ion_map_kernel(struct ion_handle *handle)
{
    int i;
    struct iommu_vm_struct *vma;

    vma = alloc_kernel_vm(handle->size);
    if (!vma)
    {
        return NULL;
    }

    if (map_vm_area(handle->npages * PAGE_SIZE,
                    get_memory_prot(PAGE_MEM_ATTR_CACHEABLE),
                    (void *)handle->pages, vma->start_addr))
    {
        free_kernel_vm(vma);
        return NULL;
    }
    handle->kernel_vma = vma;
    return (void *)vma->start_addr;
}

void ion_unmap_kernel(struct ion_handle *handle)
{
    int i;
    struct iommu_vm_struct *vma;

    if (!handle->kernel_vma)
    {
        return;
    }

    vma = handle->kernel_vma;

    unmap_vmap_area(vma->start_addr, vma->size, (void *)handle->pages);
    free_kernel_vm(vma);
    handle->kernel_vma = NULL;
}

void *ion_map_dma(struct ion_handle *handle)
{
    int i;
    int ret;
    struct iommu_vm_struct *vma;

    vma = alloc_dma_vm(handle->size);
    if (!vma)
    {
        return NULL;
    }

    for (i = 0; i < handle->npages; i++)
    {
        ret = sunxi_iommu_map(vma->start_addr + PAGE_SIZE * i, __va_to_pa((unsigned long)handle->pages[i]), PAGE_SIZE, IOMMU_READ | IOMMU_WRITE);
        if (ret < 0)
        {
            return NULL;
        }
    }

    handle->dma_vma = vma;
    return (void *)vma->start_addr;
}

void ion_unmap_dma(struct ion_handle *handle)
{
    int i;
    int ret;
    struct iommu_vm_struct *vma;

    if (!handle->dma_vma)
    {
        return;
    }

    vma = handle->dma_vma;

    for (i = 0; i < handle->npages; i++)
    {
        ret = sunxi_iommu_unmap(vma->start_addr + PAGE_SIZE * i, PAGE_SIZE);
        if (ret < 0)
        {
            hal_log_err("iommu unmap failed\n");
            break;
        }
    }

    free_dma_vm(vma);
    handle->dma_vma = NULL;
}

int hal_iommu_init(void)
{
    struct sunxi_iommu_dev *sunxi_iommu;

    sunxi_iommu = hal_malloc(sizeof(struct sunxi_iommu_dev));
    if (!sunxi_iommu)
    {
        return -1;
    }

    sunxi_iommu->base = SUNXI_IOMMU_BASE;
    sunxi_iommu->bypass = DEFAULT_BYPASS_VALUE;
    sunxi_iommu->irq = SUNXI_IOMMU_IRQ_NUM;
    sunxi_iommu->plat_data = &iommu_data;

    global_iommu_dev = sunxi_iommu;
    hal_spin_lock_init(&sunxi_iommu->iommu_lock);
    hal_spin_lock_init(&iommu_vma_lock);

    INIT_LIST_HEAD(&init_kernel_vma.i_list);
    list_add(&init_kernel_vma.i_list, &g_kernel_vm_list);
    INIT_LIST_HEAD(&init_dma_vma.i_list);
    list_add(&init_dma_vma.i_list, &g_dma_vm_list);

    if (sunxi_iommu_clk_init() < 0)
    {
        goto fail;
    }

    if (sunxi_iommu->plat_data->version !=
        sunxi_iommu_read(sunxi_iommu, IOMMU_VERSION_REG))
    {
        printf("iommu version mismatch, please check and reconfigure\n");
        goto fail;
    }

    if (hal_request_irq(sunxi_iommu->irq, sunxi_iommu_irq, "iommu", sunxi_iommu) != HAL_OK)
    {
        goto fail;
    }

    if (sunxi_iommu_domain_alloc() == NULL)
    {
        hal_free_irq(sunxi_iommu->irq);
        goto fail;
    }

    hal_enable_irq(sunxi_iommu->irq);
    return 0;

fail:
    hal_free(sunxi_iommu);
    return -1;
}
