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

#include <stdio.h>
#include <irqs.h>
#include <console.h>
#include <hal_mem.h>
#include <hal_gpio.h>
#include <openamp/sunxi_helper/openamp_log.h>
#include <openamp/sunxi_helper/mem_map.h>
#include <openamp/openamp_share_irq.h>
#include "./../sunxi_helper/sunxi_rsc_tab.h"

#define READY_TAG		(('R' << 24) | ('E' << 16) | ('D' << 8) | 'Y')
#define USED_TAG		(('U' << 24) | ('S' << 16) | ('E' << 8) | 'D')

struct _table_head {
	uint32_t tag;
	uint32_t len;
	uint32_t banks_off;
	uint32_t banks_num;
} __attribute__((packed));

struct _table_entry {
	uint32_t status;
	/*
	 * current we only support update gpio bank info.
	 *
	 * type = 0 -> nomal interrupt
	 * type = 1 -> GPIOA
	 * type = N -> GPIO['A' + N]
	 */
	uint32_t type;
	uint32_t flags;
	uint32_t arch_irq;		/* hwirq or GPIO custom IRQ value */
} __attribute__((packed));

static uint32_t *share_irq_table = NULL;
static uint32_t *share_gpio_banks = NULL; /* record gpio banks mask */
static uint32_t *share_gpio_irqs = NULL;  /* record gpio hw irq number */
static uint32_t share_bank_num;
int irq_num0, irq_num1;

extern int amp_printf(const char *fmt, ...);

/*
 * [0] = addr
 * [1] = len
 *
 * uboot/boot0 will update this array when early boot.
 */
static uint32_t early_buf_info[2] __attribute__((__section__(".share_irq_table")));

int openamp_share_irq_early_init(void)
{
	struct fw_rsc_carveout *carveout;
	void *share_buf;
	uint32_t buf, buf_len;
	int ret;

	carveout = resource_table_get_share_irq_entry(NULL, RPROC_IRQ_TAB);
	if (!carveout) {
		amp_printf("[%s:%d] get irq table shm failed\r\n", __func__, __LINE__);
		return -EINVAL;
	}

	/*
	 * if run befor the remote kernel, this should be updated in boot process.
	 */
	if (early_buf_info[0] != 0) {
		buf = early_buf_info[0];
		buf_len = early_buf_info[1];
		amp_printf("[%s:%d] boot before kernel\r\n", __func__, __LINE__);
		goto _init;
	}
	if (carveout->pa == 0 || carveout->pa == FW_RSC_ADDR_ANY) {
		return -ENODEV;
	}
	buf = carveout->pa;
	buf_len = carveout->len;

_init:
	ret = mem_pa_to_va(buf, (unsigned long *)&share_buf, NULL);
	if (ret < 0) {
		amp_printf("[%s:%d] invalid share_table\r\n", __func__, __LINE__);
		return -EFAULT;
	}
	openamp_share_irq_init(share_buf, buf_len);

	return 0;
}

int openamp_share_irq_init(void *share_buf, int buf_len)
{
	int len, i;
	struct _table_entry *ptable;
	struct _table_head *phead;

	share_irq_table = share_buf;

	phead = share_buf;
	ptable = share_buf + sizeof(*phead);

	if (phead->tag == USED_TAG) {
		amp_printf("[%s:%d] share_irq_table already init\r\n",
						__func__, __LINE__);
		return 0;
	}
	if (phead->tag != READY_TAG) {
		printf("[%s:%d] share_irq_table(addr: %p) invalid tag: 0x%x, should be 0x%x\r\n",
						__func__, __LINE__, phead, phead->tag, READY_TAG);
		share_irq_table = NULL;
		return -EFAULT;
	}

	if (phead->len != buf_len) {
		phead->len = buf_len;
	}

	share_gpio_banks = (uint32_t *)((char *)share_irq_table + phead->banks_off);
	share_bank_num = phead->banks_num;

	/* record gpio hwirq */
	share_gpio_irqs = hal_malloc(sizeof(uint32_t) * share_bank_num);
	if (!share_gpio_irqs) {
		share_irq_table = NULL;
		share_gpio_banks = NULL;
		share_bank_num = 0;
		amp_printf("[%s:%d] malloc gpio_table failed\r\n",
						__func__, __LINE__, phead->tag);
		return -ENOMEM;
	}

	len = (phead->banks_off - sizeof(*phead)) / sizeof(*ptable);
	memset(share_gpio_irqs, 0, sizeof(uint32_t) * share_bank_num);

	for (i = 0; i < len; i++) {
		uint32_t type = ptable[i].type;
		if (type >= 1 && type <= share_bank_num)
			share_gpio_irqs[type - 1] = ptable[i].arch_irq;
	}

	phead->tag = USED_TAG;

	amp_printf("[%s:%d] done\r\n", __func__, __LINE__);

	return 0;
}

