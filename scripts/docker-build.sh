#!/usr/bin/env bash
# ============================================================================
# ExynosNext Docker Build Wrapper
# Build the kernel using Docker containers
# ============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_DIR"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
RESET='\033[0m'

log()  { echo -e "${CYAN}[DOCKER]${RESET} $*"; }
ok()   { echo -e "${GREEN}[OK]${RESET} $*"; }
warn() { echo -e "${YELLOW}[WARN]${RESET} $*"; }
err()  { echo -e "${RED}[ERROR]${RESET} $*" >&2; }

# ── Detect docker compose command ───────────────────────────────────────
if docker compose version &>/dev/null 2>&1; then
    COMPOSE="docker compose"
elif docker-compose version &>/dev/null 2>&1; then
    COMPOSE="docker-compose"
else
    err "Docker Compose not found. Install Docker Desktop or docker-compose."
    exit 1
fi

# ── Check Docker is running ─────────────────────────────────────────────
if ! docker info &>/dev/null 2>&1; then
    err "Docker is not running. Start Docker Desktop and try again."
    exit 1
fi

# ── Parse args ──────────────────────────────────────────────────────────
ACTION="${1:-help}"
VARIANT="${VARIANT:-ksu}"
JOBS="${JOBS:-$(nproc --all 2>/dev/null || echo 8)}"

show_help() {
    echo ""
    echo -e "${BOLD}╔══════════════════════════════════════════════╗${RESET}"
    echo -e "${BOLD}║   ExynosNext Docker Build System              ║${RESET}"
    echo -e "${BOLD}╚══════════════════════════════════════════════╝${RESET}"
    echo ""
    echo "  Usage: $0 <command> [options]"
    echo ""
    echo "  Commands:"
    echo "    build          Build kernel in Docker (default: KSU variant)"
    echo "    build-vanilla  Build without root"
    echo "    build-sukisu   Build with SukiSU Ultra"
    echo "    setup          One-time environment setup (install deps, sync ACK)"
    echo "    shell          Open interactive shell in build container"
    echo "    sync           Sync ACK sources only"
    echo "    clean          Clean build artifacts"
    echo "    flash          Flash kernel to device (requires USB passthrough)"
    echo "    help           Show this help"
    echo ""
    echo "  Environment Variables:"
    echo "    JOBS=8         Parallel build jobs (default: nproc)"
    echo "    VARIANT=ksu    Build variant: ksu, vanilla, suksu"
    echo ""
    echo "  Examples:"
    echo "    $0 build              # Build with KernelSU"
    echo "    $0 build-vanilla      # Build without root"
    echo "    $0 setup              # First-time setup"
    echo "    JOBS=16 $0 build      # Build with 16 parallel jobs"
    echo "    $0 shell              # Interactive shell for debugging"
    echo ""
}

# ── Commands ────────────────────────────────────────────────────────────

do_setup() {
    log "Running first-time setup..."
    $COMPOSE run --rm setup
    ok "Setup complete"
}

do_build() {
    local variant="${1:-ksu}"
    log "Building kernel ($variant variant) with $JOBS jobs..."
    $COMPOSE run --rm -e JOBS="$JOBS" -e VARIANT="$variant" build \
        bash -c "./scripts/build.sh --${variant} --jobs=${JOBS}"
    ok "Build complete! Output in: out/dist/"
}

do_shell() {
    log "Opening interactive shell..."
    $COMPOSE run --rm build bash
}

do_sync() {
    log "Syncing ACK android17-6.18..."
    $COMPOSE run --rm setup \
        bash -c "
            repo init -u https://android.googlesource.com/kernel/manifest \
                -b common-android17-6.18 --depth=1 && \
            repo sync -j${JOBS} --no-tags
        "
    ok "Sync complete"
}

do_clean() {
    log "Cleaning build artifacts..."
    rm -rf out/
    ok "Clean complete"
}

do_flash() {
    log "Starting flash tool..."
    $COMPOSE run --rm --privileged flash
}

# ── Main ────────────────────────────────────────────────────────────────

case "$ACTION" in
    build)         do_build "ksu" ;;
    build-vanilla) do_build "vanilla" ;;
    build-sukisu)  do_build "sukisu" ;;
    setup)         do_setup ;;
    shell)         do_shell ;;
    sync)          do_sync ;;
    clean)         do_clean ;;
    flash)         do_flash ;;
    help|-h|--help) show_help ;;
    *)
        err "Unknown command: $ACTION"
        show_help
        exit 1
        ;;
esac
