/*
* Allwinner GMAC driver.
*
* Copyright(c) 2022-2027 Allwinnertech Co., Ltd.
*
* This file is licensed under the terms of the GNU General Public
* License version 2.  This program is licensed "as is" without any
* warranty of any kind, whether express or implied.
*/

#include "platform_gmac.h"
const u32 hal_gmac_dma_desc_num = 256;

/* Ring buffer caculate method */
#define circ_cnt(head, tail, size) (((head) > (tail)) ? \
					((head) - (tail)) : \
					((head) - (tail)) & ((size) - 1))

#define circ_space(head, tail, size) circ_cnt((tail), ((head) + 1), (size))

#define circ_inc(n, s) (((n) + 1) % (s))

#ifdef DEBUG
static void pkt_hex_dump(char *prefix_str, void *pkt, unsigned int len)
{
	int i;
	unsigned char *data = (unsigned char *)pkt;
	for (i = 0; i < len; i++) {
		if (!(i % 16))
			printf("\n%s %08x:", prefix_str, i);

		printf(" %02x", data[i]);
	}
	printf("\n");
}
#else
#define pkt_hex_dump(a, b, c)  {}
#endif


static void hal_gmac_desc_init_chain(struct dma_desc *desc, u32 addr, u32 size)
{
	int i;
	struct dma_desc *p = desc;
	u32 dma_phy = addr;

	for (i = 0; i < (size - 1); i++) {
		dma_phy += sizeof(*p);
		p->desc3 = dma_phy;

		p->desc1.all |= 1 << GMAC_CHAIN_MODE_OFFSET;
		p++;
	}

	p->desc1.all |= 1 << GMAC_CHAIN_MODE_OFFSET;
	p->desc3 = addr;
}

static void hal_gmac_write_hwaddr(struct ethernetif *chip)
{
	u32 mac;
	u8_t *mac_arr = chip->netif->hwaddr;

	mac = (mac_arr[5] << 8) | mac_arr[4];
	hal_writel(mac, chip->base + GMAC_ADDR_HI(0));

	mac = (mac_arr[3] << 24) | (mac_arr[2] << 16) | (mac_arr[1] << 8) | mac_arr[0];
	writel(mac, chip->base + GMAC_ADDR_LO(0));
}

static u16 hal_gmac_phy_read(struct ethernetif *chip, u8 phy_addr, u16 reg)
{
	u32 value = 0;
	const u32 wait_us = 1;
	u32 timeout = GMAC_TIMEOUT_US;

	value |= GMAC_MDIO_MDC_DIV;
	value |= (((phy_addr << GMAC_MDIO_PHYADDR_OFFSET) & GMAC_MDIO_PHYADDR_MASK) |
			((reg << GMAC_MDIO_PHYREG_OFFSET) & GMAC_MDIO_PHYREG_MASK) |
					MII_BUSY);

	hal_writel(value, chip->base + GMAC_MDIO_ADDR);

	while (hal_readl(chip->base + GMAC_MDIO_ADDR) & MII_BUSY) {
		hal_udelay(wait_us);
		timeout -= wait_us;
		if (timeout <  0) {
			printf("Error: %s Mii operation timeout\n", __func__);
			break;
		}
	}

	return hal_readl(chip->base + GMAC_MDIO_DATA);
}

static void hal_gmac_phy_write(struct ethernetif *chip, u8 phy_addr, u8 reg, u16 data)
{
	u32 value = 0;
	const u32 wait_us = 1;
	u32 timeout = GMAC_TIMEOUT_US;

	value |= GMAC_MDIO_MDC_DIV;
	value |= (((phy_addr << GMAC_MDIO_PHYADDR_OFFSET) & GMAC_MDIO_PHYADDR_MASK) |
			((reg << GMAC_MDIO_PHYREG_OFFSET) & GMAC_MDIO_PHYREG_MASK) |
					MII_WRITE | MII_BUSY);

	hal_writel(data, chip->base + GMAC_MDIO_DATA);
	hal_writel(value, chip->base + GMAC_MDIO_ADDR);

	while (hal_readl(chip->base + GMAC_MDIO_ADDR) & MII_BUSY) {
		hal_udelay(wait_us);
		timeout -= wait_us;
		if (timeout < 0) {
			printf("Error: %s Mii operation timeout\n", __func__);
			break;
		}
	}
}

static void hal_gmac_int_enable(struct ethernetif *chip)
{
	u32 val;

	val = hal_readl(chip->base + GMAC_INT_EN);
	val |= GMAC_RX_INT_EN;
	hal_writel(val, chip->base + GMAC_INT_EN);
}

