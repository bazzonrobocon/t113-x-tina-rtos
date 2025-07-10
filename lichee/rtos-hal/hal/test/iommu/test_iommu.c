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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <hal_iommu.h>
#include <hal_cmd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int ioctl(int fildes, int cmd, ...);

int cmd_test_iommu(int argc, char **argv)
{
    struct ion_handle *handle = ion_alloc(1024 * 1024 * 2);

    if (!handle)
    {
        printf("alloc ion_handle failed\n");
        return -1;
    }
    void *kernel_addr = ion_map_kernel(handle);
    if (!kernel_addr)
    {
        printf("map kernel addr failed\n");
        return -1;
    }
    void *dma_addr = ion_map_dma(handle);
    if (!dma_addr)
    {
        printf("map kernel addr failed\n");
        return -1;
    }
    printf("kernel_addr:0x%08x\n", kernel_addr);
    printf("   dma_addr:0x%08x\n", dma_addr);

    printf("*(volatile unsigned int *)(0x%08x)=0x%08x\n", kernel_addr, *(volatile unsigned int *)(kernel_addr));


    struct ion_handle *handle1 = ion_alloc(1024 * 1024 * 2);

    if (!handle)
    {
        printf("alloc ion_handle failed\n");
        return -1;
    }
    void *kernel_addr1 = ion_map_kernel(handle1);
    if (!kernel_addr1)
    {
        printf("map kernel addr failed\n");
        return -1;
    }
    void *dma_addr1 = ion_map_dma(handle1);
    if (!dma_addr1)
    {
        printf("map kernel addr failed\n");
        return -1;
    }
    printf("kernel_addr1:0x%08x\n", kernel_addr1);
    printf("   dma_addr1:0x%08x\n", dma_addr1);

    printf("*(volatile unsigned int *)(0x%08x)=0x%08x\n", kernel_addr1, *(volatile unsigned int *)(kernel_addr1));
    ion_unmap_dma(handle1);
    ion_unmap_kernel(handle1);
    ion_free(handle1);

    ion_unmap_dma(handle);
    ion_unmap_kernel(handle);
    ion_free(handle);
    return 0;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_test_iommu, __cmd_test_iommu, iommu test)

int cmd_test_iommu2(int argc, char **argv)
{
    struct ion_cmd_args args;
    int ret;

    int fd = open("/dev/iommu", O_RDWR);
    if (fd < 0)
    {
        printf("open iommu failed\n");
        return -1;
    }
    memset(&args, 0, sizeof(args));
    args.size = 1024 * 1024 * 2;

    ret = ioctl(fd, ION_CMD_ALLOC, &args);
    if (ret)
    {
        printf("ion alloc failed\n");
        close(fd);
        return -1;
    }

    ret = ioctl(fd, ION_CMD_MAP_KERNEL, &args);
    if (ret)
    {
        printf("ion map kernel failed\n");
        ioctl(fd, ION_CMD_FREE, &args);
        close(fd);
        return -1;
    }

    ret = ioctl(fd, ION_CMD_MAP_DMA, &args);
    if (ret)
    {
        printf("ion map dma failed\n");
        ioctl(fd, ION_CMD_UNMAP_KERNEL, &args);
        ioctl(fd, ION_CMD_FREE, &args);
        close(fd);
        return -1;
    }

    printf("kernel_addr:0x%08x\n", args.handle->kernel_vma->start_addr);
    printf("   dma_addr:0x%08x\n", args.handle->dma_vma->start_addr);

    printf("*(volatile unsigned int *)(0x%08x)=0x%08x\n", args.handle->kernel_vma->start_addr,
           *(volatile unsigned int *)(args.handle->kernel_vma->start_addr));

    ioctl(fd, ION_CMD_UNMAP_DMA, &args);
    ioctl(fd, ION_CMD_UNMAP_KERNEL, &args);
    ioctl(fd, ION_CMD_FREE, &args);
    close(fd);

    return 0;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_test_iommu2, __cmd_test_iommu2, iommu test)
