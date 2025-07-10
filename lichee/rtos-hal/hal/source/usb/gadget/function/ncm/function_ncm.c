#include <sunxi_hal_usb.h>
#include <sunxi_hal_common.h>
#include "ch9.h"
#include "cdc.h"
#include "function_ncm.h"
#include "f_ncm_dev.h"
#include "gadget.h"

#define FORMATS_SUPPORTED	(USB_CDC_NCM_NTB16_SUPPORTED |	\
				 USB_CDC_NCM_NTB32_SUPPORTED)
#define NTB_DEFAULT_IN_SIZE	USB_CDC_NCM_NTB_MIN_IN_SIZE
#define NTB_OUT_SIZE		16384
#define ETH_FRAME_LEN	12112		/* Max. octets in frame sans FCS */
#define NCM_NDP_HDR_CRC_MASK	0x01000000
#define NCM_NDP_HDR_CRC		0x01000000
#define NCM_NDP_HDR_NOCRC	0x00000000

enum {
	USB_NCM_GADGET_LANGUAGE_IDX = 0,
	USB_NCM_GADGET_MANUFACTURER_IDX,
	USB_NCM_GADGET_PRODUCT_IDX,
	USB_NCM_GADGET_SERIAL_IDX,
	USB_NCM_GADGET_CONFIG_IDX,
	USB_NCM_GADGET_INTERFACE_IDX,
	USB_NCM_GADGET_FUNCTION_IDX,
	USB_NCM_GADGET_IAP_IDX,
	USB_NCM_GADGET_MAC_IDX,
	USB_NCM_GADGET_MAX_IDX,
};

#define INIT_NDP16_OPTS {					\
		.nth_sign = USB_CDC_NCM_NTH16_SIGN,		\
		.ndp_sign = USB_CDC_NCM_NDP16_NOCRC_SIGN,	\
		.nth_size = sizeof(struct usb_cdc_ncm_nth16),	\
		.ndp_size = sizeof(struct usb_cdc_ncm_ndp16),	\
		.ndplen_align = 4,				\
		.dgram_item_len = 1,				\
		.block_length = 1,				\
		.fp_index = 1,					\
		.reserved1 = 0,					\
		.reserved2 = 0,					\
		.next_fp_index = 1,				\
	}


#define INIT_NDP32_OPTS {					\
		.nth_sign = USB_CDC_NCM_NTH32_SIGN,		\
		.ndp_sign = USB_CDC_NCM_NDP32_NOCRC_SIGN,	\
		.nth_size = sizeof(struct usb_cdc_ncm_nth32),	\
		.ndp_size = sizeof(struct usb_cdc_ncm_ndp32),	\
		.ndplen_align = 8,				\
		.dgram_item_len = 2,				\
		.block_length = 2,				\
		.fp_index = 2,					\
		.reserved1 = 1,					\
		.reserved2 = 2,					\
		.next_fp_index = 2,				\
	}

#define	DEFAULT_FILTER	(USB_CDC_PACKET_TYPE_BROADCAST \
			|USB_CDC_PACKET_TYPE_ALL_MULTICAST \
			|USB_CDC_PACKET_TYPE_PROMISCUOUS \
			|USB_CDC_PACKET_TYPE_DIRECTED)


static const struct ndp_parser_opts ndp16_opts = {
			.nth_sign = USB_CDC_NCM_NTH16_SIGN,
			.ndp_sign = USB_CDC_NCM_NDP16_NOCRC_SIGN,
			.nth_size = sizeof(struct usb_cdc_ncm_nth16),
			.ndp_size = sizeof(struct usb_cdc_ncm_ndp16),
			.dpe_size = sizeof(struct usb_cdc_ncm_dpe16),
			.ndplen_align = 4,
			.dgram_item_len = 1,
			.block_length = 1,
			.ndp_index = 1,
			.reserved1 = 0,
			.reserved2 = 0,
			.next_ndp_index = 1
};


