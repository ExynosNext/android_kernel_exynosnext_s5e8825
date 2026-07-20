# CLAUDE.md — ExynosNext Kernel Project Context

> AI-assisted development context file for the ExynosNext Kernel project.
> Provides complete project knowledge so AI assistants can understand the
> codebase without re-reading everything. **This file must be kept in sync
> with reality.** If you change the build system, variants, or architecture,
> update this file.

---

## Project Overview

- **Name**: ExynosNext Kernel
- **Base**: Linux 6.18 LTS (ACK `android17-6.18` / GKI)
- **Target SoC**: Samsung Exynos 1280 (s5e8825)
- **Architecture**: Hybrid (ACK GKI kernel + Samsung vendor modules)
- **Target Devices**: Galaxy A53, A25, A26, A33, M33, M34, Tab S6 Lite 2024
  (all Exynos 1280 variants)

## Build System

| Item | Details |
|------|---------|
| **Primary build tool** | make-based GKI build (CI uses this) |
| **CI command** | `.github/workflows/build.yml` (GitHub Actions) |
| **Kleaf (Bazel)** | Files present but **not exercised** by CI |
| **Build config** | `build.config.exynosnext` |
| **Defconfig fragment** | `vendor_exynosnext.config` (300+ options) |
| **OneUI fragment** | `oneui.config` (applied for `*-oneui` variants) |
| **Variants** | `vanilla`, `ksu`, `vanilla-oneui`, `ksu-oneui` |
| **Local build** | `./scripts/build.sh` or `./scripts/docker-build.sh` |

## Build Variants

| Variant | KSU | OneUI | Description |
|---------|-----|-------|-------------|
| `vanilla` | ❌ | ❌ | Clean GKI kernel |
| `ksu` | ✅ | ❌ | + KernelSU-Next root |
| `vanilla-oneui` | ❌ | ✅ | + OneUI config fragment |
| `ksu-oneui` | ✅ | ✅ | + KSU + OneUI |

- KSU is integrated by cloning `KernelSU-Next/KernelSU-Next` (dev branch) and
  copying `kernel/` to `drivers/kernelsu/` as a **real directory** (not a
  symlink — symlinks break `#include "..."` resolution in the O=out build).
- The OneUI fragment is applied after `vendor_exynosnext.config` via
  `merge_config.sh`.

## Key Files

### Build & Configuration

| File | Purpose |
|------|---------|
| `.github/workflows/build.yml` | CI pipeline (4-variant matrix) |
| `build.config.exynosnext` | Kleaf build config (module list) |
| `vendor_exynosnext.config` | GKI defconfig fragment (300+ options) |
| `oneui.config` | OneUI defconfig fragment (Samsung vendor hooks) |
| `BUILD.bazel` / `MODULE.bazel` / `.bazelrc` | Bazel files (untested) |
| `Makefile` | Convenience wrapper |

### Device Trees

| File | Purpose |
|------|---------|
| `arch/arm64/boot/dts/samsung/exynos1280.dtsi` | SoC base — **transcribed from real Samsung firmware DT** |
| `arch/arm64/boot/dts/samsung/exynos1280-pmic-s2mps34.dtsi` | S2MPS34 PMIC regulator tree (39 LDO + 9 BUCK) |
| `arch/arm64/boot/dts/samsung/exynos1280-*.dts` | 7 per-device board files |
| `arch/arm64/boot/dts/samsung/Makefile` | DTB build targets (gated on `CONFIG_OF`) |

### Clock Controller

| File | Purpose |
|------|---------|
| `vendor/samsung/clock/clk-s5e8825.c` | Clock driver (generic `clk-provider` API) |
| `vendor/samsung/clock/clk-s5e8825.h` | Clock ID definitions |
| `include/dt-bindings/clock/samsung,exynos1280-clk.h` | DTS clock bindings |

### KMI

| File | Purpose |
|------|---------|
| `gki/aarch64/symbols/exynosnext` | KMI symbol list (193 symbols) |

### Reference

| File | Purpose |
|------|---------|
| `docs/reference-a53-real.dts` | Full decoded Samsung firmware DT (137 nodes) |
| `docs/STATUS.md` | Honest project status (source of truth) |
| `docs/PORTING.md` | Porting roadmap and checklist |

### Scripts

