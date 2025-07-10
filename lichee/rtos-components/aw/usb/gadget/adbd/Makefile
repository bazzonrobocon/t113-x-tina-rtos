obj-$(CONFIG_COMPONENTS_USB_GADGET_ADBD) += adb.o
obj-$(CONFIG_COMPONENTS_USB_GADGET_ADBD) += usb.o
obj-$(CONFIG_COMPONENTS_USB_GADGET_ADBD) += adb_misc.o
obj-$(CONFIG_COMPONENTS_USB_GADGET_ADBD) += transport.o
obj-$(CONFIG_COMPONENTS_USB_GADGET_ADBD) += transport_usb.o
obj-$(CONFIG_COMPONENTS_USB_GADGET_ADBD) += service.o
obj-$(CONFIG_COMPONENTS_USB_GADGET_ADBD) += file_sync_service.o
obj-$(CONFIG_COMPONENTS_USB_GADGET_ADB_FORWARD) += sockets.o
obj-$(CONFIG_COMPONENTS_USB_GADGET_ADB_LOCAL_TRANSPORT) += transport_local.o
obj-$(CONFIG_COMPONENTS_USB_GADGET_ADB_LOCAL_TRANSPORT) += socket_inaddr_any_server.o

ifeq ($(CONFIG_COMPONENTS_MULTI_CONSOLE), y)
obj-$(CONFIG_COMPONENTS_USB_GADGET_ADBD) += adb_shell.o
endif

ifeq ($(CONFIG_COMPONENTS_USB_GADGET_ADB_FORWARD), y)
CFLAGS += -DADB_SUPPORT_SOCKET_API
ifeq ($(CONFIG_COMPONENTS_LWIP_2_1_2), y)
CFLAGS += -I components/common/thirdparty/network/lwip-2.1.2/src/include
else
CFLAGS += -I components/common/thirdparty/network/lwip-2.0.3/src/include
endif
CFLAGS += -I components/common/aw/xradio/include
endif

ifeq ($(CONFIG_COMPONENTS_USB_GADGET_ADB_FORWARD_WITHOUT_SOCKET), y)
CFLAGS += -DADB_SOCKET_UNUSED
endif
