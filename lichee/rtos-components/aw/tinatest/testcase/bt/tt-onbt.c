
#include <stdint.h>
#include <tinatest.h>
#include <stdio.h>
#include "tt-test.h"

//bt on test
//cmd:tt bt_on
int tt_onbttest(int argc, char **argv)
{
	int ret = 0;
	printf("\n========TINATEST FOR BT ON========\n");
	ret = cmd_ble_test(argc, argv);
	if (ret) {
		printf("\n======TINATEST FOR BT ON FAILD=======\n");
		return -1;
	}
	printf("\n======TINATEST FOR BT ON OK=======\n");

	vTaskDelay(100);
scantest:
	printf("\n========TINATEST FOR BT SCAN========\n");
	printf("\n============ BT SCAN ON ============\n");
	ret = cmd_ble_scan(0);
	if (ret) {
		printf("scan on faild\n");
		return -1;
	}
	vTaskDelay(1000);
	printf("\n============ BT SCAN OFF ===========\n");
	ret = cmd_ble_scan(1);
	if (ret) {
		printf("scan off faild\n");
		return -1;
	}
	vTaskDelay(100);
	printf("\n============ BT SCAN OK ===========\n");

offtest:
	printf("\n========TINATEST FOR BT OFF========\n");
	ret = cmd_advertise_off();
	if(ret == -ENOEXEC) {
		printf("advertise off faild.\n");
		return -1;
	}
	vTaskDelay(100);
	bt_gatt_service_unregister(&vnd_svc);
	bt_gatt_service_unregister(&vnd1_svc);
	printf("\n======== BT OFF ========\n");
	printf("\n========TINATEST FOR BT ALL OK ========\n");
	return 0;
}

testcase_init(tt_onbttest, bt_on, bt on for tinatest);