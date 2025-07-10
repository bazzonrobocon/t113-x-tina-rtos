#include <sys/ioctl.h>
#include <fcntl.h>
#include "usb_test.h"
#include "mod_usbhost.h"


int usb_test_cmd_mouse(int argc, const char **argv)
{
	int fd,val=1,enable = 0;

	fd = open("/dev/mouse", O_RDWR);

	if (argc != 3)
		return -1;

	enable = atoi(argv[2]);
	switch (enable) {
		case 0:
			if (ioctl(fd, USBH_HID_USER_CTRL_CMD_TEST_STOP, &val) < 0) {
				printf(" mouse test end  fail!!!\n");
			}
			break;
		case 1:
			if (ioctl(fd, USBH_HID_USER_CTRL_CMD_TEST_START, &val) < 0) {
				printf(" mouse test start  fail!!!\n");
			}
			break;
		default:
			printf("ERR: usb mouse test start/stop error!\n");
			break;
	}

	close(fd);
	printf("close.......\n");
	return 0;
}