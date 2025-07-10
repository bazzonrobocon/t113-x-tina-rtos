1.demo的使用
(1)测试脚本说明
命令格式：di_test {width} {height} {format} {mode}
脚本内的测试用例的参数说明，举例：
di_demo 1920 1080 1 0
参数1：测试图片水平像素（即长度）
参数2：测试图片垂直像素（即宽度）
参数3: 测试文件的像素格式
enum di_pixel_format {
	DI_FORMAT_YUV420_PLANNER = 0,
	DI_FORMAT_YUV420_NV21, /*UV UV UV*/
	DI_FORMAT_YUV420_NV12, /*VU VU VU*/

	DI_FORMAT_YUV422_PLANNER,
	DI_FORMAT_YUV422_NV61, /*UV UV UV*/
	DI_FORMAT_YUV422_NV16, /*VU VU VU*/

	DI_FORMAT_NUM,
};
参数4：DI采用的算法已经输出的数据流个数
enum {
	DIV1XX_MODE_BOB_OUT2 = 0, //场内插值算法，不常用，一张I源图片可以DI出两张P源图片
	DIV1XX_MODE_BOB_OUT1, //场内插值算法，不常用，一张I源图片可以DI出一张P源图片
	DIV1XX_MODE_MD_OUT2, //运动检测算法，常用，一张I源图片可以DI出两张P源图片
	DIV1XX_MODE_MD_OUT1, //运动检测算法，常用，一张I源图片可以DI出一张P源图片
	DIV1XX_MODE_NUM,
};

(2)测试源文件
本demo将所有的测试源图片文件的路径均包含在头文件test_file.h中

(3)使用流程
步骤一：用adb push将测试源图片文件推入文件系统中
特别注意，推入后，测试源图片所在的目录要与test_file.h中描述的文件路径相同
源图片可以在linux-test-examples/deinterlace/pic中找到

由于文件系统的大小可能有限，不一定要将所有的测试图片一次性推完，可以在测试时，根据每个测试用例需要什么就推什么。

步骤二：运行测试用例命令

步骤三：如果程序跑成功，会在相应的目录下输出DI后的文件，可查看确认

2. RTOS验证方法：
（1）测试参数说明：
diMode:
enum {
	DIV3X_MODE_BOB = 0,   //简单去隔行功能
	DIV3X_MODE_OUT2_TNR,  //60HZ输出 + 降噪功能
	DIV3X_MODE_OUT2,      //60HZ输出 + 去隔行
	DIV3X_MODE_OUT1_TNR,  //30HZ输出 + 降噪功能
	DIV3X_MODE_OUT1,      //30HZ输出 + 去隔行
	DIV3X_MODE_ONLY_TNR,  //纯降噪
};

srcFormat/dstFormat
enum di_pixel_format {
	DI_FORMAT_YUV420_PLANNER = 0,
	DI_FORMAT_YUV420_NV21, /*UV UV UV*/
	DI_FORMAT_YUV420_NV12, /*VU VU VU*/

	DI_FORMAT_YUV422_PLANNER,
	DI_FORMAT_YUV422_NV61, /*UV UV UV*/
	DI_FORMAT_YUV422_NV16, /*VU VU VU*/

	DI_FORMAT_NUM,
};
isSingleField ： 0/1 0代表输入数据一帧，1代表输入数据为一场（1尚未验证）
(2) 进入rtos后，输入以下命令进行验证
 test_di300_30HZ  5 176 144 0 176 144 0 0 0 176 144 0 0 176 144 0  验证纯TNR模式
 test_di300_30HZ  0 176 144 0 176 144 0 0 0 176 144 0 0 176 144 0  验证纯dit模式
 test_di300_30HZ  3 176 144 0 176 144 0 0 0 176 144 0 0 176 144 0  验证30Hz+TNR模式
 test_di300_60HZ 2 176 144 0 176 144 0 0 0 176 144 0 0 176 144 0 0 0 验证60Hz模式
 test_di300_60HZ 1 176 144 0 176 144 0 0 0 176 144 0 0 176 144 0 0 0 验证60Hz+TNR模式

 观察输出log，dump出来的字节信息数据是否全部为0x00（outbuffer初始化的值），若不是则说明有数据输出；观察程序运行后打印的irq count是否增大，test_di300_30HZ 运行一次增加11，test_di300_60HZ运行一次增加10.