| File | Purpose |
|------|---------|
| `scripts/build.sh` | Local build helper |
| `scripts/docker-build.sh` | Docker-based build |
| `scripts/extract-modules.sh` | Extract built `.ko` modules |
| `scripts/flash.sh` | Interactive flash TUI |
| `scripts/get-samsung-source.sh` | Download Samsung 5.10 source |
| `scripts/setup.sh` | KernelSU-Next setup helper |

## Architecture — Real SoC Map (from firmware audit)

The `exynos1280.dtsi` was rewritten using the real device tree extracted from
Samsung's official firmware (`A536BXXSMGZE1`). Key facts:

| Component | Compatible | Address | Notes |
|-----------|------------|---------|-------|
| GIC | `arm,gic-400` | `0x12b00000` | **GICv2** (not v3!) — 3-cell interrupts |
| ChipID | `samsung,s5e8825-chipid` | `0x10000000` | |
| MCT timer | `samsung,exynos4210-mct` | `0x10050000` | sched_clock source |
| Clock controller | `samsung,s5e8825-clock` | `0x11800000` | **ACPM IPC** (`acpm-ipc-channel = <0>`) |
| CAL-IF | `samsung,exynos_cal_if` | `0x12900000` | Register mirror |
| PMU | `samsung,exynos1280-pmu` | `0x11860000` | syscon |
| UART0 | `samsung,exynos-uart` | `0x13800000` | Debug console |
| UFS | `samsung,exynos-ufs` | `0x13500000` | 6 reg regions |
| Mali GPU | `arm,mali` | `0x10300000` | |
| DECON | `samsung,exynos-decon` | `0x14940000` | 5 reg regions |
| DSIM | `samsung,exynos-dsim` | `0x148c0000` | |
| ABOX (audio) | `samsung,abox` | `0x14e50000` | 5 reg regions |
| WiFi/BT | `samsung,scsc_wifibt` | `0x11a70000` | |

### CPU topology

- **cluster0** (LITTLE): 6× `arm,ananke` (Cortex-A55) at cpu@0..0500
- **cluster1** (big): 2× `arm,hercules` (Cortex-A78) at cpu@600,0700
- Samsung uses **codenames**, not `arm,cortex-a55`/`arm,cortex-a78`
- Per-cluster OPP tables with `#cooling-cells`

### Critical discovery: ACPM

Samsung does **not** program clock registers directly from Linux. The clock
tree is driven through **ACPM IPC** (Audio Co-Processor Manager). The
`samsung,s5e8825-clock` node uses `acpm-ipc-channel = <0>`. This means:

- The current `clk-s5e8825.c` driver (which writes registers directly) **cannot
  actually program clocks**.
- The **ACPM mailbox driver** (`drivers/soc/samsung/acpm` in 5.10) must be
  ported first — it is the #1 blocker for booting.

## CI Pipeline

The GitHub Actions workflow (`.github/workflows/build.yml`):

1. Syncs ACK `android17-6.18` into `common/`
2. Applies the ExynosNext overlay (arch/include/vendor/gki)
3. Registers `samsung` in `arch/arm64/boot/dts/Makefile` (ACK has no Exynos)
4. Integrates KernelSU-Next (for `ksu*` variants)
5. Configures: `gki_defconfig` + `vendor_exynosnext.config` (+ `oneui.config`)
6. Builds: `Image`, `Image.gz`, DTBs, `clk_s5e8825.ko`, `boot.img`, `dtbo.img`
7. Packages into `out/dist/` (flat directory, no zip-in-zip)
8. Uploads as artifact `ExynosNext-GKI-<variant>`

GitHub Actions versions: `actions/checkout@v5`, `actions/cache@v5`,
`actions/upload-artifact@v5` (Node 22 — Node 20 was deprecated 2025-09-19).

## Porting Status

