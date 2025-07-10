#include "sensor_register.h"

struct sensor_cfg_array sensor_array[] = {
#ifdef CONFIG_SENSOR_GC4663_MIPI
        {"gc4663_mipi", &gc4663_core},
#endif
#ifdef CONFIG_SENSOR_GC2053_MIPI
        {"gc2053_mipi", &gc2053_core},
#endif
#ifdef CONFIG_SENSOR_GC2083_MIPI
        {"gc2083_mipi", &gc2083_core},
#endif
#ifdef CONFIG_SENSOR_GC0406_MIPI
        {"gc0406_mipi", &gc0406_core},
#endif
#ifdef CONFIG_SENSOR_SC2355_MIPI
        {"sc2355_mipi", &sc2355_core},
#endif
#ifdef CONFIG_SENSOR_SC2336_MIPI
        {"sc2336_mipi", &sc2336_core},
#endif
#ifdef CONFIG_SENSOR_SC3336_MIPI
        {"sc3336_mipi", &sc3336_core},
#endif
#ifdef CONFIG_SENSOR_SC5336_MIPI
        {"sc5336_mipi", &sc5336_core},
#endif
#ifdef CONFIG_SENSOR_GC1054_MIPI
        {"gc1054_mipi", &gc1054_core},
#endif
#ifdef CONFIG_SENSOR_GC1084_MIPI
        {"gc1084_mipi", &gc1084_core},
#endif
#ifdef CONFIG_SENSOR_IMX319_MIPI
        {"imx319_mipi", &imx319_core},
#endif
#ifdef CONFIG_SENSOR_IMX335_MIPI
        {"imx335_mipi", &imx335_core},
#endif
#ifdef CONFIG_SENSOR_SC035HGS_MIPI
        {"sc035hgs_mipi", &sc035hgs_core},
#endif
#ifdef CONFIG_SENSOR_TP9950_MIPI
        {"tp9950_mipi", &tp9950_mipi_core},
#endif
#ifdef CONFIG_SENSOR_TP9953_DVP
        {"tp9953_dvp",  &tp9953_dvp_core},
#endif
#ifdef CONFIG_SENSOR_TP2815_MIPI
        {"tp2815_mipi",  &tp2815_core},
#endif
#ifdef CONFIG_SENSOR_BF2253L_MIPI
        {"bf2253l_mipi", &bf2253l_core},
#endif
#ifdef CONFIG_SENSOR_N5_DVP
        {"n5",  &n5_core},
#endif
};

struct sensor_fuc_core *find_sensor_func(char *sensor_name)
{
        int i;

        if (!ARRAY_SIZE(sensor_array)) {
                vin_err("%s:sensor_array is NULL\n", __func__);
                return NULL;
        }

	for (i = 0; i < ARRAY_SIZE(sensor_array); i++) {
                if (!strncmp(sensor_name, sensor_array[i].sensor_name, 6)) {
                        vin_print("%s: find %s sensor core function!\n", __func__, sensor_name);
                        return sensor_array[i].sensor_core;
                }
        }

        return NULL;
}
