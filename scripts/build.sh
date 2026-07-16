#!/usr/bin/env bash
# ============================================================================
# ExynosNext Kernel Build Script
# Builds GKI kernel + Samsung vendor modules for Exynos 1280
# ============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_DIR"

# ── Colors ──────────────────────────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
RESET='\033[0m'

log_info()  { echo -e "${CYAN}[INFO]${RESET} $*"; }
log_ok()    { echo -e "${GREEN}[OK]${RESET} $*"; }
log_warn()  { echo -e "${YELLOW}[WARN]${RESET} $*"; }
log_err()   { echo -e "${RED}[ERROR]${RESET} $*" >&2; }

# ── Defaults ────────────────────────────────────────────────────────────────
DEFCONFIG="${DEFCONFIG:-gki_defconfig}"
JOBS="${JOBS:-$(nproc --all)}"
CCACHE="${CCACHE:-1}"
FULL_LTO="${FULL_LTO:-0}"
VARIANT="${VARIANT:-ksu}"

# ── Parse args ──────────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --clean)     CLEAN=1; shift ;;
        --no-ccache) CCACHE=0; shift ;;
        --lto)       FULL_LTO=1; shift ;;
        --vanilla)   VARIANT="vanilla"; shift ;;
        --ksu)       VARIANT="ksu"; shift ;;
        --sukisu)    VARIANT="sukisu"; shift ;;
        --jobs=*)    JOBS="${1#*=}"; shift ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --clean       Clean build (make mrproper)"
            echo "  --no-ccache   Disable ccache"
            echo "  --lto         Enable Full LTO (slower, smaller binary)"
            echo "  --vanilla     Build without root (no KernelSU)"
            echo "  --ksu         Build with KernelSU Next (default)"
            echo "  --sukisu      Build with SukiSU Ultra"
            echo "  --jobs=N      Parallel jobs (default: nproc)"
            echo "  --help        Show this help"
            exit 0
            ;;
        *) log_err "Unknown option: $1"; exit 1 ;;
    esac
done

# ── Environment ─────────────────────────────────────────────────────────────
export ARCH=arm64
export SUBARCH=arm64
export PLATFORM_VERSION=16
export ANDROID_MAJOR_VERSION=v
export LLVM=1
export LLVM_IAS=1
export CC=clang
export LD=ld.lld
export AR=llvm-ar
export NM=llvm-nm
export OBJCOPY=llvm-objcopy
export OBJDUMP=llvm-objdump
export STRIP=llvm-strip

if [[ "$CCACHE" -eq 1 ]]; then
    export USE_CCACHE=1
    export CCACHE_DIR="${HOME}/.ccache/exynosnext"
    export CCACHE_EXEC="$(which ccache 2>/dev/null || true)"
    if [[ -z "$CCACHE_EXEC" ]]; then
        log_warn "ccache not found, building without cache"
        export USE_CCACHE=0
    fi
fi

OUTDIR="$PROJECT_DIR/out"
DISTDIR="$OUTDIR/dist"
KDIR="$PROJECT_DIR/common"

# ── Toolchain check ─────────────────────────────────────────────────────────
check_toolchain() {
    log_info "Checking toolchain..."
    local missing=0
    for tool in clang ld.lld llvm-ar llvm-nm llvm-objcopy llvm-objdump llvm-strip; do
        if ! command -v "$tool" &>/dev/null; then
            log_err "Missing: $tool"
            missing=1
        fi
    done
    if ! command -v python3 &>/dev/null; then
        log_err "Missing: python3"
        missing=1
    fi
    if [[ "$missing" -eq 1 ]]; then
        log_err "Install missing tools and retry"
        exit 1
    fi
    log_ok "Toolchain ready"
}

# ── Clean ───────────────────────────────────────────────────────────────────
do_clean() {
    if [[ "${CLEAN:-0}" -eq 1 ]]; then
        log_info "Cleaning build tree..."
        cd "$KDIR"
        make ARCH=arm64 LLVM=1 LLVM_IAS=1 mrproper 2>/dev/null || true
        rm -rf "$OUTDIR"
        log_ok "Clean complete"
    fi
}

# ── Build ───────────────────────────────────────────────────────────────────
do_build() {
    log_info "Building ExynosNext kernel ($VARIANT mode)..."
    log_info "  Kernel:   Linux $(make -s -C "$KDIR" kernelversion 2>/dev/null || echo 'unknown')"
    log_info "  Config:   $DEFCONFIG"
    log_info "  Jobs:     $JOBS"
    log_info "  LTO:      $([ "$FULL_LTO" -eq 1 ] && echo 'Full' || echo 'Thin')"
    log_info "  Variant:  $VARIANT"
    echo ""

    cd "$KDIR"

    # Generate defconfig
    log_info "Generating defconfig..."
    make -j"$JOBS" \
        ARCH=arm64 \
        SUBARCH=arm64 \
        LLVM=1 \
        LLVM_IAS=1 \
        CC=clang \
        LD=ld.lld \
        "$DEFCONFIG" \
        2>&1 | tail -5

    # Build kernel
    log_info "Compiling kernel..."
    make -j"$JOBS" \
        ARCH=arm64 \
        SUBARCH=arm64 \
        LLVM=1 \
        LLVM_IAS=1 \
        CC=clang \
        LD=ld.lld \
        AR=llvm-ar \
        NM=llvm-nm \
        OBJCOPY=llvm-objcopy \
        OBJDUMP=llvm-objdump \
        STRIP=llvm-strip \
        2>&1 | tee "$OUTDIR/build.log" | tail -20

    log_ok "Kernel build complete"
}

# ── Package ─────────────────────────────────────────────────────────────────
do_package() {
    log_info "Packaging..."
    mkdir -p "$DISTDIR"

    # Copy kernel image
    local image="$KDIR/out/arch/arm64/boot/Image"
    if [[ -f "$image" ]]; then
        cp "$image" "$DISTDIR/"
        log_ok "Image: $(du -h "$image" | cut -f1)"
    else
        log_err "Image not found at $image"
        exit 1
    fi

    # Copy modules
    local moddir="$KDIR/out/lib/modules"
    if [[ -d "$moddir" ]]; then
        cp -r "$moddir" "$DISTDIR/"
        log_ok "Modules copied"
    fi

    log_ok "Packaging complete → $DISTDIR"
}

# ── Main ────────────────────────────────────────────────────────────────────
main() {
    echo ""
    echo -e "${BOLD}╔══════════════════════════════════════════════╗${RESET}"
    echo -e "${BOLD}║     ExynosNext Kernel Build System           ║${RESET}"
    echo -e "${BOLD}║     Linux 6.18 LTS · Exynos 1280             ║${RESET}"
    echo -e "${BOLD}╚══════════════════════════════════════════════╝${RESET}"
    echo ""

    mkdir -p "$OUTDIR"

    check_toolchain
    do_clean
    do_build
    do_package

    echo ""
    log_ok "Build finished!"
    echo -e "  Output: ${CYAN}$DISTDIR${RESET}"
    echo ""
}

main "$@"
