#/bin/bash
export TA_DEV_KIT_DIR=~/optee_os/out/arm-plat-vexpress/export-ta_arm32
export CROSS_COMPILE=arm-linux-gnueabihf-
cd ../ta
make clean
make
