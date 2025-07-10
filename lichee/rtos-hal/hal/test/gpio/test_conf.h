#include <hal_gpio.h>

/* config the gpio to test */
#if defined(CONFIG_ARCH_SUN8IW19P1) || defined(CONFIG_ARCH_SUN55IW3) || defined(CONFIG_ARCH_SUN60IW1)
#define GPIO_TEST		GPIO_PC0
#elif defined(CONFIG_ARCH_SUN8IW20)
#define GPIO_TEST		GPIO_PB0
#else
#define GPIO_TEST		GPIO_PA1
#endif