| Component | Status | Notes |
|-----------|--------|-------|
| GKI core kernel | ✅ Builds | CI syncs ACK android17-6.18, overlays, builds `Image` |
| `vendor_exynosnext.config` | ✅ Applied | 300+ options: PM, DVFS, USB, UFS, crypto, BPF, etc. |
| KernelSU-Next | ⚠️ Integration WIP | Manual clone of dev branch; symlink→real copy fix |
| Clock driver | 🟡 Compiles, incomplete | Uses generic `clk-provider` API; **real Samsung clocks use ACPM IPC** — needs ACPM mailbox driver |
| KMI symbol list | ✅ Complete | 193 symbols, all clock driver deps present |
| Device DTBs | ✅ Compile | 7 board DTBs; **all addresses match Samsung's real DT** |
| GIC | ✅ Correct | `arm,gic-400` (GICv2) at `0x12b00000` |
| MCT timer | ✅ Present | `samsung,exynos4210-mct` at `0x10050000` |
| CPU topology | ✅ Correct | 8 cores (6×Ananke/A55 + 2×Hercules/A78) |
| PMIC (S2MPS34) | ⚠️ DT only | 39 LDO + 9 BUCK defined; driver not ported |
| 13 vendor modules | ❌ Scaffolds | display, gpu, media, audio, modem, wifi, input, sensors, battery, fingerprint — empty |
| ACPM mailbox | ❌ Not ported | **THE blocker.** Without it, no clock/DVFS/PMIC can be programmed |
| Booting on device | ❌ | Requires ACPM + clock + PMIC + pinctrl |

> **Reality check**: no booting kernel exists for Exynos 1280 on any 6.x tree.
> The only proven source is Samsung's Linux **5.10.260** + CAL-IF. This project
> is a from-scratch 6.18 GKI bring-up. See `docs/STATUS.md`.

## Important Constraints

- **Windows**: Cannot build the kernel directly; use Linux or Docker.
- **Samsung source**: Required for actual driver code
  (`scripts/get-samsung-source.sh`).
- **KMI**: Vendor modules must only use exported symbols from
  `gki/aarch64/symbols/exynosnext`.
- **ACPM**: The clock tree is driven through ACPM IPC, not direct register
  writes. The ACPM mailbox driver must be ported before the clock driver can
  actually program clocks.
- **GICv2**: This SoC uses `arm,gic-400` (GICv2), **not** GICv3. Interrupt
  specifiers are 3-cell (`<type irq flags>`), with `GIC_SPI`/`GIC_PPI` macros.
- **CAL-IF**: Samsung's 5.10 clock framework. It cannot ship as a GKI `.ko`
  (its core symbols are not module-exported), so the clock driver here uses
  the generic exported `clk-provider` API instead.
- **Kernel version**: Samsung ships only 5.10 for this SoC; the 6.18 target is
  an unproven bring-up, not a repackage. Linux 7.x cannot be used (no mainline
  Exynos 1280 support, no ACK on 7.x).

## Common Tasks

### Adding a New Vendor Module

1. Create `vendor/samsung/<subsystem>/Kconfig`
2. Create `vendor/samsung/<subsystem>/Makefile`
3. Create `vendor/samsung/<subsystem>/BUILD.bazel` with `ddk_module()`
4. Add source files and wire into the build
5. Add the module to `build.config.exynosnext`
6. Add any new exported symbols to `gki/aarch64/symbols/exynosnext`

### Porting a Samsung Driver

1. Extract from Samsung source: `./scripts/get-samsung-source.sh`
2. Fix API changes for Linux 6.18 (~8 years of drift)
3. Ensure KMI compliance (check against the symbol list)
4. Add to `vendor_exynosnext.config` if it needs new CONFIG options
5. Update `docs/STATUS.md` with the new state

### Adding Device Support

1. Create `arch/arm64/boot/dts/samsung/exynos1280-<device>.dts`
2. Include `exynos1280.dtsi` and `exynos1280-pmic-s2mps34.dtsi`
3. Override board-specific nodes (panel, touchscreen, regulators)
4. Add the DTB target to `arch/arm64/boot/dts/samsung/Makefile`

## Code Conventions

- **Clock controllers**: Use generic `clk-provider` API
  (`clk_hw_register_*`, not CAL-IF)
- **Code style**: Follow upstream kernel style; run `scripts/checkpatch.pl`
- **Modules**: All must have `MODULE_LICENSE("GPL v2")`
- **DTS**: Use standard `dt-bindings/` headers; real addresses from the
  firmware audit (see `docs/reference-a53-real.dts`)
- **Interrupts**: GICv2 3-cell format (`<GIC_SPI irq IRQ_TYPE_*>`)
- **Bazel rules**: Use `ddk_module()` for all vendor modules
- **Kernel-doc**: Use for all public functions and structures
- **checkpatch**: Run before submitting; fix all warnings
