# CLAUDE.md - ExynosNext Kernel Project Context

> AI-assisted development context file for the ExynosNext Kernel project.
> Provides complete project knowledge so AI assistants can understand the codebase without re-reading everything.

---

## Project Overview

- **Name**: ExynosNext Kernel
- **Base**: Linux 6.18 LTS kernel (ACK android17-6.18 / GKI)
- **Target SoC**: Samsung Exynos 1280 (s5e8825)
- **Architecture**: Hybrid (ACK GKI kernel + Samsung vendor modules)
- **Target Devices**: Galaxy A53, A25, A26, A33, M33, M34, Tab S6 Lite (all Exynos 1280 variants)

## Build System

| Item | Details |
|------|---------|
| **Build tool** | Kleaf (Bazel) |
| **Main command** | `tools/bazel run //:exynosnext_dist` |
| **Build config** | `build.config.exynosnext` |
| **Defconfig fragment** | `vendor_exynosnext.config` (200+ options) |
| **Variants** | `ksu` (default), `vanilla`, `susu` |
| **Docker** | `docker compose run build` or `./scripts/build.sh --ksu` |

## Key Files

### Build & Configuration
| File | Purpose |
|------|---------|
| `BUILD.bazel` | Root Bazel build file |
| `MODULE.bazel` | Module definitions |
| `.bazelrc` | Bazel settings |
| `build.config.exynosnext` | Kernel build configuration |
| `vendor_exynosnext.config` | GKI defconfig fragment |

### Device Trees
| File | Purpose |
|------|---------|
| `arch/arm64/boot/dts/samsung/exynos1280.dtsi` | SoC DTS (60+ peripheral nodes) |
| `arch/arm64/boot/dts/samsung/exynos1280-a53x.dts` | Galaxy A53 device DTS |
| `arch/arm64/boot/dts/samsung/exynos1280-a25x.dts` | Galaxy A25 device DTS |
| `arch/arm64/boot/dts/samsung/exynos1280-a26x.dts` | Galaxy A26 device DTS |
| `arch/arm64/boot/dts/samsung/exynos1280-a33x.dts` | Galaxy A33 device DTS |
| `arch/arm64/boot/dts/samsung/exynos1280-m33x.dts` | Galaxy M33 device DTS |
| `arch/arm64/boot/dts/samsung/exynos1280-m34x.dts` | Galaxy M34 device DTS |
| `arch/arm64/boot/dts/samsung/exynos1280-gta4xls.dts` | Galaxy Tab S6 Lite device DTS |

### Clock Controller
| File | Purpose |
|------|---------|
| `vendor/samsung/clock/clk-s5e8825.c` | Clock controller driver (generic clk-provider API) |
| `vendor/samsung/clock/clk-s5e8825.h` | Clock ID definitions |
| `include/dt-bindings/clock/samsung,exynos1280-clk.h` | DTS clock bindings |

### KMI
| File | Purpose |
|------|---------|
| `gki/aarch64/symbols/exynosnext` | KMI symbol list (200+ symbols) |

### Scripts
| File | Purpose |
|------|---------|
| `scripts/build.sh` | Build script |
| `scripts/flash.sh` | Interactive flash tool (TUI) |
| `scripts/setup.sh` | Environment setup |
| `scripts/get-samsung-source.sh` | Samsung source downloader |
| `scripts/extract-modules.sh` | Module extraction from Samsung source |
| `scripts/docker-build.sh` | Docker build wrapper |
| `Dockerfile` | Docker build environment |
| `docker-compose.yml` | Docker Compose services |

### CI/CD
| File | Purpose |
|------|---------|
| `.github/workflows/build.yml` | CI pipeline (KSU + Vanilla) |

### Vendor Module Structure (per subsystem)
| File | Purpose |
|------|---------|
| `vendor/samsung/*/Kconfig` | Module configuration (12 subsystems) |
| `vendor/samsung/*/Makefile` | Module build rules |
| `vendor/samsung/*/BUILD.bazel` | DDK module rules |

## Architecture Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Kernel version | GKI android17-6.18 (Linux 6.18 LTS) | Samsung ACK base |
| Clock controller | Generic `clk-provider` API | GKI-compatible; real register data from CAL-IF still needed |
| Build system | Kleaf/Bazel | GKI requirement, DDK module support |
| Root solution | KernelSU Next | Primary root method for user builds |
| Module format | Loadable kernel modules (`.ko`) | GKI compatibility |
| KMI compliance | Required | Mandatory for GKI vendor modules |