static void hal_gmac_int_disable(struct ethernetif *chip)
{
	u32 val;

	val = hal_readl(chip->base + GMAC_INT_EN);
	val &= ~GMAC_RX_INT_EN;
	hal_writel(val, chip->base + GMAC_INT_EN);
}

static void hal_gmac_syscfg_init(struct ethernetif *chip)
{
	u32 value;

	value = hal_readl(chip->syscfg_base);

	/* Write phy type */
	value &= ~(1 << GMAC_PHY_SELECT_OFFSET);

	/* Only support RGMII/RMII */
	if (chip->phy_interface == PHY_INTERFACE_MODE_RGMII)
		value |= GMAC_PHY_RGMII_MASK;
	else
		value &= (~GMAC_PHY_RGMII_MASK);

	/* Write RGMII ETCS ret */
	value &= (~GMAC_ETCS_RMII_MASK);
	if (chip->phy_interface == PHY_INTERFACE_MODE_RGMII)
		value |= GMAC_RGMII_INTCLK_MASK;
	else if (chip->phy_interface == PHY_INTERFACE_MODE_RMII)
		value |= GMAC_RMII_MASK;

	/*
	 * Adjust Tx/Rx clock delay
	 * Tx clock delay: 0~7
	 * Rx clock delay: 0~31
	 */
	value &= ~(GMAC_TX_DELAY_MASK << GMAC_TX_DELAY_OFFSET);
	value |= ((chip->tx_delay & GMAC_TX_DELAY_MASK) << GMAC_TX_DELAY_OFFSET);
	value &= ~(GMAC_RX_DELAY_MASK << GMAC_RX_DELAY_OFFSET);
	value |= ((chip->rx_delay & GMAC_RX_DELAY_MASK) << GMAC_RX_DELAY_OFFSET);

	hal_writel(value, chip->syscfg_base);
}

static void hal_gmac_tx_complete(struct ethernetif *chip)
{
	struct dma_desc *desc;
	u32 entry = 0;

	hal_spin_lock(&chip->tx_lock);
	while (circ_cnt(chip->tx_dirty, chip->tx_clean, hal_gmac_dma_desc_num) > 0) {
		entry = chip->tx_clean;
		desc = chip->dma_desc_tx + entry;

		hal_dcache_invalidate((unsigned long)desc, sizeof(*desc));

		/* make sure invalidate dcache finish */
		isb();

		if (desc->desc0.tx.own)
			break;

		desc->desc1.all = 0;
		desc->desc2 = 0;
		desc->desc1.all |= (1 << GMAC_CHAIN_MODE_OFFSET);

		hal_dcache_clean((unsigned long)desc, sizeof(*desc));

		chip->tx_clean = circ_inc(entry, hal_gmac_dma_desc_num);
	}
	hal_spin_unlock(&chip->tx_lock);
}

int hal_gmac_output(struct ethernetif *chip, struct pbuf *p)
{
	struct pbuf *q;
	struct dma_desc *tx_p, *first;
	u32 value, send_status, frame_len, entry;

	entry = chip->tx_dirty;
	first = chip->dma_desc_tx + entry;
	tx_p = chip->dma_desc_tx + entry;

	if (circ_space(chip->tx_dirty, chip->tx_clean, hal_gmac_dma_desc_num) < 1) {
		printf("Error: Gmac not have sufficient desc\n");
		return -EBUSY;
	}

	send_status = hal_readl((chip->base + GMAC_TX_DMA_STA)) & GMAC_TX_DMA_MASK;

	for (q = p; q != NULL; q = q->next) {
		tx_p = chip->dma_desc_tx + entry;
		frame_len = q->len;

		hal_dcache_invalidate((unsigned long)tx_p, CACHELINE_LEN);

		/* make sure invalidate dcache finish */
		isb();

		tx_p->desc1.all |= GMAC_DMA_BUFF_SIZE(frame_len);
		tx_p->desc2 = __va_to_pa((unsigned long)q->payload);

		if (first != tx_p)
			tx_p->desc0.all = GMAC_DMA_OWN_DESC;  /* Set Own */

		hal_dcache_clean((unsigned long)(q->payload), SZ_2K);
		hal_dcache_clean((unsigned long)tx_p, CACHELINE_LEN);

		entry = circ_inc(entry, hal_gmac_dma_desc_num);
		pkt_hex_dump("TX", (void *)q->payload, 64);
	}

	/* tx close */
	first->desc1.tx.first_sg = 1;
	tx_p->desc1.tx.last_seg = 1;
	tx_p->desc1.tx.interrupt = 1;
	chip->tx_dirty = entry;

	/* make sure config dma desc finish */
	wmb();

	first->desc0.all = GMAC_DMA_OWN_DESC;  /* Set Own */
	hal_dcache_clean((unsigned long)first, CACHELINE_LEN);

	/* make sure clean dcache finish */
	isb();

	value = hal_readl(chip->base + GMAC_TX_CTL1);
	value |= GMAC_TX_DMA_FLUSH_FIFO;
	hal_writel(value, chip->base + GMAC_TX_CTL1);

	/* Enable transmit and Poll transmit */
	value = hal_readl(chip->base + GMAC_TX_CTL1);
	if (send_status == GMAC_TX_DMA_STOP)
		value |= GMAC_TX_DMA_EN;
	else
		value |= GMAC_TX_DMA_START;
	hal_writel(value, chip->base + GMAC_TX_CTL1);

	hal_gmac_tx_complete(chip);

	return 0;
}

