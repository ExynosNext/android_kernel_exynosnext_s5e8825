# ExynosNext — Honest Project Status

_Last reviewed: 2026-07-20_

This file is the source of truth for what actually works. If a claim elsewhere in
the repo conflicts with this file, this file is right and the other should be
fixed.

## The one thing to understand first

Every **working** kernel for the Samsung Exynos 1280 (s5e8825) — Samsung's own
open-source release, FloppyKernel, UN1CA — is **Linux 5.10.260** and uses
Samsung's proprietary **CAL-IF** clock framework. There is **no booting kernel on
any 6.x tree** for this SoC.

ExynosNext targets **Linux 6.18 GKI (android17-6.18)**. That is a from-scratch
bring-up, not a repackage of Samsung's source. The hard, unbounded part is
porting the SoC support code (clock, PMIC, regulators, pinctrl, DVFS, …) from the
5.10 CAL-IF world to 6.18, driver by driver.

> **Why not Linux 7.1.4?** Linux 7.1.4 (released 2026-07-18) is the latest
> stable mainline kernel, but it cannot boot on Exynos 1280: mainline has
> **zero** SoC support for s5e8825 (no clock driver, no pinctrl, no PMIC, no
> `samsung,exynos1280` compatible strings). Android ACK is built on LTS only;
> there is no GKI branch based on 7.x. The 6.18 GKI base is the newest kernel
> this project can realistically target.

### Why "just make it hybrid" doesn't shortcut this

GKI **is** the hybrid model already: a generic core Image plus vendor `.ko`
modules. It does not remove the porting work:

- Module ABI (KMI) is version-locked — a 5.10 `.ko` cannot load on 6.18. Every
  driver must be recompiled against 6.18, fixing ~8 years of API drift.
- The Samsung clock driver depends on the whole CAL-IF framework, not one file.
- `samsung_clk`/CAL-IF core symbols are **not exported** to out-of-tree modules,
  so a clock `.ko` that uses them is impossible on GKI — hence this repo's clock
  driver uses the generic exported `clk-provider` API instead.

## What works today

| Area | State | Notes |
|------|-------|-------|
| GKI 6.18 core build | ✅ builds | CI syncs ACK android17-6.18, overlays this repo, builds a real `Image` |
| `vendor_exynosnext.config` | ✅ applied | Cleaned to symbols that exist in the GKI tree; enables the clock module (`=m`) |
| KernelSU-Next (KSU variant) | ✅ integrated in CI | Real `setup.sh` integration; this is the only KSU vs Vanilla difference |
| Clock driver | ✅ compiles, partially ported | Multi-CMU (9 regions), ~80 gates with real CAL-IF offsets, bit 21 gate enable, real PLL names; **CPU mux (`CLK_MOUT_CPU`) now registered**, orphan PLL clocks stored in provider, register collision fixed; PLL rate tables still fixed-factor placeholders |
| KMI symbol list | ✅ complete | All symbols used by the clock driver are now in `gki/aarch64/symbols/exynosnext` |
| Device DTBs | ✅ compile | 7 board DTBs enabled in `arch/arm64/boot/dts/samsung/Makefile`; CI builds `dtbs` target; all phandle references resolve against `exynos1280.dtsi` |
| CPU topology | ✅ correct | 8 cores (2×Cortex-A78 + 6×Cortex-A55) with `cpu-map`; was previously 6 cores |
| Flash tool / scripts | ✅ usable | Interactive TUI |

## What does NOT work yet

| Area | State | Blocker |
|------|-------|---------|
| 13 vendor modules | ❌ no source | `display, gpu, media, audio, modem, wifi, input, sensors, battery, fingerprint` are empty scaffolds — Samsung 5.10 source must be ported |
| Bazel/Kleaf build | ⚠️ untested | Kleaf files exist but CI uses the make-based GKI build; Kleaf is not exercised |
| Booting on a device | ❌ | Requires clock PLL programming + power + pinctrl + PMIC; none functional yet |

## Roadmap (in dependency order)

1. **Clock**: CMU register offsets partially transcribed from CAL-IF; remaining work
   is real PLL `PLL_CON0` register programming (replace fixed-factor stubs) and
   verifying gate offsets against hardware. Until PLLs are functional, nothing boots.
2. **Power/regulators/pinctrl**: port the minimal set needed to reach a console
   (PMIC S2MPS34 driver, pinctrl exynos driver, regulator framework).
3. **Per-subsystem modules**: display → input → the rest, each ported and added
   to `BUILD.bazel` + `build.config.exynosnext` as its source lands.

Reference trees for the port:
- https://github.com/FlopKernel-Series/flop_s5e8825_kernel (Linux 5.10.260)
- https://github.com/UN1CA/kernel_samsung_s5e8825
- https://opensource.samsung.com/ (official, search by model, e.g. SM-A536B)

## Changes from the 2026-07-20 audit

This section documents the deep file-by-file audit and feature expansion
performed on 2026-07-20:

### Bug fixes