static const struct ndp_parser_opts ndp32_opts = {
		.nth_sign = USB_CDC_NCM_NTH32_SIGN,
		.ndp_sign = USB_CDC_NCM_NDP32_NOCRC_SIGN,
		.nth_size = sizeof(struct usb_cdc_ncm_nth32),
		.ndp_size = sizeof(struct usb_cdc_ncm_ndp32),
		.dpe_size = sizeof(struct usb_cdc_ncm_dpe32),
		.ndplen_align = 8,
		.dgram_item_len = 2,
		.block_length = 2,
		.ndp_index = 2,
		.reserved1 = 1,
		.reserved2 = 2,
		.next_ndp_index = 2,
};


static struct f_ncm function_ncm;

static const uint16_t g_str_lang_id[] = {
	0x0304, 0x0409
};

static const uint16_t g_str_manufacturer[] = {
	0x0314, 'A', 'l', 'l', 'w', 'i', 'n', 'n', 'e', 'r'
};

static const uint16_t g_str_product[] = {
	0x0316, 'N', 'C', 'M', ' ', 'G', 'a', 'd', 'g', 'e', 't'
};

static const uint16_t g_str_serialnumber[] = {
	0x0312, '2', '0', '0', '8', '0', '4', '1', '1'
};

static const uint16_t g_str_config[] = {
    0x0308, 'N', 'c', 'm'
};

static const uint16_t g_str_interface[] = {
    0x0308, 'N', 'c', 'm'
};

static const uint16_t g_str_function[] = {
    0x0310, 'C', 'D', 'C', ' ', 'N', 'C', 'M'
};

static const uint16_t g_str_iap[] = {
    0x031c, 'i', 'A', 'P', ' ', 'I', 'n', 't', 'e', 'r', 'f', 'a', 'c', 'e'
};
static const uint16_t g_str_mac[] = {
    0x0324, 'e', 'a', ':', '0', '7', ':', '4', '2', ':', '2', '6', ':', '8', '7', ':', 'a', '2'
};

static const uint16_t *ncm_string_desc[USB_NCM_GADGET_MAX_IDX] = {
	g_str_lang_id,
	g_str_manufacturer,
	g_str_product,
	g_str_serialnumber,
	g_str_config,
	g_str_interface,
	g_str_function,
	g_str_iap,
	g_str_mac
};

static struct usb_device_descriptor ncm_device_desc = {
	.bLength            = USB_DT_DEVICE_SIZE,
	.bDescriptorType    = USB_DT_DEVICE,
	.bcdUSB             = 0x0200,
	.bDeviceClass       = 0,
	.bDeviceSubClass    = 0,
	.bDeviceProtocol    = 0,
	.bMaxPacketSize0    = 64,
	.idVendor           = 0x0525,
	.idProduct          = 0xA4A1,
	.bcdDevice          = 0x0409,
	.iManufacturer      = USB_NCM_GADGET_MANUFACTURER_IDX,
	.iProduct           = USB_NCM_GADGET_PRODUCT_IDX,
	.iSerialNumber      = USB_NCM_GADGET_SERIAL_IDX,
	.bNumConfigurations = 1
};

static struct usb_config_descriptor ncm_config_desc = {
	.bLength         = USB_DT_CONFIG_SIZE,
	.bDescriptorType = USB_DT_CONFIG,
	.wTotalLength    = USB_DT_CONFIG_SIZE,
#ifdef NCM_IAP_ENABLE
	.bNumInterfaces      = 3,
#else
	.bNumInterfaces      = 2,
#endif
	.bConfigurationValue = 1,
	.iConfiguration      = USB_NCM_GADGET_CONFIG_IDX,
	.bmAttributes        = 0xc0,
	.bMaxPower           = 0xfa /* 500mA */
};

struct usb_cdc_ncm_ntb_parameters ntb_parameters = {
	cpu_to_le16(sizeof(ntb_parameters)),
	cpu_to_le16(FORMATS_SUPPORTED),
	cpu_to_le32(NTB_DEFAULT_IN_SIZE),
	cpu_to_le16(4),
	cpu_to_le16(0),
	cpu_to_le16(4),
	0,
	cpu_to_le32(NTB_OUT_SIZE),
	cpu_to_le16(4),
	cpu_to_le16(0),
	cpu_to_le16(4),
	0
};

/*
 * Use wMaxPacketSize big enough to fit CDC_NOTIFY_SPEED_CHANGE in one
 * packet, to simplify cancellation; and a big transfer interval, to
 * waste less bandwidth.
 */
