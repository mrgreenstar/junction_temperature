#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/delay.h> 
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

struct fpga_attrs {
    struct device_node *dev_node;
    struct grif_fpga *fpga;
    struct regmap *dev_regmap;
    struct fpga_feature *hwmon_feature;
};

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

static int hwmon_read(struct device          *hwmon_dev,
                      enum hwmon_sensor_types type,
                      u32                     attr,
                      int                     channel,
                      long                   *val)
{
    unsigned int reg_val;
    struct fpga_attrs* info = hwmon_dev->driver_data;
    /* Indexes are decimal equivalents. 
    Values are junction temperature in degree celsius.*/
    static const int16_t junction_temps[] = {
        -58, -56, -54, -52, -45, -44, -43, -42, -41, -40, -39, -38, -37, -36,
        -30, -20, -10, -4, 0, 4, 10, 21, 22, 23, 24, 25, 26, 27, 28, 29, 40, 50,
        60, 70, 76, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 95, 96, 97, 98, 99,
        100, 101, 102, 103, 104, 105, 106, 107, 108, 116, 120, 124, 128, 132
    };

    switch (attr) {
    case hwmon_temp_input:
        /* Mask and val are 0x1 to update first bit */
        regmap_update_bits(info->dev_regmap, (info->hwmon_feature->cr_base)*2, 0x1, 0x1);
        /* Wait 70 useconds before get result */
        udelay(70);
        /* Get junction temp from ECP5_TEMP_MON_TEMP_SR */
        regmap_read(info->dev_regmap, (info->hwmon_feature->sr_base + 1)*2, &reg_val);
        regmap_update_bits(info->dev_regmap, (info->hwmon_feature->cr_base)*2, 0x1, 0x0);
        /* Checking 7 bit */
        if ((reg_val >> 7) & 1) {
            *val = junction_temps[reg_val & 0x3f];
        }
        else {
            return -EAGAIN;
        }
        return 0;

    default:
        return -EOPNOTSUPP;
    }
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
    struct fpga_attrs *info;

    info = devm_kzalloc(dev, sizeof(struct fpga_attrs), GFP_KERNEL);
    info->dev_node = of_find_node_by_name(NULL, "junction_temperature_device");
    if (!info->dev_node) {
        pr_err("Can not get device node from tree\n");
        goto err_node;
    }

    info->fpga = get_grif_fpga(info->dev_node);
    if (!info->fpga) {
        pr_err("Can not get grif_fpga\n");
        goto err_fpga;
    }

    info->hwmon_feature = grif_fpga_get_feature(info->fpga, FPGA_FEATURE_ECP5_TEMP_MON);
    if (!info->hwmon_feature) {
        pr_err("Can not get feature\n");
        goto err_feature;
    }

    info->dev_regmap = dev_get_regmap(info->fpga->dev, NULL);
    if (!info->dev_regmap) {
        pr_err("Can not get regmap\n");
        goto err_regmap;
    }
    
    hwmon_dev = devm_hwmon_device_register_with_info(
        dev, "junction_temperature_driver", info, &driver_hwmon_info, NULL
    );
    if (IS_ERR(hwmon_dev)) {
        pr_err("Can not create hwmon device\n");
        goto err_probe;
    }
    
    dev_set_drvdata(&(p_device->dev), info);
    printk(KERN_INFO "Junction temperature monitor is started\n");
    return 0;


err_probe:
    dev_err(hwmon_dev, "Probe error\n");
err_regmap:
err_feature:
err_fpga:
    of_node_put(info->dev_node);
err_node:
    return -ENODEV;
}

int junction_temp_remove(struct platform_device *p_device)
{
    struct fpga_attrs *info = platform_get_drvdata(p_device);
    of_node_put(info->dev_node);
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
    .remove = junction_temp_remove,
};
module_platform_driver(junction_temp_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("hwmon register with info");