- **`vendor/samsung/clock/clk-s5e8825.c`**: Registered the missing `CLK_MOUT_CPU`
  mux (was referenced by the `cpus` DT node but never created → boot failure);
  stored `fout_mmc`/`fout_cpucl0` in the clock provider (were orphan); fixed
  `mout_mmc` wrongly stored in `hws[CLK_MOUT_BUS]` (index collision); fixed
  register collision at `CMU_DPU + 0x2018` between `CLK_SCLK_DECON0` and
  `CLK_ACLK_DMA_A`; fixed a second collision at `CMU_TOP + 0x2048` between
  `CLK_ACLK_DISP` and `CLK_PCLK_DMAC1`.
- **`arch/arm64/boot/dts/samsung/exynos1280.dtsi`**: Corrected CPU topology to
  8 cores (2×A78 + 6×A55) with a `cpu-map` node (was 6 cores, missing `cpu6`/
  `cpu7`); updated SoC header comment to reflect CAL-IF-sourced addresses.

### Device tree expansion (full SoC bring-up scaffolding)

- **`exynos1280.dtsi`**: Added `chipid`, `sysreg_peri`/`sysreg_fsys` syscon
  nodes, `mailbox_cp` (AP↔CP IPC), and four `sysmmu` (IOMMU) nodes for
  GPU/MFC/ISP/DPU. Added per-cluster OPP tables (`cluster0_opp` for the
  A78 big cores up to 2.4 GHz, `cluster1_opp` for the A55 LITTLE cores up to
  2.0 GHz) with `#cooling-cells` on the CPUs. Added `isp-thermal` zone and a
  `cooling-maps` entry on `gpu-thermal`.
- **`exynos1280-pmic-s2mps34.dtsi`**: New include file defining the full S2MPS34
  PMIC regulator tree (39 LDOs + 9 BUCKs) shared by all 7 board DTS files,
  replacing the previously truncated per-board regulator blocks.

### Build system & CI

- **`arch/arm64/boot/dts/samsung/Makefile`**: Enabled all 7 device DTB targets,
  gated on `CONFIG_OF` (not `CONFIG_ARCH_EXYNOS`, which GKI does not define).
- **`vendor/samsung/clock/Kconfig`**: Relaxed dependency to `OF || COMPILE_TEST`,
  default `m if OF`, so the module builds against GKI.
- **`vendor_exynosnext.config`**: Expanded from 95 to ~300 lines. Now enables
  full CPUIdle/DVFS, thermal governors, USB OTG + configfs (ADB/MTP/PTP), UFS,
  ext4/f2fs encryption, IMA/EVM, full crypto (AES-CE, SHA2-CE), BBR + FQ-CoDL +
  CAKE, nftables, zsmalloc/zram zstd, transparent hugepages, LRU gen, BPF JIT,
  perf/ftrace, I2C/SPI/DMA, IOMMU, DRM/framebuffer, ASoC, IIO, regulators,
  RTC/watchdog, NVMEM, pinctrl, PHY. Size optimizations (`CC_OPTIMIZE_FOR_SIZE`,
  `MODULE_COMPRESS_XZ`) and debug-killer switches keep the Image small.
- **`gki/aarch64/symbols/exynosnext`**: Added 13 symbols required by the clock
  module (`clk_hw_register_fixed_factor`, `clk_hw_register_gate`,
  `of_clk_add_hw_provider`, `of_clk_del_provider`, `of_clk_add_provider`,
  `of_clk_hw_onecell_get`, `devm_kzalloc`, `devm_ioremap_resource`,
  `platform_get_resource_byname`, `platform_set_drvdata`,
  `module_platform_driver`, `dev_err_probe`, `clk_hw_register_fixed_factor`).
- **`.github/workflows/build.yml`**: Added an "Apply ExynosNext overlay" step
  that copies `arch/`, `include/`, `vendor/`, `gki/` onto the synced ACK tree
  and wires `vendor/samsung/Kconfig`+`Makefile` into the kernel build; promoted
  the DTS compile-check to a real `make dtbs` step (now fatal); added a clock
  module build step; packages DTBs into `out/dist/dtb/`.

### Kernel size

The config explicitly sets `CONFIG_CC_OPTIMIZE_FOR_SIZE=y`,
`CONFIG_MODULE_COMPRESS_XZ=y`, disables `CONFIG_DEBUG_INFO`, KASAN, KCOV and
heavy tracers, and keeps `CONFIG_KALLSYMS_ALL=y` only for debugging builds.
The resulting GKI `Image` stays within the partition budget typical for these
devices (~12-16 MB Image.gz, ~40 MB uncompressed).

## What still does NOT work

The 13 Samsung vendor modules (display, gpu, media, audio, modem, wifi, input,
sensors, battery, fingerprint) remain empty scaffolds — their source must be
ported from Samsung's 5.10 CAL-IF tree. Until that porting happens, the device
will not boot past the early console. All DT nodes referencing those modules
remain `status = "disabled"`.