static struct usb_interface_assoc_descriptor ncm_iad_desc = {
	.bLength = sizeof(ncm_iad_desc),
	.bDescriptorType = USB_DT_INTERFACE_ASSOCIATION,

	/* .bFirstInterface =	DYNAMIC, */
	.bFirstInterface = 0,	/* control + data */
	.bInterfaceCount = 2,
	.bFunctionClass = USB_CLASS_COMM,
	.bFunctionSubClass = USB_CDC_SUBCLASS_NCM,
	.bFunctionProtocol = USB_CDC_PROTO_NONE,
	.iFunction = USB_NCM_GADGET_FUNCTION_IDX
	/* .iFunction =		DYNAMIC */
};

static struct usb_cdc_header_desc ncm_header_desc = {
	sizeof(ncm_header_desc),
	USB_DT_CS_INTERFACE,
	USB_CDC_HEADER_TYPE,
	cpu_to_le16(0x0110),
};

static struct usb_cdc_union_desc ncm_union_desc = {
	.bLength = sizeof(ncm_union_desc),
	.bDescriptorType = USB_DT_CS_INTERFACE,
	.bDescriptorSubType = USB_CDC_UNION_TYPE,
	.bMasterInterface0 = 0,
	.bSlaveInterface0 = 1
	/* .bMasterInterface0 =	DYNAMIC */
	/* .bSlaveInterface0 =	DYNAMIC */
};

static struct usb_cdc_ether_desc ecm_desc = {
	.bLength = sizeof(ecm_desc),
	.bDescriptorType = USB_DT_CS_INTERFACE,
	.bDescriptorSubType = USB_CDC_ETHERNET_TYPE,

	/* this descriptor actually adds value, surprise! */
	/* .iMACAddress = DYNAMIC */
	.iMACAddress = USB_NCM_GADGET_MAC_IDX,
	.bmEthernetStatistics = cpu_to_le32(0), /* no statistics */
	.wMaxSegmentSize = cpu_to_le16(ETH_FRAME_LEN),
	.wNumberMCFilters = cpu_to_le16(0),
	.bNumberPowerFilters = 0
};

#define NCAPS	(USB_CDC_NCM_NCAP_ETH_FILTER | USB_CDC_NCM_NCAP_CRC_MODE)

static struct usb_cdc_ncm_desc ncm_desc = {
	.bLength = sizeof(ncm_desc),
	.bDescriptorType = USB_DT_CS_INTERFACE,
	.bDescriptorSubType = USB_CDC_NCM_TYPE,
	.bcdNcmVersion = cpu_to_le16(0x0100),
	/* can process SetEthernetPacketFilter */
	.bmNetworkCapabilities = NCAPS
};

static struct usb_interface_descriptor iap_intf = {
	.bLength = sizeof(iap_intf),
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 2,
	.bAlternateSetting = 0,
	.bNumEndpoints = 2,
	.bInterfaceClass = 0xff,
	.bInterfaceSubClass = 0xf0,
	.bInterfaceProtocol = 0,
	.iInterface = USB_NCM_GADGET_IAP_IDX
	/* .iInterface = DYNAMIC */
};

static struct usb_endpoint_descriptor hs_ncm_iap_in_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 3 | USB_DIR_IN,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize = cpu_to_le16(512),
	.bInterval = 0,
	.bRefresh = 0,
	.bSynchAddress = 0
};

static struct usb_endpoint_descriptor hs_ncm_iap_out_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 3 | USB_DIR_OUT,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize = cpu_to_le16(512),
	.bInterval = 0,
	.bRefresh = 0,
	.bSynchAddress = 0

};

/* the default data interface has no endpoints ... */

static struct usb_interface_descriptor ncm_data_nop_intf = {
	.bLength = sizeof(ncm_data_nop_intf),
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 0,
	.bAlternateSetting = 0,
	.bNumEndpoints = 1,
	.bInterfaceClass = USB_CLASS_COMM,
	.bInterfaceSubClass = USB_CDC_SUBCLASS_NCM,
	.bInterfaceProtocol = 0,
	.iInterface = 0
	/* .iInterface = DYNAMIC */
};
/* high speed support: */

