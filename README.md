# junction_temperature

Instructions
------------

1) Add node to DT (i have used DT in ```stcmtk-smartsfp-2.1.dts``` file and added node as a root child):
```
junction_temperature_device {
    compatible = "linux,junction_temperature_driver";
	target-fpga = <&fpga0>;
    status = "okay";
};
```
2) Build module using Makefile.  
    Change ```kernel-path``` variable in Makefile to path where kernel sources are placed.  
    Don`t forget to export these variables:
    ```
    export ARCH=arm CROSS_COMPILE=/opt/gcc-linaro-6.3.1-2017.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf- LOADADDR=0x82000000 DEBFULLNAME="systeam" DEBEMAIL="ttt" UTS_MACHINE=armhf KBUILD_IMAGE=uImage KDEB_CHANGELOG_DIST="jessie"
    ```

3) Build kernel and install it.
4) Add ```junction_temp.ko``` to ```/lib/modules/4.14.22-stcmtk-0.1.27/kernel/drivers/hwmon/```  
    Create hwmon directory if it doesn`t exist.
5) Use ```depmod -a``` and reboot.


Usage
-----

To check FPGA`s junction temperature you need:  
    1) Find hwmon device in ```/sys/class/hwmon``` (required device is named as "junction_temperature_driver")  
    2) Read file ```temp1_input``` to see junction temperature in degree celsius.
