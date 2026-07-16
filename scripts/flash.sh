#!/usr/bin/env bash
# ============================================================================
# ExynosNext Interactive Flash Tool
# Professional graphical flash interface for ExynosNext kernel
# ============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
DISTDIR="$PROJECT_DIR/out/dist"

# ── Colors ──────────────────────────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
WHITE='\033[1;37m'
DIM='\033[2m'
BOLD='\033[1m'
RESET='\033[0m'

# ── Unicode Box Drawing ─────────────────────────────────────────────────────
TL="┌" TR="┐" BL="└" BR="┘" H="─" V="│" DH="═"
LJ="├" RJ="┤" TJ="┬" BJ="┴" CROSS="┼"

# ── Helpers ─────────────────────────────────────────────────────────────────
W=60

print_border_top()    { printf "${1}${TL}"; for ((i=0;i<W-2;i++)); do printf "${DH}"; done; printf "${TR}${RESET}\n"; }
print_border_bottom() { printf "${1}${BL}"; for ((i=0;i<W-2;i++)); do printf "${DH}"; done; printf "${BR}${RESET}\n"; }
print_border_mid()    { printf "${1}${LJ}"; for ((i=0;i<W-2;i++)); do printf "${DH}"; done; printf "${RJ}${RESET}\n"; }

