# balong-usbdload-rageworx-mod

* A modified source for compatible with MinGW-W64 and most of Linux.
* This is not an original development repo, forked from origin.

### About

Balong-usbdload is an emergency USB boot loader utility for Huawei LTE modems and routers with Balong V2R7, V7R11 and V7R22 chipsets.  
It loads external boot loader/firmware update tool file (usbloader.bin) via emergency serial port available if the firmware is corrupted or boot pin (test point) is shorted to the ground.

**This utility can make your device unbootable!**  
Use it only if you fully understand all risks and consequences. In case of any issues, you're on your own. Do not expect any help.

### Where to get USB loader files (usbloader.bin)?

USB loader files are often found in a Huawei technologic firmware releases with **99** in version number (i.e. 21.170.**99**.03.00).

Some USB loaders are available in this repository (in bins directory), along with patched versions (usblsafe.bin).

### What is usblsafe.bin?

Original Huawei USB loaders erase NAND flash when loaded.  usblsafe.bin "safe" loaders are patched to disable flash erasure procedure. You should never need to erase flash in normal circumstances as it would remove all custom factory data (IMEI, S/N, radio calibration).

This repository contains "loader-patch" automatic patcher to convert usbloader.bin to usblsafe.bin. Moreover, balong-usbdload would patch "unsafe" usb loaders automatically, and if it failed to do so, won't allow you to load unpatched loaders without `-c` flag to prevent flash erasure.

### English user interface only.

This repo sources not support Russian, only English.

### Build binary

* Linux
```
$ make -f mkfiles/make.linux
```
* MinGW-W64
```
$ make -f mkfiles/make.mingw
```

### Getting help

This repository is for balong-usbdload utility development only. Only questions about the utility itself are appropriate.

* Please **DO NOT ASK** for usb loader files in this repository!

This repository contains all loaders we have. We can't "make" them if loader for your model is missing. The only way to find loader is to find technologic firmware. No, we don't have it either.  
You will be banned for asking questions about loaders in commit comments or pull requests. This is not a forum. Thanks for understanding.

* Please **DO NOT ASK** boot pin questions.

We have no idea where boot pin is on your model. Look [here](https://routerunlock.com/boot-pin-of-different-huawei-hi-silicon-modem-and-router/) or go search the internet.

**To sum it up**: feel free to ask questions about the program itself; Report bugs if it could be reproduced in Linux. No questions about loader files allowed. Pull requests with functionality changes or bug fixes are welcome.
