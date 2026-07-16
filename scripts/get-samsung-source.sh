#!/usr/bin/env bash
# ============================================================================
# Samsung Kernel Source Download Script
# Downloads Samsung's open-source kernel release for Exynos 1280 devices
# ============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
RESET='\033[0m'

log()  { echo -e "${CYAN}[SAMSUNG]${RESET} $*"; }
ok()   { echo -e "${GREEN}[OK]${RESET} $*"; }
warn() { echo -e "${YELLOW}[WARN]${RESET} $*"; }
err()  { echo -e "${RED}[ERROR]${RESET} $*" >&2; }

SAMSUNG_DIR="$PROJECT_DIR/vendor/samsung/kernel-source"
mkdir -p "$SAMSUNG_DIR"

echo ""
echo -e "${BOLD}╔══════════════════════════════════════════════╗${RESET}"
echo -e "${BOLD}║   Samsung Kernel Source Downloader            ║${RESET}"
echo -e "${BOLD}╚══════════════════════════════════════════════╝${RESET}"
echo ""

# ── Known Samsung kernel source URLs ────────────────────────────────────────
# These are direct download links from opensource.samsung.com
# Format: URL -> filename
#
# NOTE: Samsung's open source portal may change URLs.
# If a URL fails, manually download from:
#   https://opensource.samsung.com/uploadSearch?searchValue=A536B

declare -A SOURCES=(
    # Galaxy A53 5G - Android 16 (OneUI 8)
    ["SM-A536B"]="https://opensource.samsung.com/uploadUploads/SM-A536B_EUR_16_Opensource.zip"

    # Galaxy A25 - Android 16
    ["SM-A256E"]="https://opensource.samsung.com/uploadUploads/SM-A256E_CIS_16_Opensource.zip"

    # Galaxy A33 - Android 15
    ["SM-A336B"]="https://opensource.samsung.com/uploadUploads/SM-A336B_EUR_15_Opensource.zip"

    # Galaxy M33 - Android 15
    ["SM-M336B"]="https://opensource.samsung.com/uploadUploads/SM-M336B_Ins_15_Opensource.zip"

    # Galaxy M34 - Android 15
    ["SM-M346B"]="https://opensource.samsung.com/uploadUploads/SM-M346B_Ins_15_Opensource.zip"
)

# ── Menu ────────────────────────────────────────────────────────────────────
echo "  Select device to download kernel source for:"
echo ""
echo "  ${CYAN}1)${RESET} SM-A536B (Galaxy A53 5G) — Android 16"
echo "  ${CYAN}2)${RESET} SM-A256E (Galaxy A25) — Android 16"
echo "  ${CYAN}3)${RESET} SM-A336B (Galaxy A33) — Android 15"
echo "  ${CYAN}4)${RESET} SM-M336B (Galaxy M33) — Android 15"
echo "  ${CYAN}5)${RESET} SM-M346B (Galaxy M34) — Android 15"
echo "  ${CYAN}6)${RESET} All devices"
echo "  ${CYAN}0)${RESET} Exit"
echo ""
read -rp "  Select [1-6]: " choice

download_source() {
    local model="$1"
    local url="${SOURCES[$model]}"
    local zipfile="$SAMSUNG_DIR/${model}_kernel_source.zip"

    if [[ -z "$url" ]]; then
        err "No source URL known for $model"
        return 1
    fi

    log "Downloading $model kernel source..."
    log "URL: $url"

    if command -v wget &>/dev/null; then
        wget -q --show-progress -O "$zipfile" "$url" || {
            err "Download failed for $model"
            warn "Manually download from: https://opensource.samsung.com/uploadSearch?searchValue=$model"
            return 1
        }
    elif command -v curl &>/dev/null; then
        curl -L --progress-bar -o "$zipfile" "$url" || {
            err "Download failed for $model"
            warn "Manually download from: https://opensource.samsung.com/uploadSearch?searchValue=$model"
            return 1
        }
    else
        err "Neither wget nor curl found"
        return 1
    fi

    if [[ ! -s "$zipfile" ]]; then
        err "Downloaded file is empty"
        rm -f "$zipfile"
        return 1
    fi

    ok "Downloaded: $(du -h "$zipfile" | cut -f1)"

    # Extract
    log "Extracting..."
    local extract_dir="$SAMSUNG_DIR/$model"
    mkdir -p "$extract_dir"
    unzip -q -o "$zipfile" -d "$extract_dir" || {
        err "Extraction failed"
        return 1
    }

    ok "Extracted to: $extract_dir"

    # Find kernel source directory
    local kernel_dir
    kernel_dir=$(find "$extract_dir" -maxdepth 3 -name "Makefile" -exec grep -l "EXTRAVERSION.*s5e8825\|EXTRAVERSION.*5.10" {} \; 2>/dev/null | head -1 | xargs dirname 2>/dev/null) || true

    if [[ -n "$kernel_dir" ]]; then
        ok "Kernel source found: $kernel_dir"

        # Create symlink for easy access
        ln -sfn "$kernel_dir" "$SAMSUNG_DIR/kernel"
        ok "Symlinked: $SAMSUNG_DIR/kernel -> $kernel_dir"
    else
        warn "Kernel Makefile not found automatically. Check $extract_dir"
    fi
}

case "$choice" in
    1) download_source "SM-A536B" ;;
    2) download_source "SM-A256E" ;;
    3) download_source "SM-A336B" ;;
    4) download_source "SM-M336B" ;;
    5) download_source "SM-M346B" ;;
    6)
        for model in SM-A536B SM-A256E SM-A336B SM-M336B SM-M346B; do
            download_source "$model" || warn "Failed: $model"
        done
        ;;
    0) exit 0 ;;
    *) err "Invalid choice"; exit 1 ;;
esac

echo ""
ok "Done!"
echo ""
echo "  Next steps:"
echo "    1. Review kernel source in: $SAMSUNG_DIR/"
echo "    2. Copy needed drivers to vendor/samsung/"
echo "    3. Port drivers to Linux 6.18 API"
echo "    4. Build: make build"
echo ""