print_centered() {
    local text="$1" color="${2:-$WHITE}"
    local stripped; stripped=$(echo -e "$text" | sed 's/\x1b\[[0-9;]*m//g')
    local len=${#stripped}
    local pad=$(( (W - len - 2) / 2 ))
    local padr=$(( W - len - 2 - pad ))
    printf "${color}${V} ${text}"
    printf "%${padr}s${V}${RESET}\n"
}

print_kv() {
    local key="$1" val="$2" kc="${3:-$CYAN}" vc="${4:-$WHITE}"
    local ks; ks=$(echo -e "$key" | sed 's/\x1b\[[0-9;]*m//g')
    local vs; vs=$(echo -e "$val" | sed 's/\x1b\[[0-9;]*m//g')
    local total=$(( ${#ks} + ${#vs} + 1 ))
    local pad=$(( W - total - 2 ))
    printf "${vc}${V} ${kc}${key}${RESET} ${vc}${val}"
    printf "%${pad}s ${vc}${V}${RESET}\n"
}

clear_screen() { clear 2>/dev/null || printf '\033[2J\033[H'; }

# ── Detection ───────────────────────────────────────────────────────────────
detect_adb() {
    ADB=""
    if command -v adb &>/dev/null; then ADB="adb"; fi
}

detect_fastboot() {
    FASTBOOT=""
    if command -v fastboot &>/dev/null; then FASTBOOT="fastboot"; fi
}

detect_device() {
    DEVICE_CONNECTED=0
    DEVICE_SERIAL=""
    if [[ -n "$ADB" ]]; then
        local out
        out=$($ADB devices 2>/dev/null | grep -w "device" | head -1) || true
        if [[ -n "$out" ]]; then
            DEVICE_CONNECTED=1
            DEVICE_SERIAL=$(echo "$out" | awk '{print $1}')
        fi
    fi
}

get_device_info() {
    DEVICE_MODEL="N/A"
    DEVICE_ANDROID="N/A"
    DEVICE_KERNEL="N/A"
    DEVICE_ROM="Unknown"
    DEVICE_ROOT="None"
    if [[ "$DEVICE_CONNECTED" -eq 1 ]]; then
        DEVICE_MODEL=$($ADB -s "$DEVICE_SERIAL" shell getprop ro.product.model 2>/dev/null | tr -d '\r') || true
        DEVICE_ANDROID=$($ADB -s "$DEVICE_SERIAL" shell getprop ro.build.version.release 2>/dev/null | tr -d '\r') || true
        DEVICE_KERNEL=$($ADB -s "$DEVICE_SERIAL" shell uname -r 2>/dev/null | tr -d '\r') || true

        local fp
        fp=$($ADB -s "$DEVICE_SERIAL" shell getprop ro.build.fingerprint 2>/dev/null | tr -d '\r') || true
        if echo "$fp" | grep -qi "one ui\|samsung"; then
            DEVICE_ROM="${GREEN}OneUI${RESET}"
        elif echo "$fp" | grep -qi "lineage\|pixel\|crdroid\|evolution"; then
            DEVICE_ROM="${MAGENTA}AOSP${RESET}"
        else
            DEVICE_ROM="${YELLOW}Custom${RESET}"
        fi

        local su
        su=$($ADB -s "$DEVICE_SERIAL" shell which ksud 2>/dev/null | tr -d '\r') || true
        if [[ -n "$su" ]] && [[ "$su" != "which: not found" ]]; then
            local ver
            ver=$($ADB -s "$DEVICE_SERIAL" shell ksud --version 2>/dev/null | head -1 | tr -d '\r') || true
            if echo "$ver" | grep -qi "sukisu"; then
                DEVICE_ROOT="${MAGENTA}SukiSU${RESET}"
            else
                DEVICE_ROOT="${GREEN}KernelSU${RESET}"
            fi
        fi
    fi
}

check_build() {
    HAS_BOOT=0; HAS_VB=0; HAS_DTBO=0
    [[ -f "$DISTDIR/boot.img" ]] && HAS_BOOT=1
    [[ -f "$DISTDIR/vendor_boot.img" ]] && HAS_VB=1
    [[ -f "$DISTDIR/dtbo.img" ]] && HAS_DTBO=1
}

# ── Flash Operations ────────────────────────────────────────────────────────
flash_file() {
    local partition="$1" file="$2" label="$3"
    if [[ ! -f "$file" ]]; then
        printf "  ${RED}${label} not found!${RESET}\n"
        return 1
    fi
    printf "\n  ${CYAN}Flashing ${label} ($(du -h "$file" | cut -f1))...${RESET}\n"
    if $FASTBOOT flash "$partition" "$file" 2>&1 | while IFS= read -r l; do printf "    %s\n" "$l"; done; then
        printf "  ${GREEN}${label} flashed!${RESET}\n"
        return 0
    else
        printf "  ${RED}Failed to flash ${label}!${RESET}\n"
        return 1
    fi
}

# ── UI ──────────────────────────────────────────────────────────────────────
show_header() {
    clear_screen
    printf "\n"
    print_border_top "$CYAN"
    printf "${CYAN}${V}${RESET}                                                            ${CYAN}${V}${RESET}\n"
    printf "${CYAN}${V}${RESET}  ${WHITE}${BOLD}██████╗ ███████╗██╗   ██╗███████╗███████╗███████╗${RESET}  ${CYAN}${V}${RESET}\n"
    printf "${CYAN}${V}${RESET}  ${WHITE}${BOLD}██╔══██╗██╔════╝╚██╗ ██╔╝██╔════╝██╔════╝██╔════╝${RESET}  ${CYAN}${V}${RESET}\n"
    printf "${CYAN}${V}${RESET}  ${WHITE}${BOLD}██████╔╝█████╗   ╚████╔╝ █████╗  ███████╗███████╗${RESET}  ${CYAN}${V}${RESET}\n"
    printf "${CYAN}${V}${RESET}  ${WHITE}${BOLD}██╔══██╗██╔══╝    ╚██╔╝  ██╔══╝  ╚════██║╚════██║${RESET}  ${CYAN}${V}${RESET}\n"
    printf "${CYAN}${V}${RESET}  ${WHITE}${BOLD}██║  ██║███████╗   ██║   ███████╗███████║███████║${RESET}  ${CYAN}${V}${RESET}\n"
    printf "${CYAN}${V}${RESET}  ${WHITE}${BOLD}╚═╝  ╚═╝╚══════╝   ╚═╝   ╚══════╝╚══════╝╚══════╝${RESET}  ${CYAN}${V}${RESET}\n"
    printf "${CYAN}${V}${RESET}                                                            ${CYAN}${V}${RESET}\n"
    print_centered "${DIM}Linux 6.18 LTS · Exynos 1280 · Interactive Flash Tool${RESET}" "$CYAN"
    print_border_bottom "$CYAN"
    printf "\n"
}

show_device_info() {
    print_border_top "$GREEN"
    print_centered "${GREEN}${BOLD}Device Information${RESET}" "$GREEN"
    print_border_mid "$GREEN"
    if [[ "$DEVICE_CONNECTED" -eq 1 ]]; then
        print_kv "  Model:" "$DEVICE_MODEL"
        print_kv "  Serial:" "${DIM}${DEVICE_SERIAL}${RESET}"
        print_kv "  Android:" "$DEVICE_ANDROID"
        print_kv "  Kernel:" "${DIM}${DEVICE_KERNEL}${RESET}"
    else
        print_centered "${RED}No device connected via ADB${RESET}" "$GREEN"
    fi
    print_border_mid "$GREEN"
    print_kv "  ROM:" "$DEVICE_ROM"
    print_kv "  Root:" "$DEVICE_ROOT"
    print_kv "  ADB:" "$([ -n "$ADB" ] && echo "${GREEN}OK${RESET}" || echo "${RED}Missing${RESET}")"
    print_kv "  fastboot:" "$([ -n "$FASTBOOT" ] && echo "${GREEN}OK${RESET}" || echo "${RED}Missing${RESET}")"
    print_border_bottom "$GREEN"
}

show_build_info() {
    print_border_top "$BLUE"
    print_centered "${BLUE}${BOLD}Build Artifacts${RESET}" "$BLUE"
    print_border_mid "$BLUE"
    for f in boot.img vendor_boot.img dtbo.img; do
        local path="$DISTDIR/$f"
        local status
        if [[ -f "$path" ]]; then
            status="${GREEN}OK${RESET} ($(du -h "$path" | cut -f1))"
        else
            status="${RED}MISSING${RESET}"
        fi
        print_kv "  $f:" "$status"
    done
    print_border_bottom "$BLUE"
}

show_menu() {
    print_border_top "$CYAN"
    print_centered "${CYAN}${BOLD}Flash Options${RESET}" "$CYAN"
    print_border_mid "$CYAN"
    printf "${CYAN}${V}${RESET}                                                            ${CYAN}${V}${RESET}\n"
    printf "${CYAN}${V}${RESET}  ${WHITE}${BOLD}1)${RESET} Flash kernel only        ${DIM}(boot.img)${RESET}            ${CYAN}${V}${RESET}\n"
    printf "${CYAN}${V}${RESET}  ${WHITE}${BOLD}2)${RESET} Flash kernel + modules   ${DIM}(boot + vendor_boot)${RESET}   ${CYAN}${V}${RESET}\n"
    printf "${CYAN}${V}${RESET}  ${WHITE}${BOLD}3)${RESET} Full flash               ${DIM}(boot + vb + dtbo)${RESET}     ${CYAN}${V}${RESET}\n"
    printf "${CYAN}${V}${RESET}  ${WHITE}${BOLD}4)${RESET} Reboot device                                     ${CYAN}${V}${RESET}\n"
    printf "${CYAN}${V}${RESET}  ${WHITE}${BOLD}0)${RESET} Exit                                                 ${CYAN}${V}${RESET}\n"
    printf "${CYAN}${V}${RESET}                                                            ${CYAN}${V}${RESET}\n"
    print_border_bottom "$CYAN"
}

# ── Main ────────────────────────────────────────────────────────────────────
main() {
    while true; do
        detect_adb
        detect_fastboot
        detect_device
        get_device_info
        check_build

        show_header
        show_device_info
        show_build_info
        show_menu

        printf "\n"
        read -rp "  ${WHITE}Select [0-4]: ${RESET}" choice

        case "$choice" in
            1)
                [[ "$HAS_BOOT" -ne 1 ]] && { printf "  ${RED}boot.img missing! Build kernel first.${RESET}\n"; sleep 2; continue; }
                [[ "$DEVICE_CONNECTED" -ne 1 ]] && { printf "  ${RED}No device connected!${RESET}\n"; sleep 2; continue; }
                printf "\n  ${BOLD}Flashing boot.img ...${RESET}\n"
                read -rp "  ${YELLOW}Proceed? [y/N]: ${RESET}" c
                [[ "$c" =~ ^[Yy]$ ]] && flash_file boot "$DISTDIR/boot.img" "boot.img"
                read -rp "  ${DIM}Enter to continue...${RESET}" _
                ;;
            2)
                [[ "$HAS_BOOT" -ne 1 || "$HAS_VB" -ne 1 ]] && { printf "  ${RED}Artifacts missing!${RESET}\n"; sleep 2; continue; }
                [[ "$DEVICE_CONNECTED" -ne 1 ]] && { printf "  ${RED}No device connected!${RESET}\n"; sleep 2; continue; }
                printf "\n  ${BOLD}Flashing boot + vendor_boot ...${RESET}\n"
                read -rp "  ${YELLOW}Proceed? [y/N]: ${RESET}" c
                if [[ "$c" =~ ^[Yy]$ ]]; then
                    flash_file boot "$DISTDIR/boot.img" "boot.img"
                    flash_file vendor_boot "$DISTDIR/vendor_boot.img" "vendor_boot.img"
                fi
                read -rp "  ${DIM}Enter to continue...${RESET}" _
                ;;
            3)
                [[ "$HAS_BOOT" -ne 1 || "$HAS_VB" -ne 1 ]] && { printf "  ${RED}Artifacts missing!${RESET}\n"; sleep 2; continue; }
                [[ "$DEVICE_CONNECTED" -ne 1 ]] && { printf "  ${RED}No device connected!${RESET}\n"; sleep 2; continue; }
                printf "\n  ${BOLD}Full flash: boot + vendor_boot + dtbo ...${RESET}\n"
                read -rp "  ${YELLOW}This will overwrite kernel, modules, and DT. Continue? [y/N]: ${RESET}" c
                if [[ "$c" =~ ^[Yy]$ ]]; then
                    flash_file boot "$DISTDIR/boot.img" "boot.img"
                    flash_file vendor_boot "$DISTDIR/vendor_boot.img" "vendor_boot.img"
                    if [[ "$HAS_DTBO" -eq 1 ]]; then
                        flash_file dtbo "$DISTDIR/dtbo.img" "dtbo.img"
                    else
                        printf "  ${YELLOW}dtbo.img not found, skipping.${RESET}\n"
                    fi
                    printf "\n  ${CYAN}Rebooting...${RESET}\n"
                    $FASTBOOT reboot 2>/dev/null || $ADB reboot 2>/dev/null || true
                    break
                fi
                read -rp "  ${DIM}Enter to continue...${RESET}" _
                ;;
            4)
                if [[ "$DEVICE_CONNECTED" -eq 1 ]]; then
                    printf "  ${CYAN}Rebooting...${RESET}\n"
                    $ADB reboot 2>/dev/null || $FASTBOOT reboot 2>/dev/null || true
                else
                    printf "  ${RED}No device connected!${RESET}\n"
                fi
                sleep 1
                ;;
            0)
                printf "\n  ${GREEN}Goodbye!${RESET}\n\n"
                exit 0
                ;;
            *)
                printf "  ${RED}Invalid.${RESET}\n"; sleep 1
                ;;
        esac
    done
}

main "$@"