uint32_t sunxi_get_banks_mask(int hwirq)
{
	int banks, i;

	if (!share_gpio_banks)
		return 0;

	banks = -1;
	for (i = 0; i < share_bank_num; i++) {
		if (share_gpio_irqs[i] == hwirq) {
			banks = i;
			break;
		}
	}

	if (banks >= 0)
		return share_gpio_banks[banks];
	else
		return 0;
}

bool sunxi_is_arch_irq(int irq)
{
	if (!share_irq_table)
		return false;
	if (share_irq_table[irq])
		return true;
	return false;
}

int disable_openamp_share_gpio_interrupt(void)
{
	int ret, i, entry_num;
	struct _table_entry *ptable;
	struct _table_head *phead;
	uint32_t gpio_group_id, gpio_id, gpio_num_in_group, gpio_irq_id = 0, alloc_mask, bit;

	if (!share_irq_table) {
		printf("Warning: no shared GPIO interrupt info table!\n");
		return -1;
	}

	phead = (struct _table_head *)share_irq_table;
	ptable = (void *)share_irq_table + sizeof(*phead);

	entry_num = (phead->banks_off - sizeof(*phead)) / sizeof(*ptable);

	gpio_num_in_group = GPIOB(0) - GPIOA(0);

	for (i = 0; i < entry_num; i++) {
		if (!ptable[i].status)
			continue;

		if (!ptable[i].type)
			continue;

		if (!ptable[i].flags)
			continue;

		gpio_group_id = ptable[i].type;
		alloc_mask = ptable[i].flags;
		for (bit = 0; bit <= 31; bit++) {
			if (!(alloc_mask & (1 << bit)))
				continue;

			gpio_id = (gpio_num_in_group * (gpio_group_id - 1)) + bit;
			ret = hal_gpio_to_irq(gpio_id, &gpio_irq_id);
			if (ret) {
				printf("hal_gpio_to_irq failed, GPIO: P%c%u, gpio_group_id: %u, gpio_id: %u\n",
						'A' + (gpio_group_id - 1), gpio_id % gpio_num_in_group,
						gpio_group_id, gpio_id);
				continue;
			}

			ret = hal_gpio_irq_disable(gpio_irq_id);
			if (ret)
				printf("hal_gpio_irq_disable failed, GPIO: P%c%u, gpio_group_id: %u, gpio_id: %u, gpio_irq_id: %u\n",
						'A' + (gpio_group_id - 1), gpio_id % gpio_num_in_group,
						gpio_group_id, gpio_id, gpio_irq_id);
		}
	}

    return 0;
}

int cmd_dump_shirq(int argc, char ** argv)
{
	int i, len;
	struct _table_entry *ptable;
	struct _table_head *phead;

	if (!share_irq_table) {
		printf("AMP share interrupt info table is not initialized.\n");
		return 0;
	}

	phead = (struct _table_head *)share_irq_table;
	ptable = (void *)share_irq_table + sizeof(*phead);

	len = (phead->banks_off - sizeof(*phead)) / sizeof(*ptable);

	printf("     :Status  Type   Flags     IRQ\n");
	for (i = 0; i < len; i++) {
		if (!ptable[i].status)
			continue;

		printf(" %4d:%s %s 0x%08x %d\n", i,
						ptable[i].status > 0 ? "Enable " : "Disable",
						ptable[i].type == 0 ? "Normal" : "GPIO  ",
						ptable[i].flags, ptable[i].arch_irq);
	}

	printf("\nGPIO group interrupt allocation info:\n");
	for (i = 0; i < share_bank_num; i++) {
		if (!share_gpio_irqs[i])
			continue;

		printf("GPIO%c, allocation mask: 0x%08x, IRQ:%3d\n", 'A' + i, share_gpio_banks[i],
						share_gpio_irqs[i]);
	}

    return 0;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_dump_shirq, dump_shirq, show share interrupt table);

static hal_irqreturn_t hal_gpio_irq_handler0(void *data)
{
	irq_num0++;
	return 0;
}

static hal_irqreturn_t hal_gpio_irq_handler1(void *data)
{
	irq_num1++;
	return 0;
}

