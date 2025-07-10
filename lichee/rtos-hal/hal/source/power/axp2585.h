/* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.

 * Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
 * the the People's Republic of China and other countries.
 * All Allwinner Technology Co.,Ltd. trademarks are used with permission.

 * DISCLAIMER
 * THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
 * IF YOU NEED TO INTEGRATE THIRD PARTY’S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
 * IN ALLWINNERS’SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
 * ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
 * ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
 * COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
 * YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY’S TECHNOLOGY.


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

#ifndef BMU1760_H_
#define BMU1760_H_

#include "ffs.h"
#include <stddef.h>
#include <sunxi_hal_power.h>
#include <sunxi_hal_power_private.h>

/* For axp2585 */
#define AXP2585_STATUS              (0x00)
#define AXP2585_IC_TYPE             (0x03)
#define AXP2585_ILIMIT              (0x10)
#define AXP2585_RBFET_SET           (0x11)
#define AXP2585_POK_SET             (0x15)
#define AXP2585_GPIO1_CTL           (0x18)
#define AXP2585_GPIO2_CTL           (0x19)
#define AXP2585_GPIO1_SIGNAL        (0x1A)
#define AXP2585_CC_EN               (0x22)
#define AXP2585_ADC_EN              (0x24)
#define AXP2585_OFF_CTL             (0x28)
#define AXP2585_CC_GLOBAL_CTRL      (0x31)
#define AXP2585_CC_LOW_POWER_CTRL   (0x32)
#define AXP2585_CC_MODE_CTRL        (0x33)
#define AXP2585_CC_STATUS0          (0x37)
#define AXP2585_INTEN1              (0x40)
#define AXP2585_INTEN2              (0x41)
#define AXP2585_INTEN3              (0x42)
#define AXP2585_INTEN4              (0x43)
#define AXP2585_INTEN5              (0x44)
#define AXP2585_INTEN6              (0x45)
#define AXP2585_INTSTS1             (0x48)
#define AXP2585_INTSTS2             (0x49)
#define AXP2585_INTSTS3             (0x4A)
#define AXP2585_INTSTS4             (0x4B)
#define AXP2585_INTSTS5             (0x4C)
#define AXP2585_INTSTS6             (0x4D)
#define AXP2585_TDIE_REG            (0x56)
#define AXP2585_VTS_RES             (0x58)
#define AXP2585_VBATH_REG           (0x78)
#define AXP2585_IBATH_REG           (0x7A)
#define AXP2585_DISIBATH_REG        (0x7c)
#define AXP2585_ADC_CONTROL         (0x80)
#define AXP2585_TS_PIN_CONTROL      (0x81)
#define AXP2585_VLTF_CHARGE         (0x84)
#define AXP2585_VHTF_CHARGE         (0x85)
#define AXP2585_VLTF_WORK           (0x86)
#define AXP2585_VHTF_WORK           (0x87)
#define AXP2585_ICC_CFG             (0x8B)
#define AXP2585_CHARGE_CONTROL2     (0x8C)
#define AXP2585_TIMER2_SET          (0x8E)
#define AXP2585_COULOMB_CTL         (0xB8)
#define AXP2585_CAP                 (0xB9)
#define AXP2585_RDC0                (0xBA)
#define AXP2585_RDC1               (0xBB)
#define AXP2585_BATCAP0             (0xE0)
#define AXP2585_BATCAP1             (0xE1)
#define AXP2585_WARNING_LEVEL       (0xE6)
#define AXP2585_ADJUST_PARA         (0xE8)
#define AXP2585_ADJUST_PARA1        (0xE9)
#define AXP2585_ADDR_EXTENSION      (0xFF)

#define AXP2585_VBAT_MAX        (8000)
#define AXP2585_VBAT_MIN        (2000)
#define AXP2585_SOC_MAX         (100)
#define AXP2585_SOC_MIN         (0)
#define AXP2585_MASK_VBUS_STATE BIT(1)
#define AXP2585_MODE_RSTGAUGE   BIT(3)
#define AXP2585_MODE_RSTMCU     BIT(2)
#define AXP2585_MAX_PARAM       512
#define AXP2585_BROMUP_EN       BIT(0)
#define AXP2585_CFG_UPDATE_MARK BIT(4)

#define AXP2585_CHARGING_TRI  (0)
#define AXP2585_CHARGING_PRE  (1)
#define AXP2585_CHARGING_CC   (2)
#define AXP2585_CHARGING_CV   (3)
#define AXP2585_CHARGING_DONE (4)
#define AXP2585_CHARGING_NCHG (5)
#define AXP2585_MANUFACTURER  "xpower,axp2585"

#define AXP2585_ADC_BATVOL_ENABLE   (1 << 4)
#define AXP2585_ADC_TSVOL_ENABLE    (1 << 5)
#define AXP2585_ADC_BATCUR_ENABLE   (1 << 6)
#define AXP2585_ADC_DIETMP_ENABLE   (1 << 7)

int axp2585_init_power(struct power_dev *rdev);

int axp2585_get_power(struct power_dev *rdev);

int axp2585_get_version(struct power_dev *rdev);

#endif /* BMU1760_H_ */
