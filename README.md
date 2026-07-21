# ExynosNext Kernel

<div align="center">

**A from-scratch Linux 6.18 GKI bring-up for the Samsung Exynos 1280 (s5e8825) SoC family.**

Built on Android Common Kernel (ACK) `android17-6.18` with Samsung vendor
module support and KernelSU-Next root integration.

[![CI](https://github.com/ExynosNext/android_kernel_exynosnext_s5e8825/actions/workflows/build.yml/badge.svg)](https://github.com/ExynosNext/android_kernel_exynosnext_s5e8825/actions/workflows/build.yml)
[![License: GPL-2.0](https://img.shields.io/badge/License-GPL--2.0-blue.svg)](LICENSE)

</div>

---

## ⚠️ Project Status — Read Before Expecting a Bootable Kernel

This is an **early bring-up project**. The GKI core kernel builds successfully and
the device trees compile, but **no kernel produced by this project can boot on a
device yet**. The 13 Samsung vendor modules (display, GPU, audio, modem, WiFi,
input, sensors, battery, fingerprint) are empty scaffolds awaiting a port from
Samsung's Linux 5.10 CAL-IF source tree. The clock controller driver compiles but
cannot program real clocks because Samsung drives the clock tree through the
**ACPM IPC** co-processor, not direct register writes.

For the complete, honest state of every component, see
**[docs/STATUS.md](docs/STATUS.md)**.

### Why not Linux 7.1.4 (or any 7.x mainline)?

Linux 7.1.4 (released 2026-07-18) is the latest stable mainline kernel, but it
**cannot boot on Exynos 1280**:

- Mainline Linux has **zero** SoC support for s5e8825 — no clock driver, no
  pinctrl, no PMIC, no `samsung,exynos1280` compatible strings.
- Android ACK is built on **LTS kernels only**; there is no GKI branch based
  on 7.x.
- Writing full SoC support from scratch and submitting it upstream is a
  multi-year effort, not a session task.

The **6.18 LTS** base (via ACK `android17-6.18`) is the newest kernel this
project can realistically target.

---

## Target Devices

All devices below share the Exynos 1280 (s5e8825) SoC. The A53 is the primary
bring-up target; the others reuse the common `exynos1280.dtsi`.

| Device | Codename | DTS file | RAM | Bring-up |
|--------|----------|----------|-----|----------|
| Galaxy A53 5G | `a53x` | `exynos1280-a53x.dts` | 6 GB | Primary |
| Galaxy A25 | `a25x` | `exynos1280-a25x.dts` | 6 GB | Planned |
| Galaxy A26 | `a26x` | `exynos1280-a26x.dts` | 6 GB | Planned |
| Galaxy A33 | `a33x` | `exynos1280-a33x.dts` | 6 GB | Planned |
| Galaxy M33 | `m33x` | `exynos1280-m33x.dts` | 6 GB | Planned |
| Galaxy M34 | `m34x` | `exynos1280-m34x.dts` | 6 GB | Planned |
| Galaxy Tab S6 Lite (2024) | `gta4xls` | `exynos1280-gta4xls.dts` | 4 GB | Planned |

---

## Build Variants

The CI pipeline produces **five variants** via a build matrix, each as a
professional AnyKernel3 flashable zip:

| Variant | Root | OneUI | Description |
|---------|------|-------|-------------|
| `aosp-noroot` | ❌ | ❌ | Clean GKI kernel for AOSP ROMs, no root |
| `aosp-ksu` | ✅ | ❌ | GKI kernel + KernelSU-Next root for AOSP ROMs |
| `oneui-noroot` | ❌ | ✅ | GKI kernel + OneUI config fragment for Samsung OneUI ROMs |
| `oneui-ksu` | ✅ | ✅ | GKI kernel + KernelSU-Next + OneUI config fragment |
| `resuki` | ✅ | ❌ | ReSuki (embedded SU, no KSU app needed) |

### AnyKernel3 packaging

Each variant is packaged as an **AnyKernel3 flashable zip** — the professional
approach used by FloppyKernel, KernelSU official builds, and every serious
Android kernel project. The zip contains:

- `Image` — the built kernel
- `dtb` — device tree blob
- `anykernel.sh` — AK3 install script with automatic AOSP/OneUI detection
- `tools/` — magiskboot, busybox, ak3-core.sh, and other AK3 tools
- `META-INF/` — recovery install scripts

At flash time, AnyKernel3 extracts the device's current boot.img, replaces
only the kernel (Image) and dtb, preserves the device's ramdisk/init, and
repacks. This is the **only correct way** to flash a GKI kernel — the device's
ramdisk is essential and cannot be replaced with a stub.

### Multi-Android-version support

The CI framework supports building against multiple ACK branches:

| Android | ACK branch | Linux |
|---------|-----------|-------|
| 17 | `common-android17-6.18` | 6.18 (default) |
| 16 | `common-android16-6.12` | 6.12 |
| 15 | `common-android15-6.6` | 6.6 |
| 14 | `common-android14-6.1` | 6.1 |

---

## What the CI Builds

The GitHub Actions pipeline (`.github/workflows/build.yml`) performs:

1. **Syncs** Google's ACK `android17-6.18` GKI tree into `common/` via `repo`.
2. **Overlays** this repo's `arch/`, `include/`, `vendor/`, `gki/` files on top
   of the synced ACK tree, and wires `vendor/samsung/Kconfig` + `Makefile` into
   the kernel build. The Samsung DTS subdirectory is registered in
   `arch/arm64/boot/dts/Makefile`.
3. **Integrates KernelSU-Next** (for `ksu*` variants only).
4. **Configures** the kernel: `gki_defconfig` + `vendor_exynosnext.config` (+
   `oneui.config` for `*-oneui` variants) + KSU enable for `ksu*`.
5. **Builds**:
   - The GKI `Image` and `Image.gz`.
   - The 7 Exynos 1280 device DTBs (`exynos1280-*.dtb`).
   - The clock controller module (`clk_s5e8825.ko`).
   - A flashable `boot.img` (via in-tree `mkbootimg` with a stub ramdisk).
   - A `dtbo.img` if `.dtbo` overlays were built.
6. **Packages** everything into a flat `out/dist/` directory (no zip-in-zip):
   - `Image`, `Image.gz`, `boot.img`, `dtbo.img`
   - `dtb/*.dtb` (device tree blobs)
   - `modules/*.ko` (vendor kernel modules)
   - `kernel.config` (the `.config` used)
   - `README.txt` (flashing instructions)

---

## Building Locally

### Prerequisites

```bash
# Install the repo tool
mkdir -p ~/bin
curl https://storage.googleapis.com/git-repo-downloads/repo > ~/bin/repo
chmod a+x ~/bin/repo
export PATH=~/bin:$PATH

# Install build dependencies (Ubuntu/Debian)
sudo apt-get install -y bc bison build-essential ccache curl flex \
    git libncurses5-dev libssl-dev python3 python3-pip lz4 \
    zip unzip tar xz-utils cpio

# A recent LLVM/Clang toolchain (16+) is required for GKI builds.
# The CI installs it via the 'Install toolchain' step; locally you can use:
sudo apt-get install -y llvm clang lld
```

### Manual Build (mirrors the CI)

```bash
# 1. Sync the ACK GKI tree
repo init -u https://android.googlesource.com/kernel/manifest \
  -b common-android17-6.18 --depth=1
repo sync -j$(nproc)

# 2. Apply the ExynosNext overlay
OVERLAY=/path/to/this/repo
cp -rv "$OVERLAY/arch/."    common/arch/
cp -rv "$OVERLAY/include/." common/include/
cp -rv "$OVERLAY/vendor/."  common/vendor/
cp -rv "$OVERLAY/gki/."     common/gki/
printf '\nsource "vendor/samsung/Kconfig"\n' >> common/Kconfig
printf '\nobj-y += vendor/samsung/\n'         >> common/Makefile
printf '\nsubdir-y += samsung\n'              >> common/arch/arm64/boot/dts/Makefile

cd common

# 3. Configure
make O=out ARCH=arm64 LLVM=1 LLVM_IAS=1 gki_defconfig
scripts/kconfig/merge_config.sh -m -O out out/.config \
  "$OVERLAY/vendor_exynosnext.config"
make O=out ARCH=arm64 LLVM=1 LLVM_IAS=1 olddefconfig

# 4. Build the kernel
make O=out ARCH=arm64 LLVM=1 LLVM_IAS=1 -j$(nproc) Image Image.gz

# 5. Build device DTBs
make O=out ARCH=arm64 LLVM=1 LLVM_IAS=1 -j$(nproc) dtbs

# 6. Build the clock module
make O=out ARCH=arm64 LLVM=1 LLVM_IAS=1 -j$(nproc) \
  M=vendor/samsung/clock modules
```

### KernelSU-Next Integration (for KSU variants)

```bash
cd common
git clone --depth=1 --branch dev \
  https://github.com/KernelSU-Next/KernelSU-Next.git
# Replace the symlink with a real copy (symlinks break #include resolution)
if [ -L KernelSU-Next/kernel/include/uapi ]; then
  rm KernelSU-Next/kernel/include/uapi
  cp -r KernelSU-Next/uapi KernelSU-Next/kernel/include/uapi
fi
cp -r KernelSU-Next/kernel drivers/kernelsu
printf '\nobj-$(CONFIG_KSU) += kernelsu/\n' >> drivers/Makefile
sed -i '/endmenu/i\source "drivers/kernelsu/Kconfig"' drivers/Kconfig
./scripts/config --file out/.config -e KSU
make O=out ARCH=arm64 LLVM=1 LLVM_IAS=1 olddefconfig
```

### Using the Helper Scripts

```bash
# Full local build (requires Docker)
./scripts/docker-build.sh

# Or run the build steps directly
./scripts/build.sh

# Extract built modules from a build tree
./scripts/extract-modules.sh

# Flash via interactive TUI
./scripts/flash.sh

# Download Samsung's 5.10 source for porting reference
./scripts/get-samsung-source.sh
```

---

## Flashing

> ⚠️ The kernel produced by this project **does not boot**. Flashing it will
> leave the device in a bootloop. The instructions below describe the intended
> flashing flow once bring-up is complete.

### Via fastboot

```bash
# Boot to bootloader
adb reboot bootloader

# Flash kernel + dtbo + modules
fastboot flash boot out/dist/boot.img
fastboot flash dtbo out/dist/dtbo.img    # if present
fastboot reboot
```

### Via the interactive flash tool

```bash
./scripts/flash.sh
# Auto-detects device, ROM type, and selects the correct variant.
```

---

## Project Structure

```
android_kernel_exynosnext_s5e8825/
├── .github/workflows/build.yml      # CI: syncs ACK, overlays, builds 4 variants
├── arch/arm64/boot/dts/samsung/
│   ├── exynos1280.dtsi              # SoC base (transcribed from real Samsung DT)
│   ├── exynos1280-pmic-s2mps34.dtsi # S2MPS34 PMIC regulator tree (39 LDO + 9 BUCK)
│   ├── exynos1280-a53x.dts          # Galaxy A53 5G board DTS
│   ├── exynos1280-a25x.dts          # Galaxy A25 board DTS
│   ├── exynos1280-a26x.dts          # Galaxy A26 board DTS
│   ├── exynos1280-a33x.dts          # Galaxy A33 board DTS
│   ├── exynos1280-m33x.dts          # Galaxy M33 board DTS
│   ├── exynos1280-m34x.dts          # Galaxy M34 board DTS
│   ├── exynos1280-gta4xls.dts       # Galaxy Tab S6 Lite board DTS
│   └── Makefile                     # DTB build targets (gated on CONFIG_OF)
├── include/dt-bindings/clock/
│   └── samsung,exynos1280-clk.h     # Clock ID enum header
├── vendor/samsung/
│   ├── clock/                       # Clock controller (clk-s5e8825.c — real source)
│   │   ├── clk-s5e8825.c            # Multi-CMU driver (generic clk-provider API)
│   │   ├── clk-s5e8825.h            # Clock ID definitions
│   │   ├── Kconfig                  # CONFIG_COMMON_CLK_S5E8825
│   │   ├── Makefile                 # clk-s5e8825.o
│   │   └── BUILD.bazel              # Kleaf module definition
│   ├── display/                     # Scaffolding (empty — awaiting port)
│   ├── gpu/                         # Scaffolding (empty)
│   ├── audio/                       # Scaffolding (empty)
│   ├── modem/                       # Scaffolding (empty)
│   ├── wifi/                        # Scaffolding (empty)
│   ├── input/                       # Scaffolding (empty)
│   ├── sensors/                     # Scaffolding (empty)
│   ├── battery/                     # Scaffolding (empty)
│   ├── fingerprint/                 # Scaffolding (empty)
│   ├── media/                       # Scaffolding (empty)
│   ├── Kconfig                      # Top-level vendor Kconfig
│   ├── Makefile                     # Top-level vendor Makefile
│   └── BUILD.bazel                  # Top-level vendor Bazel
├── gki/aarch64/symbols/
│   └── exynosnext                   # KMI symbol list (193 symbols)
├── vendor_exynosnext.config         # Kernel config fragment (300+ options)
├── oneui.config                     # OneUI config fragment (Samsung vendor hooks)
├── build.config.exynosnext          # Kleaf build config (module list)
├── BUILD.bazel                      # Root Bazel build file
├── MODULE.bazel                     # Bazel module definition
├── Makefile                         # Convenience Makefile
├── scripts/
│   ├── build.sh                     # Local build helper
│   ├── docker-build.sh              # Docker-based build
│   ├── extract-modules.sh           # Extract built .ko modules
│   ├── flash.sh                     # Interactive flash TUI
│   ├── get-samsung-source.sh        # Download Samsung 5.10 source
│   └── setup.sh                     # KernelSU-Next setup helper
└── docs/
    ├── STATUS.md                    # Honest project status (source of truth)
    ├── PORTING.md                   # Porting roadmap and checklist
    └── reference-a53-real.dts       # Decoded Samsung firmware DT (137 nodes)
```

---

## How It Works — Hybrid GKI Architecture

ExynosNext uses the standard **GKI (Generic Kernel Image)** architecture:

1. **GKI Core Kernel** (from ACK `android17-6.18`): a generic Linux 6.18 kernel
   with a stable KMI (Kernel Module Interface) ABI. This is the `Image` that
   boots the device.

2. **Vendor Modules** (from Samsung BSP): hardware-specific drivers (display,
   camera, audio, etc.) built as loadable kernel modules (`.ko`). These must be
   ported from Samsung's Linux 5.10 CAL-IF source to compile against 6.18 —
   approximately 8 years of kernel API drift must be resolved per driver.

3. **Device Tree**: Exynos 1280 SoC definitions (`exynos1280.dtsi`) and
   per-device board files (`exynos1280-*.dts`), added to the ACK tree.

4. **KernelSU-Next** (optional): root solution integrated as an in-tree driver
   for the `ksu` and `ksu-oneui` variants.

This approach gives:
- A modern Linux 6.18 LTS kernel (security, performance, features)
- Hardware support via Samsung's proven vendor drivers (once ported)
- GKI-compatible architecture for OTA-friendly updates
- Optional root via KernelSU-Next

---

## The Real Device Tree Audit

The `exynos1280.dtsi` was **completely rewritten** using the real device tree
extracted from Samsung's official firmware (`A536BXXSMGZE1`, SM-A536B / Galaxy
A53 5G). The full decoded dump (137 addressed nodes) is committed as
`docs/reference-a53-real.dts` for verification.

### Key architectural facts discovered

| Component | Real (from Samsung firmware) | Previous (wrong) |
|-----------|------------------------------|-------------------|
| Interrupt controller | `arm,gic-400` (GICv2) at `0x12b00000` | `arm,gic-v3` at `0x10000000` |
| Clock controller | `samsung,s5e8825-clock` at `0x11800000` (ACPM IPC) | `0x12900000` (direct regs) |
| Mali GPU | `arm,mali` at `0x10300000` | `0x14000000` |
| UFS storage | `samsung,exynos-ufs` at `0x13500000` | `0x13d00000` |
| Display (DECON) | `samsung,exynos-decon` at `0x14940000` | `0x15100000` |
| UART0 | `samsung,exynos-uart` at `0x13800000` | `0x13100000` |
| MCT timer | `samsung,exynos4210-mct` at `0x10050000` | _missing_ |

### Critical discoveries

1. **GICv2, not GICv3**: the SoC uses `arm,gic-400` with the 3-cell GICv2
   interrupt specifier. The previous dtsi assumed GICv3 — this alone would have
   made the kernel unable to receive any interrupt.

2. **Clock tree driven through ACPM IPC**: Samsung does not program clock
   registers directly from Linux; it sends IPC messages to the Audio
   Co-Processor Manager (ACPM). This is the **primary blocker** for booting —
   the ACPM mailbox driver must be ported first.

3. **CPU topology**: 6× `arm,ananke` (Cortex-A55, LITTLE, cluster0) + 2×
   `arm,hercules` (Cortex-A78, big, cluster1). Samsung uses codenames, not the
   standard `arm,cortex-a55`/`arm,cortex-a78` strings.

---

## Configuration

### `vendor_exynosnext.config`

A 300+ option kernel config fragment applied on top of `gki_defconfig`. It
enables:

- **Power management**: CPUIdle, CPUFreq (schedutil governor), DVFS, suspend
- **Thermal**: thermal zones, cooling maps, devfreq thermal
- **USB**: OTG, DWC3, configfs (ADB, MTP, PTP, mass storage)
- **Storage**: UFS, eMMC (DWMMC), dm-verity, dm-crypt
- **Filesystems**: ext4 (encryption, ACL), f2fs (encryption, compression),
  overlay, fuse, pstore
- **Security**: MTE, BTI, PAC, stack protector, fortify, CFI, IMA/EVM, Yama,
  Landlock, lockdown, loadpin
- **Crypto**: AES-CE, SHA2-CE, GHASH-CE, XTS, GCM, ZSTD compression
- **Networking**: BBR, FQ-CoDL, CAKE, nftables, conntrack
- **Memory**: zram (zstd), zsmalloc, THP, LRU gen, KSM, memcg
- **BPF**: BPF JIT, ftrace, perf events
- **Hardware**: I2C (S3C), SPI (S3C64XX), DMA (PL330), IOMMU (SMMU, Exynos),
  DRM, ASoC, IIO, regulators, RTC, watchdog, NVMEM, pinctrl, PHY
- **Size optimizations**: `CC_OPTIMIZE_FOR_SIZE`, `MODULE_COMPRESS_XZ`,
  `DEBUG_INFO`/`KASAN`/`KCOV` explicitly disabled

### `oneui.config`

Applied on top of `vendor_exynosnext.config` for `*-oneui` variants. Enables
Samsung vendor hooks (`ANDROID_VENDOR_HOOKS`, `SAMSUNG_VH_*`), SELinux develop
mode, binderfs, ashmem, devfreq governors, LED/vibrator, Samsung sysfs stubs,
and USB function drivers (MTP/PTP/ADB/RNDIS).

### KMI symbol list (`gki/aarch64/symbols/exynosnext`)

193 exported kernel symbols required by the vendor modules. Updated to include
all symbols used by the clock driver (`clk_hw_register_fixed_factor`,
`of_clk_add_hw_provider`, `devm_ioremap_resource`, `platform_get_resource_byname`,
`module_platform_driver`, `dev_err_probe`, etc.).

---

## Contributing

See **[CONTRIBUTING.md](CONTRIBUTING.md)** for guidelines. The key rule:
**be honest about what works and what doesn't.** If you add a driver, verify it
compiles and (if possible) note whether it has been tested on hardware. Update
`docs/STATUS.md` to reflect the actual state.

### Porting roadmap (dependency order)

1. **ACPM mailbox driver** — the foundation. Without it, no clock/DVFS/PMIC can
   be programmed. Port `drivers/soc/samsung/acpm` from 5.10.
2. **Clock driver rewrite** — replace direct-register stubs with ACPM-based IPC.
3. **PMIC (S2MPS34) + pinctrl** — port from 5.10; they use ACPM for some ops.
4. **Reach a console** — with ACPM + clock + pinctrl + UART, the kernel can
   reach early boot console (first measurable milestone).
5. **Per-subsystem modules** — display → input → the rest.

See **[docs/PORTING.md](docs/PORTING.md)** for the detailed per-subsystem
checklist.

---

## Credits

- **[Android Common Kernel](https://android.googlesource.com/kernel/common/)** by
  Google — the GKI base (`android17-6.18`).
- **[Samsung Open Source](https://opensource.samsung.com/)** — vendor driver
  sources (search by model, e.g. SM-A536B).
- **[KernelSU-Next](https://github.com/KernelSU-Next/KernelSU-Next)** — root
  solution for the `ksu` variants.
- **[FlopKernel](https://github.com/FlopKernel-Series/flop_s5e8825_kernel/)** —
  Exynos 1280 community kernel reference (Linux 5.10.260 + CAL-IF).
- **[UN1CA](https://github.com/UN1CA/kernel_samsung_s5e8825)** — another
  community reference tree.

---

## License

The kernel is licensed under the terms of the **GNU General Public License
version 2**. See the [LICENSE](LICENSE) file for the full license text. Vendor
modules ported from Samsung's source retain their original Samsung licensing.
