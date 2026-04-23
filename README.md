# brscan — Open-Source Brother Scanner Driver

Fully open-source SANE backend for Brother MFC/DCP scanners, tested on **Raspberry Pi** and **NanoPi NEO** (ARM). Produces pixel-perfect scans on Brother DCP-1510 without any proprietary binary blobs.

The source code originates from the Brother [download page](http://www.brother.com/cgi-bin/agreement/agreement.cgi?dlfile=http://www.brother.com/pub/bsc/linux/dlf/brscan3-src-0.2.11-5.tar.gz&lang=English_source) and has been significantly cleaned and extended.

## What was done

The original Brother distribution shipped two **proprietary x86-64 binary blobs** (`libbrscandec.so` and `libbrcolm.so`) with no source code. Both have been fully reverse-engineered and replaced with open-source C implementations:

- **libbrcolm** (color matching) — written from scratch by analyzing the original blob's disassembly. Implements 3D trilinear LUT interpolation with optional gamma pre-correction. Verified byte-for-byte against the original blob (21/21 tests).

- **libbrscandec** (scan decoder / resolution changer) — fixed from Ghidra decompilation by verifying every exported function against the original disassembly. Fixed 6 classes of Ghidra decompilation errors. Verified byte-for-byte (6/6 tests across same-reso, upscale, downscale, B&W, color modes).

Additionally, the **DCP-1510 scan protocol** (brscan4) was reverse-engineered from `libsane-brother4.so.1.0.7`:

- **Mono packbits** wire format: `[10-byte wrapper][2-byte LE length][packbits data]`, with the per-block 1-byte status code (`0x80` Page End, `0x81` NextPage, `0x83`/`0xE3` Cancel) appearing inline between frames.
- **24-bit Color** is a single **baseline JPEG stream** framed into per-block wrappers. The driver stages the payload to a temp file and decodes via libjpeg with `out_color_space = JCS_RGB`.
- **EOF detection** propagates the in-stream `0x80` byte to `ProcessMain`'s `GetStatusCode` path so `SCAN_EOF` is raised cleanly (replacing the prior heuristic that lost trailing scanlines).
- **USB session teardown** mirrors the reference `CloseDevice` exactly — `BREQ_GET_CLOSE` then `usb_release_interface(1)`, with no stray `usb_set_altinterface(0)` between them. This eliminates the firmware-side `BCOMMAND_RETURN=0x80` that previously required a power cycle between scans.
- USB endpoint mapping (EP 0x85 IN / EP 0x04 OUT) and I-command response parsing with variable-length headers.

## Supported Models

Tested on **Brother DCP-1510**. Should work on other Brother MFC/DCP models listed in `data/Brsane.ini`. Models with `seriesNo >= 10` use the brscan4 protocol with the new line framing format.

## Prerequisites

```
sudo apt install libsane-dev libusb-dev libjpeg-dev pkg-config cmake gcc
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

Pre-built binaries for amd64, arm64, and armv7 are published as GitHub releases — see [Releases](https://github.com/dmikushin/brscan/releases).

## USB Permissions

Add a udev rule for your scanner (use the appropriate product ID):

```
lsusb | grep Brother
# Bus 004 Device 009: ID 04f9:02d0 Brother Industries, Ltd DCP-1510
```

```
sudo tee /etc/udev/rules.d/60-brother-scanner.rules << 'EOF'
SUBSYSTEM=="usb", ENV{DEVTYPE}=="usb_device", ATTRS{idVendor}=="04f9", ATTRS{idProduct}=="02d0", GROUP="lp", ENV{libsane_matched}="yes", MODE="0666"
EOF
sudo udevadm control --reload-rules
```

## Scanning

Verify the scanner is detected:

```
scanimage -L
# device `brother:bus4;dev1' is a Brother DCP-1510 USB scanner
```

Scan a full A4 page in grayscale:

```
scanimage --mode "True Gray" --resolution 200 -x 210 -y 297 --format=pnm > scan.pnm
```

Scan a full A4 page in 24-bit color:

```
scanimage --mode "24bit Color" --resolution 200 -x 210 -y 297 --format=pnm > color.pnm
```

Available scan modes: `Black & White`, `Gray[Error Diffusion]`, `True Gray`, `24bit Color`

## Tests

```
cd build
gcc -o test_integration ../tests/test_integration.c -ldl -lm
./test_integration ./libbrscandec.so.1 ./libbrcolm.so.1 ../libbrcolm/GrayCmData/YL4FB/brlutcm.dat
# === Results: 65 passed, 0 failed ===
```

Tests cover: brscan4 frame structure, packbits decompression (including ARM `signed char` edge cases), ScanDecOpen parameter computation, full decode pipeline, color matching, and end-to-end JPEG color decode (`tests/test_color_jpeg.c`).

## Debug Logging

Set `BROTHER_DEBUG=1` to enable verbose logging to stderr:

```
BROTHER_DEBUG=1 scanimage --mode "True Gray" --resolution 200 -x 210 -y 297 > scan.pnm
```

## Architecture

```
libsane-brother.so.1    SANE backend (talks USB, parses scan protocol)
  ├── libbrscandec.so.1  Scan decoder: packbits decompression, resolution scaling
  ├── libbrcolm.so.1     Color matching: 3D LUT interpolation from .dat/.cm files
  └── libjpeg            24-bit color decode (JPEG-over-USB for brscan4)
```

## License

GPL v2 (original Brother license preserved).
