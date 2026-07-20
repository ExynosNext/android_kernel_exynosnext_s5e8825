#!/usr/bin/env bash
# ============================================================================
# Samsung / Exynos 1280 reference source fetcher
#
# For PORTING work you want a real, git-cloneable Linux 5.10 tree for the
# s5e8825. The community mirrors below are the practical choice; Samsung's
# official portal ships a pristine tarball but is search/interaction gated and
# does not expose stable direct-download URLs, so it can't be scripted reliably.
#
# This fetches a reference tree into vendor/samsung/kernel-source/ so
# extract-modules.sh and manual porting can work against it.
# ============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[0;33m'
CYAN='\033[0;36m'; BOLD='\033[1m'; RESET='\033[0m'
log()  { echo -e "${CYAN}[SAMSUNG]${RESET} $*"; }
ok()   { echo -e "${GREEN}[OK]${RESET} $*"; }
warn() { echo -e "${YELLOW}[WARN]${RESET} $*"; }
err()  { echo -e "${RED}[ERROR]${RESET} $*" >&2; }

SAMSUNG_DIR="$PROJECT_DIR/vendor/samsung/kernel-source"
mkdir -p "$SAMSUNG_DIR"

# Reference trees: name -> "git_url|branch". These are Linux 5.10.x s5e8825
# kernels used by working custom kernels for the Exynos 1280 family.
declare -A REFS=(
    ["floppy"]="https://github.com/FlopKernel-Series/flop_s5e8825_kernel|master"
    ["un1ca"]="https://github.com/UN1CA/kernel_samsung_s5e8825|main"
)

echo ""
echo -e "${BOLD}Exynos 1280 (s5e8825) reference source fetcher${RESET}"
echo ""
echo "  1) FloppyKernel  — Linux 5.10.260, all s5e8825 devices (recommended)"
echo "  2) UN1CA         — Linux 5.10.x, s5e8825"
echo "  3) Official Samsung portal (manual instructions only)"
echo "  0) Exit"
echo ""
read -rp "  Select [0-3]: " choice

clone_ref() {
    local key="$1" entry url branch dest
    entry="${REFS[$key]}"
    url="${entry%%|*}"; branch="${entry##*|}"
    dest="$SAMSUNG_DIR/$key"

    if [[ -d "$dest/.git" ]]; then
        log "Updating existing clone at $dest"
        git -C "$dest" fetch --depth=1 origin "$branch" && \
            git -C "$dest" reset --hard "origin/$branch"
    else
        log "Cloning $url ($branch) — this is large, shallow clone used"
        git clone --depth=1 --branch "$branch" "$url" "$dest"
    fi
    ln -sfn "$dest" "$SAMSUNG_DIR/kernel"
    ok "Reference ready: $dest"
    ok "Symlinked: $SAMSUNG_DIR/kernel -> $dest"
    echo ""
    echo "  Next: ./scripts/extract-modules.sh   (pulls drivers into place)"
    echo "        then port each driver 5.10 -> 6.18 (see docs/PORTING.md)"
}

official_portal() {
    cat <<'EOF'

  Samsung official source (pristine, per-device tarball):

    1. Open  https://opensource.samsung.com/uploadList?menuItem=mobile
    2. Search the device model, e.g.  SM-A536B  (A53), SM-A256E (A25),
       SM-A336B (A33), SM-M336B (M33), SM-M346B (M34).
    3. Download the matching "Kernel" open-source package (requires the
       portal's interactive flow; there is no stable direct URL).
    4. Unzip it and unpack the inner Kernel.tar.gz into:
         vendor/samsung/kernel-source/<model>/
    5. Point the symlink at it:
         ln -sfn vendor/samsung/kernel-source/<model> \
                 vendor/samsung/kernel-source/kernel

  Note: this tarball is Linux 5.10.x. It is the authoritative CAL-IF source to
  transcribe register/PLL data from, but the git mirrors above are easier to
  diff against while porting.

EOF
}

case "$choice" in
    1) clone_ref floppy ;;
    2) clone_ref un1ca ;;
    3) official_portal ;;
    0) exit 0 ;;
    *) err "Invalid choice"; exit 1 ;;
esac