static void hal_gmac_desc_buf_set(struct dma_desc *desc, u32 dma_addr, int size)
{
	desc->desc1.all &= ~GMAC_DMA_BUFF_SIZE_MASK;
	desc->desc1.all |= GMAC_DMA_BUFF_SIZE(size);
	desc->desc2 = dma_addr;
}

static void hal_gmac_rx_refill(struct ethernetif *chip)
{
	struct dma_desc *desc;
	unsigned char *rx = NULL;
	u32 dma_addr;

	while (circ_space(chip->rx_clean, chip->rx_dirty, hal_gmac_dma_desc_num) > 0) {
		int entry = chip->rx_clean;

		desc = chip->dma_desc_rx + entry;

		if (!chip->rx_buf[entry]) {
			rx = hal_malloc_align(sizeof(unsigned char) * MAX_BUF_SZ, CACHELINE_LEN);
			if (!rx)
				break;

			memset((void *)rx, 0, sizeof(unsigned char) * MAX_BUF_SZ);
			chip->rx_buf[entry] = rx;
			dma_addr = __va_to_pa((unsigned long)rx);
			hal_gmac_desc_buf_set(desc, dma_addr, MAX_BUF_SZ);

			rx = NULL;
		}

		wmb();
		desc->desc0.all = GMAC_DMA_OWN_DESC;  /* Set Own */
		hal_dcache_clean((unsigned long)desc, CACHELINE_LEN);

		chip->rx_clean = circ_inc(chip->rx_clean, hal_gmac_dma_desc_num);
	}
}

static int hal_gmac_rx_status(struct dma_desc *p)
{
	int ret = good_frame;

	if (p->desc0.rx.last_desc == 0)
		ret = discard_frame;

	if (p->desc0.rx.frm_type && (p->desc0.rx.chsum_err
			|| p->desc0.rx.ipch_err))
		ret = discard_frame;

	if (p->desc0.rx.err_sum)
		ret = discard_frame;

	if (p->desc0.rx.len_err)
		ret = discard_frame;

	if (p->desc0.rx.mii_err)
		ret = discard_frame;

	return ret;
}

struct pbuf *hal_gmac_input(struct ethernetif *chip)
{
	struct dma_desc *desc;
	struct pbuf *p = NULL;
	u32 entry, frame_len;
	int status;

	chip->irq_signal = false;
	entry = chip->rx_dirty;
	desc = chip->dma_desc_rx + entry;

	hal_dcache_invalidate((unsigned long)desc, CACHELINE_LEN);

	if (desc->desc0.all & GMAC_DMA_OWN_DESC) {
		chip->irq_signal = false;
		return NULL;
	}

	hal_dcache_invalidate((unsigned long)desc->desc2, MAX_BUF_SZ);
	status = hal_gmac_rx_status(desc);
	if (status == discard_frame) {
		printf("Error: get error pkt\n");
		goto out;
	}

	chip->rx_dirty = circ_inc(chip->rx_dirty, hal_gmac_dma_desc_num);

	/* get packet len from rx dma desc */
	frame_len = desc->desc0.rx.frm_len;

	p = pbuf_alloc(PBUF_RAW, frame_len, PBUF_POOL);
	memcpy(p->payload, chip->rx_buf[entry], frame_len);

out:
	hal_free_align(chip->rx_buf[entry]);
	chip->rx_buf[entry] = NULL;

	hal_gmac_rx_refill(chip);

