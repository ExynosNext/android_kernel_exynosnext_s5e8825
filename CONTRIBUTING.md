# Contributing to ExynosNext Kernel

Thank you for your interest in contributing to ExynosNext! This guide will help you get started.

## Getting Started

### Prerequisites

- Linux system (Ubuntu 22.04+ recommended) or Docker
- `git`, `repo`, `clang`, `make`, `bc`, `bison`, `flex`
- Samsung Galaxy A53 5G (or other Exynos 1280 device) for testing

### Development Setup

```bash
# Clone the repository
git clone https://github.com/ExynosNext/android_kernel_exynosnext_s5e8825.git
cd android_kernel_exynosnext_s5e8825

# Option A: Docker (recommended for Windows/macOS)
docker compose run setup    # Install deps + sync ACK
docker compose run build    # Build kernel

# Option B: Native Linux
make setup       # Install dependencies
make sync        # Sync ACK android17-6.18
make samsung-source  # Download Samsung kernel source
```

## How to Contribute

### Reporting Bugs

1. Search [existing issues](https://github.com/ExynosNext/android_kernel_exynosnext_s5e8825/issues) first
2. Open a new issue with the **Bug Report** template
3. Include: device model, kernel version, steps to reproduce, logs

### Suggesting Features

1. Open an issue with the **Feature Request** template
2. Describe the use case and expected behavior
3. Note if you're willing to implement it

### Submitting Code

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/my-feature`
3. Make your changes
4. Test on real hardware if possible
5. Submit a pull request

## Development Guidelines

### Code Style

- Follow the [Linux kernel coding style](https://www.kernel.org/doc/html/latest/process/coding-style.html)
- Run `scripts/checkpatch.pl` before submitting
- Use `kernel-doc` for public functions and structures
- All modules must have `MODULE_LICENSE("GPL v2")`

### Commit Messages

Follow the kernel commit message format:

```
module: short description

Detailed explanation of what this patch does and why.

Signed-off-by: Your Name <your.email@example.com>
```

Examples:
- `clock: add Exynos 1280 clock controller`
- `dts: add A53x device tree`
- `wifi: fix SCSC driver build on 6.18`

### Porting Samsung Drivers

When porting drivers from Samsung's 5.10 kernel to 6.18:

1. Extract source: `./scripts/extract-modules.sh`
2. Check API compatibility (see `docs/PORTING.md`)
3. Fix compilation errors
4. Update device tree bindings
5. Test module loading

### Device Tree Changes

- Use standard `dt-bindings/` headers
- Follow existing patterns in `arch/arm64/boot/dts/samsung/`
- Test DTS compilation: `make ARCH=arm64 dtbs`

## Testing

### Build Testing

```bash
# Full build
docker compose run build

# Quick build (skip sync)
docker compose run quick-build
```

### Hardware Testing

1. Flash kernel to device
2. Check kernel log: `adb shell dmesg | head -100`
3. Test all peripherals (display, touch, WiFi, audio, camera)
4. Report any regressions

### CI

All pull requests are automatically tested via GitHub Actions. The CI builds both KSU and vanilla variants.

## Getting Help

- [GitHub Issues](https://github.com/ExynosNext/android_kernel_exynosnext_s5e8825/issues) — Bug reports and feature requests
- [README.md](README.md) — Project overview and build instructions
- [docs/PORTING.md](docs/PORTING.md) — Driver porting tracker
- [CLAUDE.md](CLAUDE.md) — AI-assisted development context

## License

By contributing, you agree that your contributions will be licensed under the GNU General Public License version 2.