static struct usb_endpoint_descriptor hs_ncm_notify_desc = {
	USB_DT_ENDPOINT_SIZE,
	USB_DT_ENDPOINT,
	.bEndpointAddress = 2 | USB_DIR_IN,
	.bmAttributes = USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize = cpu_to_le16(NCM_STATUS_BYTECOUNT),
	.bInterval = 9,
	0,
	0
};
static struct usb_interface_descriptor ncm_data_0_intf = {
	.bLength = sizeof(ncm_data_0_intf),
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 1,
	.bAlternateSetting = 0,
	.bNumEndpoints = 0,
	.bInterfaceClass = USB_CLASS_CDC_DATA,
	.bInterfaceSubClass = 0,
	.bInterfaceProtocol = USB_CDC_NCM_PROTO_NTB,
	.iInterface = 0
	/* .iInterface = DYNAMIC */
};

/* ... but the "real" data interface has two bulk endpoints */
static struct usb_interface_descriptor ncm_data_intf = {
	.bLength = sizeof(ncm_data_intf),
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 1,
	.bAlternateSetting = 1,
	.bNumEndpoints = 2,
	.bInterfaceClass = USB_CLASS_CDC_DATA,
	.bInterfaceSubClass = 0,
	.bInterfaceProtocol = USB_CDC_NCM_PROTO_NTB,
	.iInterface = 0
	/* .iInterface = DYNAMIC */
};

static struct usb_endpoint_descriptor hs_ncm_in_desc = {
	USB_DT_ENDPOINT_SIZE,
	USB_DT_ENDPOINT,
	.bEndpointAddress = 1 | USB_DIR_IN,
	USB_ENDPOINT_XFER_BULK,
	cpu_to_le16(512),
	0,
	0,
	0
};

static struct usb_endpoint_descriptor hs_ncm_out_desc = {
	USB_DT_ENDPOINT_SIZE,
	USB_DT_ENDPOINT,

	.bEndpointAddress = 1 | USB_DIR_OUT,
	USB_ENDPOINT_XFER_BULK,
	cpu_to_le16(512),
	0,
	0,
	0
};

static struct usb_descriptor_header *ncm_hs_function[] = {
	(struct usb_descriptor_header *) &ncm_config_desc,
	/* CDC NCM control descriptors */
	(struct usb_descriptor_header *) &ncm_iad_desc,
	(struct usb_descriptor_header *) &ncm_data_nop_intf,
	(struct usb_descriptor_header *) &ncm_header_desc,
	(struct usb_descriptor_header *) &ncm_union_desc,
	(struct usb_descriptor_header *) &ecm_desc,
	(struct usb_descriptor_header *) &ncm_desc,
	(struct usb_descriptor_header *) &hs_ncm_notify_desc,
	(struct usb_descriptor_header *) &ncm_data_0_intf,
	/* data interface, altsettings 0 and 1 */
	(struct usb_descriptor_header *) &ncm_data_intf,
	(struct usb_descriptor_header *) &hs_ncm_in_desc,
	(struct usb_descriptor_header *) &hs_ncm_out_desc
#ifdef NCM_IAP_ENABLE
	,
	(struct usb_descriptor_header *) &iap_intf,
	(struct usb_descriptor_header *) &hs_ncm_iap_in_desc,
	(struct usb_descriptor_header *) &hs_ncm_iap_out_desc
#endif

};
static unsigned char ep0_tx_data_buff[1024] = {0};
static inline void ncm_reset_values(struct f_ncm *ncm)
{
	ncm->parser_opts = &ndp16_opts;
	ncm->is_crc = false;
	ncm->ndp_sign = ncm->parser_opts->ndp_sign;
	ncm->port.cdc_filter = DEFAULT_FILTER;

	/* doesn't make sense for ncm, fixed size used */
	ncm->port.header_len = 0;
	ncm->port.fixed_out_len = le32_to_cpu(ntb_parameters.dwNtbOutMaxSize);
	ncm->port.fixed_in_len = NTB_DEFAULT_IN_SIZE;
}