	hal_gmac_int_enable(chip);

	return p;
}

static hal_irqreturn_t hal_gmac_irq_handler(void *p)
{
	struct ethernetif *chip = (struct ethernetif *)p;
	u32 val;

	hal_gmac_int_disable(chip);

	chip->irq_signal = true;
	val = hal_readl(chip->base + GMAC_INT_STA);
	hal_writel(val, chip->base + GMAC_INT_STA);

	return HAL_IRQ_OK;
}

static int hal_gmac_reset(u32 iobase, int n)
{
	u32 value;

	value = hal_readl(iobase + GMAC_BASIC_CTL1);
	value |= GMAC_SOFT_RST;
	hal_writel(value, iobase + GMAC_BASIC_CTL1);

	hal_udelay(n);

	return !!(hal_readl(iobase + GMAC_BASIC_CTL1) & GMAC_SOFT_RST);
}

static int hal_gmac_phy_init(struct ethernetif *chip)
{
	u32 phy_addr = 0, value, phy_id;
	u16 phy_val, phy_id_high, phy_id_low;
	int i;

	/* scan mdio bus, find phy addr */
	for (i = 0; i < GMAC_PHY_ADDR_MAX; i++) {
		phy_id_high = (u16)(hal_gmac_phy_read(chip, i, MII_PHYSID1)
							& GMAC_PHY_REG_MASK);
		phy_id_low = (u16)(hal_gmac_phy_read(chip, i, MII_PHYSID2) & GMAC_PHY_REG_MASK);

		phy_id = (phy_id_high << 16) | phy_id_low;

		if ((phy_id & GMAC_PHY_UNUSE_ID) == GMAC_PHY_UNUSE_ID)
			continue;

		phy_addr = i;
		printf("PHYID:0x%x, PHYADDR:0x%x\n", phy_id, phy_addr);
		break;
	}

	/* close autoneg and force to 1000Mbps/100Mbps */
	phy_val = hal_gmac_phy_read(chip, phy_addr, MII_BMCR);
	phy_val &= ~BMCR_ANENABLE;
	phy_val &= ~BMCR_SPEED1000;
	if (chip->phy_interface == PHY_INTERFACE_MODE_RGMII)
		phy_val |= BMCR_SPEED1000;
	else
		phy_val |= BMCR_SPEED100;
	phy_val |= BMCR_FULLDPLX;
	hal_gmac_phy_write(chip, phy_addr, MII_BMCR, phy_val);

	/* set mac speed to match phy speed */
	phy_val = hal_gmac_phy_read(chip, phy_addr, MII_BMCR);
	value = hal_readl((chip->base + GMAC_BASIC_CTL0));
	if (phy_val & BMCR_FULLDPLX)
		value |= GMAC_MAC_DUPLEX_MASK;
	else
		value &= ~GMAC_MAC_DUPLEX_MASK;

	value &= ~GMAC_MAC_SPEED_MASK;
	if (phy_val & BMCR_SPEED100)
		value |= GMAC_MAC_SPEED_100M;
	else if (!(phy_val & BMCR_SPEED1000))
		value |= GMAC_MAC_SPEED_1000M;
	hal_writel(value, chip->base + GMAC_BASIC_CTL0);

	/* type out phy ability */
	phy_val = hal_gmac_phy_read(chip, phy_addr, MII_BMCR);
	printf("Speed: %dMbps, Mode: %s, Loopback: %s\n",
				((phy_val & BMCR_SPEED1000) ? 1000 :
				((phy_val & BMCR_SPEED100) ? 100 : 10)),
				(phy_val & BMCR_FULLDPLX) ? "Full duplex" : "Half duplex",
				(phy_val & BMCR_LOOPBACK) ? "YES" : "NO");

	return 0;
}

