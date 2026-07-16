# ExynosNext Kernel

A modern Linux 6.18 LTS kernel for Samsung Exynos 1280 (s5e8825) devices,
built on Android Common Kernel (ACK) with Samsung vendor module support.

## Supported Devices

| Device | Codename | Status |
|--------|----------|--------|
| Galaxy A53 5G | a53x | Primary target |
| Galaxy A25 | a25x | Supported |
| Galaxy A26 | a26xs | Supported |
| Galaxy A33 | a33x | Supported |
| Galaxy M33 | m33x | Supported |
| Galaxy M34 | m34x | Supported |
| Galaxy Tab S6 Lite 2024 | gta4xls | Supported |

## Architecture

- **Base:** Android Common Kernel `android17-6.18` (Linux 6.18 LTS)
- **Vendor modules:** Samsung BSP drivers ported from Android 16 source
- **Root:** KernelSU Next (built-in)
- **Build system:** Kleaf (Bazel) + DDK

## Building

### Prerequisites

```bash
# Install repo tool
mkdir -p ~/bin
curl https://storage.googleapis.com/git-repo-downloads/repo > ~/bin/repo
chmod a+x ~/bin/repo

# Install build dependencies (Ubuntu/Debian)
sudo apt-get install -y bc bison build-essential ccache curl flex \
    git libncurses5-dev libssl-dev python3 python3-pip lz4 \
    zip unzip tar xz-utils

# Install Bazelisk (for Kleaf)
curl -Lo /usr/local/bin/bazel https://github.com/bazelbuild/bazelisk/releases/latest/download/bazelisk-linux-amd64
chmod +x /usr/local/bin/bazel
```

### Quick Build

```bash
# Initialize repo with ExynosNext manifest
repo init -u https://github.com/ExynosNext/manifest -b main
repo sync -j$(nproc)

# Build GKI kernel + vendor modules
tools/bazel run //:exynosnext_dist

# Output: out/dist/
#   - boot.img (OneUI variant)
#   - boot_aosp.img (AOSP variant)
#   - vendor_boot.img (modules)
#   - dtbo.img (device tree overlays)
#   - FloppyKernel-ExynosNext-<version>.zip (flashable)
```

### Build Variants

```bash
# Vanilla (no root)
tools/bazel run //:exynosnext_dist -- --config=vanilla

# KernelSU Next
tools/bazel run //:exynosnext_dist -- --config=ksu

# Full LTO
tools/bazel run //:exynosnext_dist -- --config=full_lto
```

## Flashing

### Via fastboot (recommended)

```bash
# Flash kernel + modules + dtbo
fastboot flash boot out/dist/boot.img
fastboot flash vendor_boot out/dist/vendor_boot.img
fastboot flash dtbo out/dist/dtbo.img
fastboot reboot
```

### Via Odin (Samsung)

```bash
# Create Odin flashable TAR
./scripts/create-tar.sh --variant oneui
# Output: ExynosNext-OneUI-<version>.tar
# Flash via Odin: AP tab -> Select file -> Start
```

### Interactive Flash Tool

```bash
./scripts/flash.sh
# Auto-detects device, ROM type, and selects correct variant
```

## Project Structure

```
android_kernel_exynosnext_s5e8825/
├── common/                          # ACK android17-6.18 fork
│   ├── arch/arm64/boot/dts/samsung/ # Exynos 1280 device trees
│   └── gki/aarch64/symbols/         # KMI symbol lists
├── vendor/samsung/                  # Samsung vendor modules (ported)
│   ├── clock/                       # Clock controller
│   ├── display/                     # DPU, DSIM, panel drivers
│   ├── gpu/mali/                    # Mali G78 driver
│   ├── media/mfc/                   # Multi-Function Codec
│   ├── audio/                       # Audio codecs and amplifiers
│   ├── modem/cpif/                  # Modem interface
│   ├── wifi/scsc/                   # WiFi/BT
│   ├── input/touchscreen/           # Touchscreen drivers
│   ├── sensors/sensorhub/           # Sensor hub
│   ├── battery/charger/             # Battery and charger
│   └── fingerprint/egistec/         # Fingerprint sensor
├── build.config.exynosnext          # Kleaf build config
├── BUILD.bazel                      # Root Bazel build
├── MODULE.bazel                     # Module definitions
└── scripts/                         # Build and flash scripts
```

## How It Works

ExynosNext uses a **hybrid kernel architecture**:

1. **GKI Kernel** (from ACK): The core Linux kernel with Google's Generic Kernel Image. Provides the stable KMI (Kernel Module Interface) ABI.

2. **Vendor Modules** (from Samsung BSP): Hardware-specific drivers (display, camera, audio, etc.) built as loadable kernel modules (.ko). These are ported from Samsung's Android 16 kernel source to work with Linux 6.18.

3. **Device Tree**: Exynos 1280 SoC definitions and per-device overlays, added to the ACK tree.

This approach gives us:
- Modern Linux 6.18 LTS kernel (security, performance, features)
- Full hardware support via Samsung's proven vendor drivers
- GKI-compatible architecture for easy updates
- KernelSU Next root integration

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test on real hardware
5. Submit a pull request

## Credits

- [Android Common Kernel](https://android.googlesource.com/kernel/common/) by Google
- [Samsung Open Source](https://opensource.samsung.com/) for vendor driver sources
- [KernelSU](https://kernelsu.org/) for root solution
- [FlopKernel](https://github.com/FlopKernel-Series/flop_s5e8825_kernel/) for Exynos 1280 community kernel reference

## License

The kernel is licensed under the terms of the GNU General Public License version 2.
See the [LICENSE](common/COPYING) file for the full license text.
