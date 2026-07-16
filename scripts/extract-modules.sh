#!/usr/bin/env bash
# ============================================================================
# Vendor Module Extraction Script
# Extracts Samsung vendor drivers from downloaded kernel source and places
# them in the correct vendor/samsung/ directories for Linux 6.18 porting
# ============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
SAMSUNG_DIR="$PROJECT_DIR/vendor/samsung/kernel-source"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
RESET='\033[0m'

log()  { echo -e "${CYAN}[EXTRACT]${RESET} $*"; }
ok()   { echo -e "${GREEN}[OK]${RESET} $*"; }
warn() { echo -e "${YELLOW}[WARN]${RESET} $*"; }
err()  { echo -e "${RED}[ERROR]${RESET} $*" >&2; }

# ── Find Samsung kernel source ──────────────────────────────────────────
find_samsung_kernel() {
    local kernel_dir=""

    # Try the symlink first
    if [[ -L "$SAMSUNG_DIR/kernel" ]]; then
        kernel_dir=$(readlink -f "$SAMSUNG_DIR/kernel")
        if [[ -d "$kernel_dir" ]]; then
            echo "$kernel_dir"
            return 0
        fi
    fi

    # Search for kernel Makefile
    kernel_dir=$(find "$SAMSUNG_DIR" -maxdepth 4 -name "Makefile" -exec grep -l "EXTRAVERSION.*s5e8825\|EXTRAVERSION.*5.10" {} \; 2>/dev/null | head -1 | xargs dirname 2>/dev/null) || true

    if [[ -n "$kernel_dir" ]]; then
        ln -sfn "$kernel_dir" "$SAMSUNG_DIR/kernel"
        echo "$kernel_dir"
        return 0
    fi

    # Fallback: look for drivers/samsung
    kernel_dir=$(find "$SAMSUNG_DIR" -maxdepth 4 -type d -name "samsung" -path "*/drivers/*" 2>/dev/null | head -1 | xargs dirname 2>/dev/null | xargs dirname 2>/dev/null) || true

    if [[ -n "$kernel_dir" ]]; then
        ln -sfn "$kernel_dir" "$SAMSUNG_DIR/kernel"
        echo "$kernel_dir"
        return 0
    fi

    return 1
}