static int hal_gmac_init(struct ethernetif *chip)
{
	u32 value;
	int ret;

	ret = hal_gmac_reset(chip->base, 1000);
	if (ret) {
		printf("Error: gmac reset failed, please check mac and phy clk\n");
		return -EINVAL;
	}

	/* init phy and set mac speed */
	hal_gmac_phy_init(chip);

	/* Initialize core */
	value = hal_readl(chip->base + GMAC_TX_CTL0);
	value |= (1 << GMAC_TX_FRM_LEN_OFFSET);
	value |= (1 << GMAC_TX_EN_OFFSET);
	hal_writel(value, chip->base + GMAC_TX_CTL0);

	value = hal_readl(chip->base + GMAC_TX_CTL1);
	value |= GMAC_TX_MD;
	value |= GMAC_TX_NEXT_FRM;
	hal_writel(value, chip->base + GMAC_TX_CTL1);

	value = hal_readl(chip->base + GMAC_RX_CTL0);
	value |= (1 << GMAC_CHECK_CRC_OFFSET);		/* Enable CRC & IPv4 Header Checksum */
	/*
	 * Enable rx
	 * Enable strip_fcs
	 */
	value |= (0x9 << GMAC_RX_EN_OFFSET);
	hal_writel(value, chip->base + GMAC_RX_CTL0);

	value = hal_readl(chip->base + GMAC_RX_CTL1);
	value |= GMAC_RX_DMA_MODE;
	hal_writel(value, chip->base + GMAC_RX_CTL1);

	/* GMAC frame filter */
	value = hal_readl(chip->base + GMAC_RX_FRM_FLT);
	value = GMAC_HASH_MULTICAST;
	hal_writel(value, chip->base + GMAC_RX_FRM_FLT);

	/* Burst should be 8 */
	value = hal_readl(chip->base + GMAC_BASIC_CTL1);
	value |= (GMAC_BURST_LEN << GMAC_BURST_LEN_OFFSET);
	hal_writel(value, chip->base + GMAC_BASIC_CTL1);

	/* Disable all interrupt of dma */
	hal_writel(0x00UL, chip->base + GMAC_INT_EN);

	hal_gmac_desc_init_chain(chip->dma_desc_tx, chip->dma_desc_tx_phy, hal_gmac_dma_desc_num);
	hal_gmac_desc_init_chain(chip->dma_desc_rx, chip->dma_desc_rx_phy, hal_gmac_dma_desc_num);

	chip->rx_clean = 0;
	chip->tx_clean = 0;
	chip->rx_dirty = 0;
	chip->tx_dirty = 0;

	hal_dcache_clean((unsigned long)chip->dma_desc_tx, SZ_2K);
	hal_dcache_clean((unsigned long)chip->dma_desc_rx, SZ_2K);

	hal_writel((unsigned long)chip->dma_desc_tx, chip->base + GMAC_TX_DESC_LIST);
	hal_writel((unsigned long)chip->dma_desc_rx, chip->base + GMAC_RX_DESC_LIST);

	value = hal_readl(chip->base + GMAC_RX_CTL1);
	value |= GMAC_RX_DMA_EN;
	hal_writel(value, chip->base + GMAC_RX_CTL1);

	hal_gmac_write_hwaddr(chip);

	hal_gmac_rx_refill(chip);
	return 0;
}

static void hal_gmac_init_param(struct ethernetif *chip)
{
	/* TODO:use syscfg */
	chip->base = GMAC_BASE;
	chip->syscfg_base = SYSCFG_BASE;
	chip->phy_interface = PHY_INTERFACE_MODE_RMII;
	chip->phyrst = PHYRST;
	chip->tx_delay = 0;
	chip->rx_delay = 0;
}

static int hal_gmac_resource_get(struct ethernetif *chip)
{
	hal_gmac_init_param(chip);

	chip->rst = hal_reset_control_get(HAL_SUNXI_RESET, GMAC_RST);
	if (!chip->rst) {
		printf("get gmac rst failed\n");
		return -EINVAL;
	}

	chip->gmac_clk = hal_clock_get(HAL_SUNXI_CCU, GMAC_CLK);
	if (!chip->gmac_clk) {
		printf("get gmac clk failed\n");
		return -EINVAL;
	}

	chip->gmac25m_clk = hal_clock_get(HAL_SUNXI_CCU, GMAC25M_CLK);
	if (!chip->gmac25m_clk) {
		printf("get gmac 25m clk failed\n");
		return -EINVAL;
	}

	return 0;
}

static void hal_gmac_resource_put(struct ethernetif *chip)
{
	hal_reset_control_put(chip->rst);
	hal_clock_put(chip->gmac_clk);
	hal_clock_put(chip->gmac25m_clk);
}

static int hal_gmac_clk_init(struct ethernetif *chip)
{
	int ret;

	ret = hal_reset_control_deassert(chip->rst);
	if (ret) {
		printf("deassert gmac rst failed\n");
		goto err0;
	}

	ret = hal_clock_enable(chip->gmac_clk);
	if (ret) {
		printf("enable gmac clk failed\n");
		goto err1;
	}

	ret = hal_clock_enable(chip->gmac25m_clk);
	if (ret) {
		printf("enable gmac 25m clk failed\n");
		goto err2;
	}

	return 0;

err2:
	hal_clock_disable(chip->gmac_clk);
err1:
	hal_reset_control_assert(chip->rst);
err0:
	return ret;
}

