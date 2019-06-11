#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/random.h>
#include <linux/regmap.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/time.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include <grif_fpga_mgr/grif_fpga.h>

#define DRIVER_NAME "junction_temperature_driver"
#define FPGA_FEATURE_ECP5_TEMP_MON 37

static umode_t hwmon_is_visible(const void             *data,
                                enum hwmon_sensor_types type,
                                u32                     attr,
                                int                     channel)
{
    switch(attr) {
    case hwmon_temp_input:
        /* 0444 is numeric notation for read-only file */
        return 0444;

    default:
        return 0;
    }
}

static int hwmon_read(struct device          *dev,
                      enum hwmon_sensor_types type,
                      u32                     attr,
                      int                     channel,
                      long                   *val)
{
    unsigned int reg_val;
    long start_time_us;
    struct device_node *dev_node;
    struct grif_fpga *fpga;
    struct regmap *dev_regmap;
    struct fpga_feature *hwmon_feature;
    struct timeval time;
    /* Indexes are decimal equivalents. 
    Values are junction temperature in degree celsius.*/
    static const int16_t junction_temps[] = {
        -58, -56, -54, -52, -45, -44, -43, -42, -41, -40, -39, -38, -37, -36,
        -30, -20, -10, -4, 0, 4, 10, 21, 22, 23, 24, 25, 26, 27, 28, 29, 40, 50,
        60, 70, 76, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 95, 96, 97, 98, 99,
        100, 101, 102, 103, 104, 105, 106, 107, 108, 116, 120, 124, 128, 132
    };

    dev_node = of_find_node_by_name(NULL, "junction_temperature_device");
    if (!dev_node) {
        pr_err("Cannot get device node from tree\n");
        goto err_no_node;
    }

    fpga = get_grif_fpga(dev_node);
    if (!fpga) {
        pr_err("Cannot get grif_fpga\n");
        goto err_fpga;
    }

    hwmon_feature = grif_fpga_get_feature(fpga, FPGA_FEATURE_ECP5_TEMP_MON);
    if (!hwmon_feature) {
        pr_err("Cannot get feature\n");
        goto err_no_feature;
    }

    dev_regmap = dev_get_regmap(fpga->dev, NULL);
    if (!dev_regmap) {
        pr_err("Cannot get regmap\n");
        goto err_regmap;
    }

    switch (attr) {
    case hwmon_temp_input:
        /* Mask and val are 0x1 to update first bit */
        regmap_update_bits(dev_regmap, (hwmon_feature->cr_base)*2, 0x1, 0x1);
        /* Wait 70 useconds before get result */
        do_gettimeofday(&time);
        start_time_us = time.tv_usec;
        while (time.tv_usec < start_time_us + 70) {
            do_gettimeofday(&time);
        }
        /* Get junction temp from ECP5_TEMP_MON_TEMP_SR */
        regmap_read(dev_regmap, (hwmon_feature->sr_base + 1)*2, &reg_val);
        /* Checking 7 bit */
        if ((reg_val >> 7) & 1) {
            *val = junction_temps[reg_val & 0x3f];
            regmap_update_bits(dev_regmap, (hwmon_feature->cr_base)*2, 0x1, 0x0);
        }
        else {
            return -EAGAIN;
        }

        of_node_put(dev_node);
        return 0;

    default:
        return -EOPNOTSUPP;
    }

    err_no_feature:
    err_no_node:
        return -1;
    err_fpga:
        of_node_put(dev_node);
        return -ENODEV;
    err_regmap:
        of_node_put(dev_node);
        return -1;
}

static const u32 temp_chip_config[] = {
    HWMON_C_REGISTER_TZ | HWMON_C_UPDATE_INTERVAL,
    0,
};

static const struct hwmon_channel_info device_chip = {
    .type = hwmon_chip,
    .config = temp_chip_config,
};

static const u32 temp_config[] = {
    HWMON_T_INPUT,
    0,
};

static const struct hwmon_channel_info temp = {
    .type = hwmon_temp,
    .config = temp_config,
};

static const struct hwmon_channel_info *driver_info[] = {
    &device_chip,
    &temp,
    NULL
};

static const struct hwmon_ops driver_hwmon_ops = {
    .is_visible = hwmon_is_visible,
    .read = hwmon_read,
};

static const struct hwmon_chip_info driver_hwmon_info = {
    .ops = &driver_hwmon_ops,
    .info = driver_info,
};

int junction_temp_probe(struct platform_device *p_device)
{
    struct device *dev = &p_device->dev;
    struct device *hwmon_dev;
    
    hwmon_dev = devm_hwmon_device_register_with_info(
        dev, "junction_temperature_driver", NULL, &driver_hwmon_info, NULL
    );
    if (IS_ERR(hwmon_dev)) {
        dev_err(hwmon_dev, "Probe error\n");
        return PTR_ERR(hwmon_dev);
    }
    printk(KERN_INFO "Junction temperature monitor is started\n");
    return 0;
}

static const struct of_device_id juction_of_match_table[] = {
	{.compatible = "linux,junction_temperature_driver"},
	{}
};
MODULE_DEVICE_TABLE(of, juction_of_match_table);


static struct platform_driver junction_temp_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = juction_of_match_table,
    },
    .probe = junction_temp_probe,
};
module_platform_driver(junction_temp_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("hwmon register with info");