static int ncm_standard_req(struct usb_ctrlrequest *crq)
{
	extern void ncm_do_notify(struct f_ncm *ncm);
	switch (crq->bRequest) {
		case USB_REQ_SET_CONFIGURATION:
			/* init bulk-in ep */
			hal_udc_ep_enable(hs_ncm_in_desc.bEndpointAddress,
					  hs_ncm_in_desc.wMaxPacketSize,
					  hs_ncm_in_desc.bmAttributes & USB_ENDPOINT_XFERTYPE_MASK);
			hal_udc_ep_enable(hs_ncm_out_desc.bEndpointAddress,
					  hs_ncm_out_desc.wMaxPacketSize,
					  hs_ncm_out_desc.bmAttributes & USB_ENDPOINT_XFERTYPE_MASK);
			hal_udc_ep_enable(hs_ncm_notify_desc.bEndpointAddress,
					  hs_ncm_notify_desc.wMaxPacketSize,
					  hs_ncm_notify_desc.bmAttributes & USB_ENDPOINT_XFERTYPE_MASK);
			function_ncm.is_open = 1;
//			ncm_do_notify(&function_ncm);
			break;
		case USB_REQ_SET_INTERFACE:
			if (crq->wValue) {
				extern int sunxi_ncm_netif_up(void);
				sunxi_ncm_netif_up();
			}
//			printk("[%s] line:%d standard req:%u\n", __func__, __LINE__, crq->bRequest);
//			printk("[%s] line:%d wValue:%u\n", __func__, __LINE__, crq->wValue);
//			printk("[%s] line:%d wIndex:%u\n", __func__, __LINE__, crq->wIndex);
//			printk("[%s] line:%d wLength:%u\n", __func__, __LINE__, crq->wLength);
		break;
		default:
		break;
	}
	return 0;
}

