#!/usr/bin/env bash
# ============================================================================
# ExynosNext Environment Setup Script
# Installs all dependencies needed to build the kernel
# ============================================================================

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
RESET='\033[0m'

log()  { echo -e "${CYAN}[SETUP]${RESET} $*"; }
ok()   { echo -e "${GREEN}[OK]${RESET} $*"; }
warn() { echo -e "${YELLOW}[WARN]${RESET} $*"; }
err()  { echo -e "${RED}[ERROR]${RESET} $*" >&2; }

# ── Detect distro ──────────────────────────────────────────────────────────
detect_distro() {
    if [[ -f /etc/os-release ]]; then
        . /etc/os-release
        DISTRO="$ID"
    elif command -v apt-get &>/dev/null; then
        DISTRO="debian"
    elif command -v dnf &>/dev/null; then
        DISTRO="fedora"
    elif command -v pacman &>/dev/null; then
        DISTRO="arch"
    else
        DISTRO="unknown"
    fi
    log "Detected distro: $DISTRO"
}

# ── Install deps ───────────────────────────────────────────────────────────
install_deps() {
    log "Installing build dependencies..."

    case "$DISTRO" in
        ubuntu|debian|pop|linuxmint)
            sudo apt-get update -qq
            sudo apt-get install -y -qq \
                bc bison build-essential ccache curl flex git \
                libncurses5-dev libssl-dev python3 python3-pip \
                lz4 zip unzip tar xz-utils cpio \
                gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu \
                device-tree-compiler
            ;;
        fedora|rhel|centos)
            sudo dnf install -y \
                bc bison flex gcc make automake autoconf \
                openssl-devel ncurses-devel python3 \
                lz4 zip unzip tar xz cpio \
                gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu \
                dtc
            ;;
        arch|manjaro)
            sudo pacman -S --noconfirm \
                base-devel bc bison flex ccache curl git \
                python wget lz4 zip unzip tar xz cpio \
                aarch64-linux-gnu-gcc aarch64-linux-gnu-binutils \
                dtc
            ;;
        *)
            warn "Unknown distro. Install manually:"
            echo "  bc bison build-essential ccache curl flex git"
            echo "  libncurses5-dev libssl-dev python3 python3-pip"
            echo "  lz4 zip unzip tar xz-utils cpio"
            echo "  aarch64-linux-gnu-gcc (cross-compiler)"
            echo "  device-tree-compiler"
            return 1
            ;;
    esac

    ok "Build dependencies installed"
}

# ── Install repo tool ──────────────────────────────────────────────────────
install_repo() {
    if command -v repo &>/dev/null; then
        ok "repo tool already installed"
        return
    fi

    log "Installing repo tool..."
    mkdir -p ~/bin
    curl -s https://storage.googleapis.com/git-repo-downloads/repo > ~/bin/repo
    chmod a+x ~/bin/repo

    # Add to PATH if not already
    if ! echo "$PATH" | grep -q "$HOME/bin"; then
        echo 'export PATH="$HOME/bin:$PATH"' >> ~/.bashrc
        export PATH="$HOME/bin:$PATH"
    fi

    ok "repo tool installed"
}

# ── Install Bazelisk ───────────────────────────────────────────────────────
install_bazelisk() {
    if command -v bazel &>/dev/null; then
        ok "Bazelisk already installed"
        return
    fi

    log "Installing Bazelisk..."
    local os arch
    os=$(uname -s | tr '[:upper:]' '[:lower:]')
    arch=$(uname -m)
    [[ "$arch" == "x86_64" ]] && arch="amd64"
    [[ "$arch" == "aarch64" ]] && arch="arm64"

    curl -Lo /tmp/bazelisk "https://github.com/bazelbuild/bazelisk/releases/latest/download/bazelisk-${os}-${arch}"
    chmod +x /tmp/bazelisk
    sudo mv /tmp/bazelisk /usr/local/bin/bazel

    ok "Bazelisk installed"
}

# ── Configure ccache ────────────────────────────────────────────────────────
setup_ccache() {
    log "Configuring ccache..."
    mkdir -p "${HOME}/.ccache/exynosnext"
    ccache -M 50G 2>/dev/null || true
    ok "ccache configured (50GB max)"
}

# ── Initialize repo ────────────────────────────────────────────────────────
init_repo() {
    local project_dir="$1"
    log "Initializing repo workspace..."

    cd "$project_dir"
    repo init -u https://android.googlesource.com/kernel/manifest -b common-android17-6.18 --depth=1
    repo sync -j$(nproc --all) --no-tags

    ok "ACK android17-6.18 synced"
}

# ── Main ────────────────────────────────────────────────────────────────────
main() {
    echo ""
    echo -e "${BOLD}╔══════════════════════════════════════════════╗${RESET}"
    echo -e "${BOLD}║   ExynosNext Environment Setup               ║${RESET}"
    echo -e "${BOLD}╚══════════════════════════════════════════════╝${RESET}"
    echo ""

    detect_distro
    install_deps
    install_repo
    install_bazelisk
    setup_ccache

    echo ""
    ok "Environment setup complete!"
    echo ""
    echo "  Next steps:"
    echo "    1. Clone the repo:  git clone https://github.com/ExynosNext/android_kernel_exynosnext_s5e8825.git"
    echo "    2. Enter directory: cd android_kernel_exynosnext_s5e8825"
    echo "    3. Initialize ACK:  repo init -u https://android.googlesource.com/kernel/manifest -b common-android17-6.18"
    echo "    4. Sync sources:    repo sync -j\$(nproc)"
    echo "    5. Build:           tools/bazel run //:exynosnext_dist"
    echo ""
}

main "$@"