# ── Copy files safely ──────────────────────────────────────────────────
safe_copy() {
    local src="$1" dst="$2"
    if [[ -d "$src" ]]; then
        mkdir -p "$dst"
        cp -r "$src"/* "$dst"/ 2>/dev/null || true
        ok "Copied: $(basename "$src") → $dst"
    fi
}

# ── Extract Clock Controller ────────────────────────────────────────────
extract_clock() {
    log "Extracting clock controller..."
    local src="$1"

    # Samsung's clock driver location
    local clock_src=$(find "$src" -path "*/clk*/*s5e8825*" -o -path "*/clk*/*8825*" 2>/dev/null | head -5) || true

    if [[ -n "$clock_src" ]]; then
        local dst="$PROJECT_DIR/vendor/samsung/kernel-source/clock-extract"
        mkdir -p "$dst"
        for f in $clock_src; do
            cp "$f" "$dst/"
            log "  Copied: $(basename "$f")"
        done
        ok "Clock source extracted to: $dst"
    else
        warn "No clock source found for s5e8825"
        warn "Look in: $src/drivers/clk/samsung/"
    fi
}

# ── Extract Display Drivers ─────────────────────────────────────────────
extract_display() {
    log "Extracting display drivers..."
    local src="$1"

    # DECON (Display Engine Controller)
    local decon_src="$src/drivers/gpu/drm/samsung/decon"
    if [[ -d "$decon_src" ]]; then
        safe_copy "$decon_src" "$PROJECT_DIR/vendor/samsung/kernel-source/display/decon"
    fi

    # DSIM (MIPI DSI)
    local dsim_src="$src/drivers/gpu/drm/samsung/dsim"
    if [[ -d "$dsim_src" ]]; then
        safe_copy "$dsim_src" "$PROJECT_DIR/vendor/samsung/kernel-source/display/dsim"
    fi

    # Panel drivers
    local panel_src="$src/drivers/gpu/drm/panel"
    if [[ -d "$panel_src" ]]; then
        mkdir -p "$PROJECT_DIR/vendor/samsung/kernel-source/display/panel"
        # Copy Samsung-specific panels
        find "$panel_src" -name "*.c" -exec grep -l "samsung\|s6e3\|ams646\|ea8074\|nt36\|a53" {} \; 2>/dev/null | while read -r f; do
            cp "$f" "$PROJECT_DIR/vendor/samsung/kernel-source/display/panel/"
            log "  Panel: $(basename "$f")"
        done
    fi

    # DPU
    local dpu_src="$src/drivers/gpu/drm/samsung/decon"
    if [[ -d "$dpu_src" ]]; then
        safe_copy "$dpu_src" "$PROJECT_DIR/vendor/samsung/kernel-source/display/dpu"
    fi
}

# ── Extract GPU Driver ──────────────────────────────────────────────────
extract_gpu() {
    log "Extracting GPU driver..."
    local src="$1"

    # Mali G78 driver
    local mali_src=$(find "$src" -maxdepth 3 -type d -name "mali*" -path "*/gpu/*" 2>/dev/null | head -1) || true

    if [[ -n "$mali_src" ]]; then
        safe_copy "$mali_src" "$PROJECT_DIR/vendor/samsung/kernel-source/gpu/mali"
    else
        warn "Mali driver not found in Samsung source"
        warn "Check: $src/drivers/gpu/arm/"
    fi
}

# ── Extract MFC Driver ──────────────────────────────────────────────────
extract_mfc() {
    log "Extracting MFC codec driver..."
    local src="$1"

    local mfc_src=$(find "$src" -maxdepth 3 -type d -name "mfc*" -path "*/media/*" 2>/dev/null | head -1) || true

    if [[ -n "$mfc_src" ]]; then
        safe_copy "$mfc_src" "$PROJECT_DIR/vendor/samsung/kernel-source/media/mfc"
    else
        warn "MFC driver not found"
        warn "Check: $src/drivers/media/platform/samsung/mfc/"
    fi
}

# ── Extract Audio Drivers ───────────────────────────────────────────────
extract_audio() {
    log "Extracting audio drivers..."
    local src="$1"

    # ABOX (Samsung Audio DSP)
    local abox_src=$(find "$src" -maxdepth 3 -type d -name "abox" 2>/dev/null | head -1) || true
    if [[ -n "$abox_src" ]]; then
        safe_copy "$abox_src" "$PROJECT_DIR/vendor/samsung/kernel-source/audio/abox"
    fi

    # Codec drivers
    local codec_src="$src/sound/soc/codecs"
    if [[ -d "$codec_src" ]]; then
        local dst="$PROJECT_DIR/vendor/samsung/kernel-source/audio/codec"
        mkdir -p "$dst"
        for codec in rt5665.c rt5665.h rt5691.c rt5691.h; do
            if [[ -f "$codec_src/$codec" ]]; then
                cp "$codec_src/$codec" "$dst/"
                log "  Codec: $codec"
            fi
        done
    fi

    # Amplifier drivers
    local amp_dst="$PROJECT_DIR/vendor/samsung/kernel-source/audio/amp"
    mkdir -p "$amp_dst"
    for amp in tfa9878 aw882xx; do
        local amp_src=$(find "$src" -name "*${amp}*" -type f 2>/dev/null | head -3) || true
        if [[ -n "$amp_src" ]]; then
            for f in $amp_src; do
                cp "$f" "$amp_dst/"
            done
            log "  Amp: $amp"
        fi
    done
}

# ── Extract WiFi Driver ─────────────────────────────────────────────────
extract_wifi() {
    log "Extracting WiFi driver..."
    local src="$1"

    local wifi_src=$(find "$src" -maxdepth 3 -type d -name "scsc" -path "*/wifi/*" 2>/dev/null | head -1) || true
    if [[ -z "$wifi_src" ]]; then
        wifi_src=$(find "$src" -maxdepth 4 -type d -name "scsc" 2>/dev/null | head -1) || true
    fi

    if [[ -n "$wifi_src" ]]; then
        safe_copy "$wifi_src" "$PROJECT_DIR/vendor/samsung/kernel-source/wifi/scsc"
    else
        warn "WiFi SCSC driver not found"
        warn "Check: $src/drivers/net/wireless/scsc/"
    fi
}

# ── Extract Modem Driver ────────────────────────────────────────────────
extract_modem() {
    log "Extracting modem CPIF driver..."
    local src="$1"

    local cpif_src=$(find "$src" -maxdepth 4 -type d -name "cpif" 2>/dev/null | head -1) || true

    if [[ -n "$cpif_src" ]]; then
        safe_copy "$cpif_src" "$PROJECT_DIR/vendor/samsung/kernel-source/modem/cpif"
    else
        warn "CPIF driver not found"
        warn "Check: $src/drivers/net/samsung/cpif/"
    fi
}

# ── Extract Touchscreen Driver ──────────────────────────────────────────
extract_touch() {
    log "Extracting touchscreen drivers..."
    local src="$1"

    # Goodix
    local goodix_src=$(find "$src" -maxdepth 4 -type d -name "goodix*" 2>/dev/null | head -1) || true
    if [[ -n "$goodix_src" ]]; then
        safe_copy "$goodix_src" "$PROJECT_DIR/vendor/samsung/kernel-source/input/touchscreen/goodix"
    fi

    # FocalTech
    local fts_src=$(find "$src" -maxdepth 4 -type d -name "*fts*" -path "*/touchscreen/*" 2>/dev/null | head -1) || true
    if [[ -n "$fts_src" ]]; then
        safe_copy "$fts_src" "$PROJECT_DIR/vendor/samsung/kernel-source/input/touchscreen/focaltech"
    fi
}

# ── Extract Sensor Hub Driver ───────────────────────────────────────────
extract_sensors() {
    log "Extracting sensor hub driver..."
    local src="$1"

    local shub_src=$(find "$src" -maxdepth 4 -type d -name "sensorhub" -o -name "shub" 2>/dev/null | head -1) || true

    if [[ -n "$shub_src" ]]; then
        safe_copy "$shub_src" "$PROJECT_DIR/vendor/samsung/kernel-source/sensors/shub"
    else
        warn "Sensor hub driver not found"
        warn "Check: $src/drivers/sensorhub/"
    fi
}

# ── Extract Battery/Charger Driver ──────────────────────────────────────
extract_battery() {
    log "Extracting battery/charger drivers..."
    local src="$1"

    local batt_src=$(find "$src" -maxdepth 4 -type d -name "battery" -path "*/power/*" 2>/dev/null | head -1) || true
    if [[ -n "$batt_src" ]]; then
        safe_copy "$batt_src" "$PROJECT_DIR/vendor/samsung/kernel-source/battery"
    fi

    # Charger ICs
    local charger_src=$(find "$src" -maxdepth 4 -name "*s2mf301*charger*" -o -name "*hl7132*" 2>/dev/null | head -5) || true
    if [[ -n "$charger_src" ]]; then
        local dst="$PROJECT_DIR/vendor/samsung/kernel-source/battery/charger"
        mkdir -p "$dst"
        for f in $charger_src; do
            cp "$f" "$dst/"
            log "  Charger: $(basename "$f")"
        done
    fi
}

# ── Extract Fingerprint Driver ──────────────────────────────────────────
extract_fingerprint() {
    log "Extracting fingerprint driver..."
    local src="$1"

    local fp_src=$(find "$src" -maxdepth 4 -type d -name "egistec" -o -name "*etspis*" -o -name "fingerprint" 2>/dev/null | head -1) || true

    if [[ -n "$fp_src" ]]; then
        safe_copy "$fp_src" "$PROJECT_DIR/vendor/samsung/kernel-source/fingerprint/egistec"
    else
        warn "Fingerprint driver not found"
        warn "Check: $src/drivers/input/fingerprint/"
    fi
}

# ── Extract NFC Driver ──────────────────────────────────────────────────
extract_nfc() {
    log "Extracting NFC driver..."
    local src="$1"

    local nfc_src=$(find "$src" -maxdepth 4 -type d -name "nfc" -o -name "nfcon" 2>/dev/null | head -1) || true

    if [[ -n "$nfc_src" ]]; then
        safe_copy "$nfc_src" "$PROJECT_DIR/vendor/samsung/kernel-source/nfc"
    else
        warn "NFC driver not found"
        warn "Check: $src/drivers/nfc/"
    fi
}

# ── Main ────────────────────────────────────────────────────────────────
main() {
    echo ""
    echo -e "${BOLD}╔══════════════════════════════════════════════╗${RESET}"
    echo -e "${BOLD}║   Vendor Module Extractor                     ║${RESET}"
    echo -e "${BOLD}╚══════════════════════════════════════════════╝${RESET}"
    echo ""

    # Check if Samsung source exists
    if [[ ! -d "$SAMSUNG_DIR" ]]; then
        err "Samsung kernel source not found at: $SAMSUNG_DIR"
        err "Run: make samsung-source"
        exit 1
    fi

    # Find kernel source
    local kernel_src
    if ! kernel_src=$(find_samsung_kernel); then
        err "Could not find kernel source in: $SAMSUNG_DIR"
        err "Run: make samsung-source"
        exit 1
    fi

    log "Using kernel source: $kernel_src"
    echo ""

    # Extract all modules
    extract_clock "$kernel_src"
    extract_display "$kernel_src"
    extract_gpu "$kernel_src"
    extract_mfc "$kernel_src"
    extract_audio "$kernel_src"
    extract_wifi "$kernel_src"
    extract_modem "$kernel_src"
    extract_touch "$kernel_src"
    extract_sensors "$kernel_src"
    extract_battery "$kernel_src"
    extract_fingerprint "$kernel_src"
    extract_nfc "$kernel_src"

    echo ""
    ok "Module extraction complete!"
    echo ""
    echo "  Extracted sources are in: $SAMSUNG_DIR/"
    echo ""
    echo "  Next steps:"
    echo "    1. Review extracted drivers"
    echo "    2. Port API changes from 5.10 → 6.18"
    echo "    3. Copy to vendor/samsung/ subdirectories"
    echo "    4. Update BUILD.bazel files with actual source paths"
    echo "    5. Build: make build"
    echo ""
}

main "$@"