## Porting Status

| Component | Status | Notes |
|-----------|--------|-------|
| GKI core kernel | **Builds** | CI syncs ACK android17-6.18, overlays this repo, builds real `Image` |
| Clock controller | **Partially ported** | Multi-CMU (9 regions), ~80 gates with real CAL-IF offsets + bit 21 enable; real PLL names; CPU mux (`CLK_MOUT_CPU`) registered; PLL rate tables still fixed-factor placeholders |
| KMI symbol list | **Complete** | All symbols used by the clock driver are present |
| Device trees | **Compile** | dtsi (60+ nodes) + 7 board DTS files; DTB targets enabled in Makefile; CI builds `dtbs`; all phandle references resolve |
| CPU topology | **Correct** | 8 cores (2×Cortex-A78 + 6×Cortex-A55) with `cpu-map` |
| Vendor modules (13) | **Not started** | Empty scaffolding; Samsung 5.10 source must be ported |
| Build system (Bazel) | **Untested** | Kleaf files present but CI uses make-based GKI build |
| CI/CD | **GKI + DTBs + clock module** | Builds GKI Image + DTBs + `clk_s5e8825.ko`; KSU variant integrates KernelSU-Next via `setup.sh` |
| Flash tool | **Complete** | Interactive TUI |

> Reality check: no booting kernel exists for Exynos 1280 on any 6.x tree. The
> only proven source is Samsung's Linux **5.10.260** + CAL-IF. This project is a
> from-scratch 6.18 GKI bring-up; treat everything above "GKI core" as work in
> progress. Linux 7.1.4 cannot be used because mainline has no Exynos 1280 SoC
> support at all. See `docs/STATUS.md`.

## Important Constraints

- **Windows**: Cannot build kernel directly; use Linux or Docker
- **Samsung source**: Required for actual driver code (`scripts/get-samsung-source.sh`)
- **KMI**: Vendor modules must only use exported symbols from `gki/aarch64/symbols/exynosnext`
- **Clock controller**: Must be ported first; blocks all other modules
- **CAL-IF**: Samsung's 5.10 clock framework (`soc/samsung/cal-if` + cmucal). It
  cannot ship as a GKI `.ko` (its core symbols are not module-exported), so the
  clock driver here uses the generic exported `clk-provider` API instead. Real
  register/PLL data must still be transcribed from the CAL-IF source.
- **Kernel version**: Samsung ships only 5.10 for this SoC; the 6.18 target is an
  unproven bring-up, not a repackage. Do not assume vendor drivers load as-is.

## Common Tasks

### Adding a New Vendor Module
1. Create `vendor/samsung/<subsystem>/Kconfig`
2. Create `vendor/samsung/<subsystem>/Makefile`
3. Create `vendor/samsung/<subsystem>/BUILD.bazel` with `ddk_module()`
4. Add source files and wire into build

### Porting a Samsung Driver
1. Extract from Samsung source: `./scripts/extract-modules.sh <module>`
2. Fix API changes for Linux 6.18
3. Ensure KMI compliance (check symbol list)
4. Add to `vendor_exynosnext.config`

### Adding Device Support
1. Create new DTS: `arch/arm64/boot/dts/samsung/exynos1280-<device>.dts`
2. Include `exynos1280.dtsi`
3. Add device-specific nodes and overrides
4. Add defconfig if needed

### Updating Configs
- Edit `vendor_exynosnext.config` for GKI options
- Edit `vendor/samsung/*/Kconfig` for module options

### Building
```bash
# Docker (recommended)
docker compose run build

# Local Linux
./scripts/build.sh --ksu

# Bazel directly
tools/bazel run //:exynosnext_dist
```

## Code Conventions

- **Clock controllers**: Use generic `clk-provider` API (`clk_hw_register_*`, not CAL-IF)
- **Code style**: Follow upstream kernel style; run `scripts/checkpatch.pl`
- **Modules**: All must have `MODULE_LICENSE("GPL v2")`
- **DTS**: Use standard `dt-bindings/` headers for bindings
- **Bazel rules**: Use `ddk_module()` for all vendor modules
- **Kernel-doc**: Use for all public functions and structures
- **checkpatch**: Run before submitting; fix all warnings