static void hal_gmac_clk_exit(struct ethernetif *chip)
{
	hal_reset_control_assert(chip->rst);
	hal_clock_disable(chip->gmac_clk);
	hal_clock_disable(chip->gmac25m_clk);
}

static void hal_gmac_gpio_init(struct ethernetif *chip)
{
	int i;

	for (i = 0; i < RMII_PIN_NUM; i++) {
		hal_gpio_pinmux_set_function(RMII_PIN(i), RMII_PINMUX);
		hal_gpio_set_driving_level(RMII_PIN(i), RMII_DRVLEVEL);
	}

	hal_gpio_set_direction(chip->phyrst, 1);
	hal_gpio_set_data(chip->phyrst, 0);
	hal_msleep(50);
	hal_gpio_set_data(chip->phyrst, 1);
}

static int hal_gmac_hardware_init(struct ethernetif *chip)
{
	int ret;

	ret = hal_gmac_clk_init(chip);
	if (ret)
		goto err0;

	hal_gmac_gpio_init(chip);

	hal_gmac_syscfg_init(chip);

	return 0;

err0:
	return ret;
}

static void hal_gmac_hardware_exit(struct ethernetif *chip)
{
	hal_gmac_clk_exit(chip);
}

static int hal_gmac_enable_irq(struct ethernetif *chip)
{
	int ret;

	ret = hal_request_irq(IRQ_GMAC, hal_gmac_irq_handler, "gmac", chip);
	if (ret < 0)
		return ret;

	hal_enable_irq(IRQ_GMAC);
	hal_gmac_int_enable(chip);

	return 0;
}

static void hal_gmac_disable_irq(struct ethernetif *chip)
{
	hal_gmac_int_disable(chip);
	hal_free_irq(IRQ_GMAC);
}

int hal_gmac_netif_init(struct ethernetif *chip)
{
	int ret;

	chip->dma_desc_tx = hal_malloc_align(sizeof(*chip->dma_desc_tx) * hal_gmac_dma_desc_num, CACHELINE_LEN);
	if (!chip->dma_desc_tx) {
		ret = -ENOMEM;
		goto err0;
	}
	chip->dma_desc_tx_phy = __va_to_pa((unsigned long)chip->dma_desc_tx);

	chip->dma_desc_rx = hal_malloc_align(sizeof(*chip->dma_desc_rx) * hal_gmac_dma_desc_num, CACHELINE_LEN);
	if (!chip->dma_desc_rx) {
		ret = -ENOMEM;
		goto err1;
	}
	chip->dma_desc_rx_phy = __va_to_pa((unsigned long)chip->dma_desc_rx);

	memset((void *)chip->dma_desc_tx, 0, sizeof(*chip->dma_desc_tx) * hal_gmac_dma_desc_num);
	memset((void *)chip->dma_desc_rx, 0, sizeof(*chip->dma_desc_rx) * hal_gmac_dma_desc_num);

	chip->rx_buf = hal_malloc_align(sizeof(unsigned char *) * hal_gmac_dma_desc_num, CACHELINE_LEN);
	if (!chip->rx_buf) {
		ret = -ENOMEM;
		goto err2;
	}
	memset((void *)chip->rx_buf, 0, sizeof(unsigned char *) * hal_gmac_dma_desc_num);

	ret = hal_gmac_resource_get(chip);
	if (ret)
		goto err3;

	ret = hal_gmac_hardware_init(chip);
	if (ret)
		goto err4;

	ret = hal_gmac_init(chip);
	if (ret)
		goto err5;

	ret = hal_gmac_enable_irq(chip);
	if (ret)
		return ret;

	return 0;

err5:
	hal_gmac_hardware_exit(chip);
err4:
	hal_gmac_resource_put(chip);
err3:
	hal_free_align(chip->rx_buf);
err2:
	hal_free_align(chip->dma_desc_rx);
err1:
	hal_free_align(chip->dma_desc_tx);
err0:
	return ret;
}

void hal_gmac_netif_exit(struct ethernetif *chip)
{
	hal_gmac_disable_irq(chip);
	hal_gmac_hardware_exit(chip);
	hal_gmac_resource_put(chip);
	hal_free_align(chip->rx_buf);
	hal_free_align(chip->dma_desc_rx);
	hal_free_align(chip->dma_desc_tx);
}
