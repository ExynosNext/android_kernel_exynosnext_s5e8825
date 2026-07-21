### AnyKernel3 Ramdisk Mod Script
## osm0sis @ xda-developers

### AnyKernel setup
# global properties
properties() { '
kernel.string=ExynosNext GKI 6.18 for Exynos 1280 devices
do.devicecheck=1
do.modules=0
do.systemless=0
do.cleanup=1
do.cleanuponabort=0
device.name1=a25x
device.name2=a53x
device.name3=a33x
device.name4=m33x
device.name5=m34x
device.name6=gta4xls
device.name7=gta4xlswifi
device.name8=f34x
device.name9=a26xs
supported.versions=12.0-16.0
supported.patchlevels=
supported.vendorpatchlevels=
'; } # end properties


### AnyKernel install
## boot files attributes
boot_attributes() {
set_perm_recursive 0 0 755 644 $RAMDISK/*;
set_perm_recursive 0 0 750 750 $RAMDISK/init* $RAMDISK/sbin;
} # end attributes

# boot shell variables
BLOCK=/dev/block/by-name/boot;
IS_SLOT_DEVICE=0;
RAMDISK_COMPRESSION=auto;
PATCH_VBMETA_FLAG=auto;

# import functions/variables and setup patching - see for reference (DO NOT REMOVE)
. tools/ak3-core.sh;

# ── ExynosNext feature detection and patching ──────────────────────────────

printed_blank=0
print_blank_once() {
  if [ "$printed_blank" -eq 0 ]; then
    ui_print " "
    printed_blank=1
  fi
}
log_rom()  { print_blank_once; ui_print "[ROM] $1"; }
log_feat() { print_blank_once; ui_print "[EN]  $1"; }
log_warn() { print_blank_once; ui_print "[!]   $1"; }

# ── AOSP vs OneUI detection ─────────────────────────────────────────────────
# ExynosNext ships two ROM compatibility variants:
#   - vanilla / ksu        → AOSP-compatible (hexpatch aosp_mode=1)
#   - vanilla-oneui / ksu-oneui → OneUI-compatible (no patch needed)
# The variant is embedded in the kernel.string property above.
detect_aosp_mode() {
  if [ "$cache_mounted" -eq 1 ] && [ -f /cache/exynosnext_feat ] && \
     grep -q "aosp_mode=" /cache/exynosnext_feat 2>/dev/null; then
    val=$(grep -o 'aosp_mode=[0-9]*' /cache/exynosnext_feat | head -n1 | cut -d= -f2)
    log_rom "Vendor type: override (aosp_mode=$val)"
    apply_aosp_mode "$val"
    return 0
  fi

  if ! grep -q ' /vendor ' /proc/mounts 2>/dev/null; then
    mount -o ro /vendor 2>/dev/null || mount -o ro /dev/block/mapper/vendor /vendor 2>/dev/null
  fi

  vendor_src=$(grep ' /vendor ' /proc/mounts 2>/dev/null | tail -n1 | awk '{print $1}')
  case "$vendor_src" in
    /dev/block/*) ;;
    *)
      log_rom "Vendor type: OneUI or stock-based (vendor not block-mounted)"
      return 0
      ;;
  esac

  if [ -d /vendor/overlay/ConnectivityOverlay ] || [ -d /vendor/overlay/TetheringOverlay ]; then
    log_rom "Vendor type: OneUI or stock-based"
  else
    log_rom "Vendor type: AOSP"
    apply_aosp_mode 1
    if [ $? -eq 0 ]; then
      log_feat "aosp_mode: patched (0 -> 1)"
    else
      log_warn "aosp_mode: hex patch failed! Aborting installation."
      exit 1
    fi
  fi
}

apply_aosp_mode() {
  local mode=$1
  local hex_0="616f73705f6d6f64653d30"
  local hex_1="616f73705f6d6f64653d31"

  [ "$mode" = "1" ] || return 0
  [ -f "$AKHOME/Image" ] || return 0

  $BIN/magiskboot hexpatch "$AKHOME/Image" "$hex_0" "$hex_1" >/dev/null 2>&1
}

# ── Check if /cache is mounted, try to mount if not ─────────────────────────
cache_mounted=0;
if mountpoint -q /cache 2>/dev/null; then
  cache_mounted=1;
else
  if mount /cache 2>/dev/null; then
    cache_mounted=1;
  fi
fi

# ── Detect and apply feature flags from /cache/exynosnext_feat ──────────────
# Supported flags (one per line):
#   aosp_mode=0|1          — force AOSP/OneUI mode
#   force_perm=1           — enable permissive SELinux at boot
#   ems_efficient=1        — enable EMS efficient mode
if [ "$cache_mounted" -eq 1 ] && [ -f /cache/exynosnext_feat ]; then
  if grep -q "force_perm" /cache/exynosnext_feat 2>/dev/null; then
    log_feat "Permissive mode: enabled"
    hex_0="666f7263655f7065726d3d30"
    hex_1="666f7263655f7065726d3d31"
    [ -f "$AKHOME/Image" ] && $BIN/magiskboot hexpatch "$AKHOME/Image" "$hex_0" "$hex_1" >/dev/null 2>&1
    if [ $? -eq 0 ]; then
      log_feat "force_perm: patched (0 -> 1)"
    else
      log_warn "force_perm: hex patch failed! Aborting installation."
      exit 1
    fi
  fi

  if grep -q "ems_efficient" /cache/exynosnext_feat 2>/dev/null; then
    log_feat "EMS efficient mode: enabled"
    hex_0="656d735f656666696369656e743d30"
    hex_1="656d735f656666696369656e743d31"
    [ -f "$AKHOME/Image" ] && $BIN/magiskboot hexpatch "$AKHOME/Image" "$hex_0" "$hex_1" >/dev/null 2>&1
    if [ $? -eq 0 ]; then
      log_feat "ems_efficient: patched (0 -> 1)"
    else
      log_warn "ems_efficient: hex patch failed! Aborting installation."
      exit 1
    fi
  fi
fi

# ── Detect ROM type and patch aosp_mode accordingly ────────────────────────
detect_aosp_mode

# ── boot install ───────────────────────────────────────────────────────────
# AnyKernel3 extracts the device's current boot.img, replaces only the kernel
# (Image) and dtb, then repacks — preserving the device's ramdisk/init.
dump_boot;

write_boot;
## end boot install

# ── vendor_boot install (for GKI modules) ──────────────────────────────────
flash_generic vendor_boot;