static int ncm_class_req(struct usb_ctrlrequest *crq)
{
	int			value = -EOPNOTSUPP;
	u16			w_index = le16_to_cpu(crq->wIndex);
	u16			w_value = le16_to_cpu(crq->wValue);
	u16			w_length = le16_to_cpu(crq->wLength);

//	printk("[%s] line:%d (crq->bRequestType << 8):%u\n", __func__, __LINE__, (crq->bRequestType << 8));
//	printk("[%s] line:%d crq->bRequest:%u\n", __func__, __LINE__, crq->bRequest);
	switch ((crq->bRequestType << 8) | crq->bRequest) {
		case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
				| USB_CDC_SET_ETHERNET_PACKET_FILTER:
			/*
			 * see 6.2.30: no data, wIndex = interface,
			 * wValue = packet filter bitmap
			 */
			if (w_length != 0 || w_index != function_ncm.ctrl_id)
				goto invalid;
			gadget_debug("packet filter %02x\n", w_value);
			/*
			 * REVISIT locking of cdc_filter.  This assumes the UDC
			 * driver won't have a concurrent packet TX irq running on
			 * another CPU; or that if it does, this write is atomic...
			 */
			function_ncm.port.cdc_filter = w_value;
			value = 0;
			break;
		/*
		 * and optionally:
		 * case USB_CDC_SEND_ENCAPSULATED_COMMAND:
		 * case USB_CDC_GET_ENCAPSULATED_RESPONSE:
		 * case USB_CDC_SET_ETHERNET_MULTICAST_FILTERS:
		 * case USB_CDC_SET_ETHERNET_PM_PATTERN_FILTER:
		 * case USB_CDC_GET_ETHERNET_PM_PATTERN_FILTER:
		 * case USB_CDC_GET_ETHERNET_STATISTIC:
		 */

		case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_GET_NTB_PARAMETERS:

			if (w_length == 0 || w_value != 0 || w_index != function_ncm.ctrl_id)
				goto invalid;
			value = w_length > sizeof(ntb_parameters)?
				sizeof(ntb_parameters) : w_length;
			hal_udc_ep_set_buf(0 | USB_DIR_IN, &ntb_parameters, value);
			printk("Host asked NTB parameters\n");
			break;

		case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_GET_NTB_INPUT_SIZE:

			if (w_length < 4 || w_value != 0 || w_index != function_ncm.ctrl_id)
				goto invalid;
			memset(ep0_tx_data_buff, 0x00, sizeof(ep0_tx_data_buff));
			put_unaligned_le32(function_ncm.port.fixed_in_len, ep0_tx_data_buff);
			value = 4;
			hal_udc_ep_set_buf(0 | USB_DIR_IN, ep0_tx_data_buff, value);
			printk("Host asked INPUT SIZE, sending %d\n",
				 function_ncm.port.fixed_in_len);
			break;

		case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_SET_NTB_INPUT_SIZE:
		{
			if (w_length != 4 || w_value != 0 || w_index != function_ncm.ctrl_id)
				goto invalid;
//			value = w_length;
//			memset(ep0_tx_data_buff, 0x00, sizeof(ep0_tx_data_buff));
//			hal_udc_ep_set_buf(0 | USB_DIR_IN, ep0_tx_data_buff, value);
			break;
		}

		case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_GET_NTB_FORMAT:
		{
			uint16_t format;

			if (w_length < 2 || w_value != 0 || w_index != function_ncm.ctrl_id)
				goto invalid;
			format = (function_ncm.parser_opts == &ndp16_opts) ? 0x0000 : 0x0001;
			value = 2;
			memset(ep0_tx_data_buff, 0x00, sizeof(ep0_tx_data_buff));
			put_unaligned_le16(format, ep0_tx_data_buff);
			hal_udc_ep_set_buf(0 | USB_DIR_IN, ep0_tx_data_buff, value);
			printk("Host asked NTB FORMAT, sending %d\n", format);
			break;
		}

		case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_SET_NTB_FORMAT:
		{
			if (w_length != 0 || w_index != function_ncm.ctrl_id)
				goto invalid;
			switch (w_value) {
			case 0x0000:
				function_ncm.parser_opts = &ndp16_opts;
				printk("NCM16 selected\n");
				break;
			case 0x0001:
				function_ncm.parser_opts = &ndp32_opts;
				printk("NCM32 selected\n");
				break;
			default:
				goto invalid;
			}
			value = 0;
			break;
		}
		case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_GET_CRC_MODE:
		{
			uint16_t is_crc;

			if (w_length < 2 || w_value != 0 || w_index != function_ncm.ctrl_id)
				goto invalid;
			is_crc = function_ncm.is_crc ? 0x0001 : 0x0000;
			put_unaligned_le16(is_crc, ep0_tx_data_buff);
			value = 2;
			memset(ep0_tx_data_buff, 0x00, sizeof(ep0_tx_data_buff));
			hal_udc_ep_set_buf(0 | USB_DIR_IN, ep0_tx_data_buff, value);
			printk("Host asked CRC MODE, sending %d\n", is_crc);
			break;
		}

		case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_SET_CRC_MODE:
		{
			int ndp_hdr_crc = 0;

			if (w_length != 0 || w_index != function_ncm.ctrl_id)
				goto invalid;
			switch (w_value) {
			case 0x0000:
				function_ncm.is_crc = false;
				ndp_hdr_crc = NCM_NDP_HDR_NOCRC;
				printk("non-CRC mode selected\n");
				break;
			case 0x0001:
				function_ncm.is_crc = true;
				ndp_hdr_crc = NCM_NDP_HDR_CRC;
				printk("CRC mode selected\n");
				break;
			default:
				goto invalid;
			}
			value = 0;
			break;
		}

		/* and disabled in ncm descriptor: */
		/* case USB_CDC_GET_NET_ADDRESS: */
		/* case USB_CDC_SET_NET_ADDRESS: */
		/* case USB_CDC_GET_MAX_DATAGRAM_SIZE: */
		/* case USB_CDC_SET_MAX_DATAGRAM_SIZE: */

		default:
invalid:
			printk("invalid control req%02x.%02x v%04x i%04x l%d\n",
				crq->bRequestType, crq->bRequest,
				w_value, w_index, w_length);
	}
	function_ncm.ndp_sign = function_ncm.parser_opts->ndp_sign |
		(function_ncm.is_crc ? NCM_NDP_HDR_CRC : 0);

	return 0;
}

