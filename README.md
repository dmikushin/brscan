This repository contains Brother scanner backends for `libsane`, which are also tested to work on Raspberry Pi. The source code originates from the Brother [download page](http://www.brother.com/cgi-bin/agreement/agreement.cgi?dlfile=http://www.brother.com/pub/bsc/linux/dlf/brscan3-src-0.2.11-5.tar.gz&lang=English_source) and has been significantly cleaned.

The original Brother distribution shipped two proprietary x86-64 binary blobs (`libbrscandec.so` and `libbrcolm.so`) with no source code. Both have been fully reverse-engineered and replaced with open-source C implementations, enabling compilation for any architecture (ARM, RISC-V, etc.).

## Prerequisites

```
sudo apt install libsane-dev libusb-dev libncurses5-dev pkg-config
```

## Building & Installing

```
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
make -j4
sudo make install
sudo sh -c "echo brother >> /etc/sane.d/dll.conf"
```

## Testing

Add udev rule for your printer (use the appropriate ID) and reboot:

```
lsusb | grep Brother
Bus 005 Device 005: ID 04f9:02d0 Brother Industries, Ltd
```

```
cat /etc/udev/rules.d/60-libsane.rules
# Brother scanners
SUBSYSTEM=="usb", ENV{DEVTYPE}=="usb_device", ATTRS{idVendor}=="04f9", ATTRS{idProduct}=="02d0", GROUP="lp", ENV{libsane_matched}="yes", MODE="0666"
```

Ensure the udev device has `rw` permissions (reboot might be needed):

```
ls -l /dev/bus/usb/00* | grep -C 3 lp
/dev/bus/usb/004:
total 0
crw-rw-r-- 1 root root 189, 384 Feb  7 20:56 001
crw-rw-rw- 1 root lp   189, 385 Feb  7 20:59 002
```

Ensure the Brother backend is loaded, and the scanner is detected:

```
SANE_DEBUG_DLL=5 scanimage -L
[dll] add_backend: adding backend `brother'
[dll] load: searching backend `brother' in `/usr/lib/aarch64-linux-gnu/sane:/usr/lib/sane'
[dll] load: trying to load `/usr/lib/aarch64-linux-gnu/sane/libsane-brother.so.1'
[dll] load: couldn't open `/usr/lib/aarch64-linux-gnu/sane/libsane-brother.so.1' (No such file or directory)
[dll] load: trying to load `/usr/lib/sane/libsane-brother.so.1'
[dll] load: dlopen()ing `/usr/lib/sane/libsane-brother.so.1'
[dll] init: initializing backend `brother'
[dll] init: backend `brother' is version 1.0.1
[dll] sane_exit: calling backend `brother's exit function
device `brother:bus5;dev1' is a Brother DCP-1510 USB scanner
```

## Testing

```
scanimage -T
```

## Bugs

For DCP1510 `QueryScannerInfo()` is failing during `sane_brother_start()`:

```
scanimage: sane_start: Invalid argument
```