static int cmd_gpio_irq_test0(int argc, char ** argv)
{
	int ret = -1;
	uint32_t irq;
	uint32_t bank = 0;
	uint32_t pin = 0;

	if (argc != 3) {
		printf("Usage:\n");
		printf("\trequest_gpio A 5: request GPIOA5 pin interrupt\n");
		return 0;
	}

	bank = (uint32_t)*(argv[1]);
	pin = (uint32_t)*(argv[2]) - '0';

	if (bank < 'A' || bank > ('A' + share_bank_num)) {
		printf("invalid bank index: %c = %d\n", bank, bank);
		return 0;
	}

	if (pin < 0 || pin > 32) {
		printf("invalid pin index\n");
		return 0;
	}

	printf("request_gpio: GPIO%c%d\n", bank, pin);

	switch (bank) {
	case 'B':
		hal_gpio_to_irq(GPIOB(pin), &irq); break;
	case 'C':
		hal_gpio_to_irq(GPIOC(pin), &irq); break;
    	case 'D':
		hal_gpio_to_irq(GPIOD(pin), &irq); break;
	case 'E':
		hal_gpio_to_irq(GPIOE(pin), &irq); break;
	case 'F':
		hal_gpio_to_irq(GPIOF(pin), &irq); break;
	case 'G':
		hal_gpio_to_irq(GPIOG(pin), &irq); break;
	case 'H':
		hal_gpio_to_irq(GPIOH(pin), &irq); break;
	case 'I':
		hal_gpio_to_irq(GPIOI(pin), &irq); break;
	case 'J':
		hal_gpio_to_irq(GPIOJ(pin), &irq); break;
	case 'K':
		hal_gpio_to_irq(GPIOK(pin), &irq); break;
	case 'L':
		hal_gpio_to_irq(GPIOL(pin), &irq); break;
	case 'M':
		hal_gpio_to_irq(GPIOM(pin), &irq); break;
	case 'N':
		hal_gpio_to_irq(GPION(pin), &irq); break;
	default:
		printf("invalid bank index: %c = %d\n", bank, bank);
		return 0;
	}
	printf("gpio irq: %d\n", irq);

	ret = hal_gpio_irq_request(irq,
				   hal_gpio_irq_handler0,
				   IRQ_TYPE_EDGE_BOTH, NULL);
	if(ret < 0) {
		printf("gpio irq request failed!\n");
		return 0;
	}

	hal_gpio_irq_enable(irq);

	printf("gpio init success!\n");
	return 0;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_gpio_irq_test0, request_gpio0, request pin interrupt);

static int cmd_gpio_irq_test1(int argc, char ** argv)
{
	int ret = -1;
	uint32_t irq;
	uint32_t bank = 0;
	uint32_t pin = 0;

	if (argc != 3) {
		printf("Usage:\n");
		printf("\trequest_gpio A 5: request GPIOA5 pin interrupt\n");
		return 0;
	}

	bank = (uint32_t)*(argv[1]);
	pin = (uint32_t)*(argv[2]) - '0';

	if (bank < 'A' || bank > ('A' + share_bank_num)) {
		printf("invalid bank index: %c = %d\n", bank, bank);
		return 0;
	}

	if (pin < 0 || pin > 32) {
		printf("invalid pin index\n");
		return 0;
	}

	printf("request_gpio: GPIO%c%d\n", bank, pin);

	switch (bank) {
	case 'B':
		hal_gpio_to_irq(GPIOB(pin), &irq); break;
	case 'C':
		hal_gpio_to_irq(GPIOC(pin), &irq); break;
	case 'D':
		hal_gpio_to_irq(GPIOD(pin), &irq); break;
	case 'E':
		hal_gpio_to_irq(GPIOE(pin), &irq); break;
	case 'F':
		hal_gpio_to_irq(GPIOF(pin), &irq); break;
	case 'G':
		hal_gpio_to_irq(GPIOG(pin), &irq); break;
	case 'H':
		hal_gpio_to_irq(GPIOH(pin), &irq); break;
	case 'I':
		hal_gpio_to_irq(GPIOI(pin), &irq); break;
	case 'J':
		hal_gpio_to_irq(GPIOJ(pin), &irq); break;
	case 'K':
		hal_gpio_to_irq(GPIOK(pin), &irq); break;
	case 'L':
		hal_gpio_to_irq(GPIOL(pin), &irq); break;
	case 'M':
		hal_gpio_to_irq(GPIOM(pin), &irq); break;
	case 'N':
		hal_gpio_to_irq(GPION(pin), &irq); break;
	default:
		printf("invalid bank index: %c = %d\n", bank, bank);
		return 0;
	}
	printf("gpio irq: %d\n", irq);

	ret = hal_gpio_irq_request(irq,
				   hal_gpio_irq_handler1,
				   IRQ_TYPE_EDGE_BOTH, NULL);
	if(ret < 0) {
		printf("gpio irq request failed!");
		return 0;
	}

	hal_gpio_irq_enable(irq);

	printf("gpio init success!\n");
	return 0;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_gpio_irq_test1, request_gpio1, request pin interrupt);

static int cmd_dump_pin_irq(int argc, char ** argv)
{

	int len, i;
	void *share_buf = share_irq_table;
	struct _table_entry *ptable;
	struct _table_head *phead;

	if (!share_irq_table) {
		printf("AMP share interrupt info table is not initialized.\n");
		return 0;
	}

	phead = share_buf;
	ptable = share_buf + sizeof(*phead);

	len = (phead->banks_off - sizeof(*phead)) / sizeof(*ptable);
	for (i = 0; i < len; i++) {
		uint32_t type = ptable[i].type;
		if (type >= 1 && type <= share_bank_num) {
			printf("ptable[%d] %d: %d(%x)\n", i, type, ptable[i].arch_irq, (uint32_t)&ptable[i].arch_irq);
			share_gpio_irqs[type - 1] = ptable[i].arch_irq;
		}

	}

	printf("irq_num0: %d\n", irq_num0);
	printf("irq_num1: %d\n", irq_num1);
	return 0;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_dump_pin_irq, dump_pin_irq, show pin interrupt number);
