#include <generated/version.h>

#define VERSION_SEC_NAME               ".version_table"
#define __version                       __attribute__((section(VERSION_SEC_NAME)))

__version const char rtos_version_msg[] = "UTS - "SDK_UTS_VERSION"\n"
                                          "Compile Time - "SDK_COMPILE_TIME;

