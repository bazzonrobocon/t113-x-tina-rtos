#include "carplay_host.h"
#include "usb_gen_hub.h"
#include "usb_core_base.h"
#include "mod_devicetable.h"

#define IPHONE_DRV_NAME		"iPhone Class"
#define IPHONE_DRV_AUTHOR		"Host Driver Author"

static struct usb_driver CarPlayDrv;

static struct usb_device_id carplay_match_table[2] = {
	{
		//match_flags
		USB_DEVICE_ID_MATCH_VENDOR|USB_DEVICE_ID_MATCH_PRODUCT,
		0x05AC,  //idVendor;
		0x12A8,  //idProduct;
	},
};

static int iPhoneDevProbe(struct usb_interface *intf, const struct usb_device_id *table_item)
{
	int ret = 0;
	unsigned char data_buff[64] = {0};
	const s32 match_table_index = table_item - carplay_match_table;
	extern s32 switch_usb_to_device(void);
	ret = usb_control_msg(interface_to_usbdev(intf), usb_rcvctrlpipe(interface_to_usbdev(intf), 0), 0x53,
												0xc0, 0x00, 0x00, data_buff, 4, USB_CTRL_GET_TIMEOUT);
	/*表明支持carplay功能*/
	printk("%s %d %s ret:%d\n", __FILE__, __LINE__, __func__, ret);
	if (ret == 4 && data_buff[0] == 1) {
		ret = usb_control_msg(interface_to_usbdev(intf), usb_sndctrlpipe(interface_to_usbdev(intf), 0), 0x51,
													0x40, 0x01, 0x00, data_buff, 0, USB_CTRL_GET_TIMEOUT);
		/*switch iphone device to host ok, us will switch to device mode*/
		printk("%s %d %s ret:%d\n", __FILE__, __LINE__, __func__, ret);
		if (ret == 0) {
//			switch_usb_to_device();
		}
	}
	return 0;
}

static void iPhoneDevRemove(struct usb_interface *intf)
{
	return;
}

static int CarPlay_Host_DriverInit(struct usb_driver *drv) {

	if (drv == NULL) {
		hal_log_err("ERR: mscDrv_init: input error\n");
		return -1;
	}

	USB_INIT_LIST_HEAD(&(drv->virt_dev_list));
	drv->func_drv_name   = IPHONE_DRV_NAME;
	drv->func_drv_auther = IPHONE_DRV_AUTHOR;
	drv->probe           = iPhoneDevProbe;
	drv->disconnect      = iPhoneDevRemove;
	drv->suspend         = NULL;
	drv->resume          = NULL;
	drv->match_table     = carplay_match_table;

	return 0;
}

int Iphone_Function_Init(void) {
	int ret = 0;

	memset(&CarPlayDrv, 0, sizeof(struct usb_driver));
	CarPlay_Host_DriverInit(&CarPlayDrv);

	ret = usb_host_func_drv_reg(&CarPlayDrv);
	return ret;
}

int Iphone_Function_Exit(void) {
	int ret = 0;

	ret = usb_host_func_drv_unreg(&CarPlayDrv);
	if(ret != 0){
		hal_log_err("Exit driver %s failed\n", CarPlayDrv.func_drv_name);
		return -1;
	}
	memset(&CarPlayDrv, 0, sizeof(struct usb_driver));
	return ret;
}

