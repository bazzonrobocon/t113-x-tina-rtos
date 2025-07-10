subdir-ccflags-y +=	-I drivers/rtos-hal/hal/source/usb/gadget
subdir-ccflags-y +=	-I drivers/rtos-hal/hal/source/usb/udc
subdir-ccflags-y +=	-I drivers/indlude/drv
# subdir-ccflags-y +=	-I$(srctree)/include/melis/ekernel/pthread
ifdef CONFIG_DRIVERS_USB_GADGET_MSC
subdir-ccflags-y +=	-I drivers/rtos-hal/hal/source/usb/gadget/function/msc
endif
ifdef CONFIG_DRIVERS_USB_GADGET_NCM
subdir-ccflags-y +=	-I drivers/rtos-hal/hal/source/usb/gadget/function/ncm
subdir-ccflags-y +=	-I drivers/rtos-hal/hal/source/timer/
subdir-ccflags-y +=	-I $(MELIS_BASE)/ekernel/components/aw/usb_gadget/ncm/
endif
obj-${CONFIG_DRIVERS_USB_GADGET_ADB} += function/adb.o
obj-${CONFIG_DRIVERS_USB_GADGET_MTP} += function/mtp.o
obj-${CONFIG_DRIVERS_USB_GADGET_UAC} += function/uac.o
obj-${CONFIG_DRIVERS_USB_GADGET_NCM} += function/ncm/f_ncm_dev.o
obj-${CONFIG_DRIVERS_USB_GADGET_NCM} += function/ncm/function_ncm.o
obj-${CONFIG_DRIVERS_USB_GADGET_MSC} += function/msc/msc.o
obj-${CONFIG_DRIVERS_USB_GADGET_MSC} += function/msc/scsi.o
obj-${CONFIG_DRIVERS_USB_GADGET_MSC} += function/msc/usb_slave_msc.o
obj-y += gadget.o
obj-y += gadget_hal.o
