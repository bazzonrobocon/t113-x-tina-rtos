#include "f_ncm_dev.h"
#include "gadget.h"

static rt_err_t sunxi_ncm_net_open(rt_device_t dev, rt_uint16_t oflag)
{
	gadget_debug("here\n");
	return 0;
}

static rt_err_t sunxi_ncm_net_close(rt_device_t dev)
{
	gadget_debug("here\n");
	return 0;
}

static rt_size_t sunxi_ncm_net_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size)
{
	gadget_debug("here");
	return usb_gadget_function_read(5, (char *)buffer, size);
}

static rt_size_t sunxi_ncm_net_write(rt_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
	gadget_debug("here");
	return usb_gadget_function_write(4, (char *)buffer, size);
}

static rt_err_t sunxi_ncm_net_control(rt_device_t dev, int cmd, void *args)
{
	gadget_debug("here");
	return 0;
}

int sunxi_ncm_net_init(struct f_ncm *ncm)
{
	int ret = 0;

	ncm->dev = rt_device_create(RT_Device_Class_USBDevice, 0);
	if (!ncm->dev) {
		gadget_err("rt_device_create fail\n");
		return ret;
	}
	// mkdevice
	ncm->dev->open = sunxi_ncm_net_open;
	ncm->dev->close = sunxi_ncm_net_close;
	ncm->dev->read = sunxi_ncm_net_read;
	ncm->dev->write = sunxi_ncm_net_write;
	ncm->dev->control = sunxi_ncm_net_control;

	ret = rt_device_register(ncm->dev, "ncm", RT_DEVICE_FLAG_RDWR);
	if (ret != RT_EOK) {
		rt_device_destroy(ncm->dev);
		goto err;
	}
	return 0;

err:
	return -1;
}
int sunxi_ncm_net_deinit(struct f_ncm *ncm)
{
	if (ncm->dev) {
		rt_device_unregister(ncm->dev);
		rt_device_destroy(ncm->dev);
	}
	return 0;
}