static int ncm_desc_init(struct usb_function_driver *fd)
{
	uint32_t config_desc_len = 0;
	int i;
	void *buf = NULL;
	uint16_t *str;

	memset(&function_ncm, 0x00, sizeof(function_ncm));
	for (i = 0; i < sizeof(ncm_hs_function)/sizeof(ncm_hs_function[0]); i++) {
		config_desc_len += ncm_hs_function[i]->bLength;
	}
	ncm_config_desc.wTotalLength = config_desc_len;
	fd->config_desc = calloc(1, config_desc_len);
	if (!fd->config_desc) {
		gadget_err("no memory.\n");
		goto err;
	}

	buf = fd->config_desc;
	for (i = 0; i < sizeof(ncm_hs_function)/sizeof(ncm_hs_function[0]); i++) {
		memcpy(buf, ncm_hs_function[i], ncm_hs_function[i]->bLength);
		buf += ncm_hs_function[i]->bLength;
	}

	hal_udc_device_desc_init(&ncm_device_desc);
	hal_udc_config_desc_init(fd->config_desc, config_desc_len);
	/*FlushDcacheRegion(fd->config_desc, config_desc_len);*/
	for (i = 0; i < USB_NCM_GADGET_MAX_IDX; i++) {
		str = fd->strings[i] ? fd->strings[i] : (uint16_t *)ncm_string_desc[i];
		hal_udc_string_desc_init(str);
	}

	fd->class_req = ncm_class_req;
	fd->standard_req = ncm_standard_req;

#ifdef NCM_IAP_ENABLE
	fd->ep_addr = calloc(6, sizeof(uint8_t));
#else
	fd->ep_addr = calloc(4, sizeof(uint8_t));
#endif
	if (!fd->ep_addr) {
		gadget_err("no memory.\n");
		goto err;
	}
	fd->ep_addr[0] = 0;
	fd->ep_addr[1] = hs_ncm_in_desc.bEndpointAddress;
	fd->ep_addr[2] = hs_ncm_out_desc.bEndpointAddress;
	fd->ep_addr[3] = hs_ncm_notify_desc.bEndpointAddress;
#ifdef NCM_IAP_ENABLE
	fd->ep_addr[4] = hs_ncm_iap_in_desc.bEndpointAddress;
	fd->ep_addr[5] = hs_ncm_iap_out_desc.bEndpointAddress;
#endif
	ncm_reset_values(&function_ncm);
	function_ncm.hrtimer = SUNXI_TMR1;
	sunxi_timer_init(function_ncm.hrtimer);
#ifdef NCM_IAP_ENABLE
	sunxi_ncm_net_init(&function_ncm);
#endif
	{
		extern int sunxi_ncm_netif_register(struct f_ncm *ncm_function);
		sunxi_ncm_netif_register(&function_ncm);
	}
	return 0;
err:
	if (fd->config_desc) {
		free(fd->config_desc);
		fd->config_desc = NULL;
	}

	if (fd->ep_addr) {
		free(fd->ep_addr);
		fd->ep_addr = NULL;
	}

	return 0;
}

static int ncm_desc_deinit(struct usb_function_driver *fd)
{
	extern int sunxi_ncm_netif_unregister(struct f_ncm *ncm_function);
#ifdef NCM_IAP_ENABLE
	sunxi_ncm_net_deinit(&function_ncm);
#endif
	sunxi_ncm_netif_unregister(&function_ncm);
	sunxi_timer_uninit(function_ncm.hrtimer);

	if (fd->config_desc) {
		free(fd->config_desc);
		fd->config_desc = NULL;
	}

	if (fd->ep_addr) {
		free(fd->ep_addr);
		fd->ep_addr = NULL;
	}

	for (int i = 0; i < USB_NCM_GADGET_MAX_IDX; i++) {
		if (fd->strings[i] != NULL) {
			free(fd->strings[i]);
			fd->strings[i] = NULL;
		}
	}

	return 0;
}

struct usb_function_driver ncm_usb_func = {
	.name        = "ncm",
	.desc_init   = ncm_desc_init,
	.desc_deinit = ncm_desc_deinit,
};

int usb_gadget_ncm_init(void) {
	usb_gadget_function_register(&ncm_usb_func);
	return usb_gadget_function_enable("ncm");
}

int usb_gadget_ncm_deinit(void) {
	usb_gadget_function_disable("ncm");
	return usb_gadget_function_unregister(&ncm_usb_func);
}

