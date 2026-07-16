# ============================================================================
# ExynosNext Kernel — Convenience Makefile
# Quick commands for building, flashing, and managing the project
# ============================================================================

SHELL := /bin/bash
.DEFAULT_GOAL := help

# ── Variables ───────────────────────────────────────────────────────────────
JOBS       ?= $(shell nproc --all 2>/dev/null || echo 8)
VARIANT    ?= ksu
CCACHE     ?= 1
OUTDIR     := out
DISTDIR    := $(OUTDIR)/dist

# ── Help ────────────────────────────────────────────────────────────────────
.PHONY: help
help: ## Show this help
	@echo ""
	@echo "  ExynosNext Kernel Build System"
	@echo "  ═══════════════════════════════════════════"
	@echo ""
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | \
		awk 'BEGIN {FS = ":.*?## "}; {printf "  \033[36m%-20s\033[0m %s\n", $$1, $$2}'
	@echo ""

# ── Setup ───────────────────────────────────────────────────────────────────
.PHONY: setup
setup: ## Install all build dependencies
	bash scripts/setup.sh

.PHONY: sync
sync: ## Sync ACK android17-6.18 sources via repo
	repo init -u https://android.googlesource.com/kernel/manifest \
		-b common-android17-6.18 --depth=1
	repo sync -j$(JOBS) --no-tags

.PHONY: samsung-source
samsung-source: ## Download Samsung Android 16 kernel source
	bash scripts/get-samsung-source.sh

# ── Build ───────────────────────────────────────────────────────────────────
.PHONY: build
build: ## Build kernel (default: KSU variant)
	bash scripts/build.sh --$(VARIANT) --jobs=$(JOBS)

.PHONY: build-ksu
build-ksu: ## Build with KernelSU Next
	bash scripts/build.sh --ksu --jobs=$(JOBS)

.PHONY: build-vanilla
build-vanilla: ## Build without root
	bash scripts/build.sh --vanilla --jobs=$(JOBS)

.PHONY: build-sukisu
build-sukisu: ## Build with SukiSU Ultra
	bash scripts/build.sh --sukisu --jobs=$(JOBS)

.PHONY: build-lto
build-lto: ## Build with Full LTO
	bash scripts/build.sh --$(VARIANT) --lto --jobs=$(JOBS)

.PHONY: clean
clean: ## Clean build tree
	bash scripts/build.sh --clean --jobs=$(JOBS)

.PHONY: mrproper
mrproper: ## Deep clean (removes all build artifacts)
	rm -rf out/
	cd common && make ARCH=arm64 LLVM=1 mrproper 2>/dev/null || true

# ── Flash ───────────────────────────────────────────────────────────────────
.PHONY: flash
flash: ## Interactive flash tool
	bash scripts/flash.sh

.PHONY: flash-boot
flash-boot: ## Flash boot.img only
	@read -p "Proceed? [y/N]: " c; [ "$$c" = "y" ] && \
	fastboot flash boot $(DISTDIR)/boot.img

.PHONY: flash-all
flash-all: ## Flash boot + vendor_boot + dtbo
	@read -p "Full flash. Proceed? [y/N]: " c; [ "$$c" = "y" ] && \
	fastboot flash boot $(DISTDIR)/boot.img && \
	fastboot flash vendor_boot $(DISTDIR)/vendor_boot.img && \
	fastboot flash dtbo $(DISTDIR)/dtbo.img && \
	fastboot reboot

# ── Info ────────────────────────────────────────────────────────────────────
.PHONY: info
info: ## Show project info
	@echo ""
	@echo "  ExynosNext Kernel"
	@echo "  ═══════════════════════════════════════════"
	@echo "  Base:      ACK android17-6.18 (Linux 6.18 LTS)"
	@echo "  SoC:       Samsung Exynos 1280 (s5e8825)"
	@echo "  Devices:   A25, A26, A33, A53, M33, M34, Tab S6 Lite"
	@echo "  Root:      KernelSU Next"
	@echo "  Build:     Kleaf (Bazel) + DDK"
	@echo "  Variant:   $(VARIANT)"
	@echo "  Jobs:      $(JOBS)"
	@echo ""

.PHONY: check
check: ## Check build environment
	@echo "  Checking toolchain..."
	@command -v clang >/dev/null && echo "  ✓ clang" || echo "  ✗ clang (MISSING)"
	@command -v ld.lld >/dev/null && echo "  ✓ ld.lld" || echo "  ✗ ld.lld (MISSING)"
	@command -v llvm-ar >/dev/null && echo "  ✓ llvm-ar" || echo "  ✗ llvm-ar (MISSING)"
	@command -v python3 >/dev/null && echo "  ✓ python3" || echo "  ✗ python3 (MISSING)"
	@command -v repo >/dev/null && echo "  ✓ repo" || echo "  ✗ repo (MISSING)"
	@command -v bazel >/dev/null && echo "  ✓ bazel" || echo "  ✗ bazel (MISSING)"
	@command -v ccache >/dev/null && echo "  ✓ ccache" || echo "  ✗ ccache (MISSING)"
	@command -v adb >/dev/null && echo "  ✓ adb" || echo "  ✗ adb (MISSING)"
	@command -v fastboot >/dev/null && echo "  ✓ fastboot" || echo "  ✗ fastboot (MISSING)"
	@echo ""
