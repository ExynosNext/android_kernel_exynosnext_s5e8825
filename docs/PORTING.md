# ExynosNext Porting Tracker

## Samsung Exynos 1280 Vendor Module Porting: 5.10 → 6.18

**Last Updated:** 2026-07-16

---

## Legend

| Symbol | Meaning |
|--------|---------|
| ✅ | Ported (complete) |
| 🔄 | In Progress |
| ⏳ | Not Started |
| ⚠️ | Blocked |
| ❌ | Removed (not needed) |

---

## Module Dependency Order

Modules must be ported in this order due to hardware and software dependencies. Blocking dependencies mean the upstream module **must** compile and load before downstream work can proceed.

| # | Module | Description | Blocks |
|---|--------|-------------|--------|
| 1 | **Clock Controller** | Clock tree and gate control | Everything |
| 2 | **Power Domains** | Power domain management | GPU, Display, Audio, Camera, MFC, Modem |
| 3 | **Pinctrl/GPIO** | Pin multiplexing and GPIO | I2C, SPI, UART, MMC |
| 4 | **UFS** | Storage controller | Must boot first |
| 5 | **Display** | DECON → DSIM → Panel | — |
| 6 | **Touchscreen** | Input touchscreen | Needs I2C |
| 7 | **Audio** | I2S/TDM → ABOX → Codecs | — |
| 8 | **WiFi** | SCSC wireless | — |
| 9 | **Modem** | CPIF modem interface | — |
| 10 | **Battery/Charger** | Power supply | — |
| 11 | **Sensors** | Sensor hub | — |
| 12 | **Fingerprint** | Fingerprint reader | — |
| 13 | **GPU** | Mali GPU | Most complex |
| 14 | **MFC** | Multi-Format Codec | — |
| 15 | **Camera** | Camera ISP pipeline | Most complex |

**Minimum boot chain:** Clock Controller → Power Domains → Pinctrl → UFS → Display (at least panel init for framebuffer)

---

## Detailed Module Tracking

---

### 1. Clock Controller

| Field | Value |
|-------|-------|
| **Module** | `clk-s5e8825.ko` |
| **Source** | `drivers/clk/samsung/clk-s5e8825.c` |
| **Target** | `vendor/samsung/clock/clk-s5e8825.c` |
| **Status** | 🔄 In Progress |
| **Difficulty** | Hard |

**Porting Notes:**
- Samsung uses a proprietary CAL-IF (Clock Abstraction Layer Interface) framework in their 5.10 kernel that wraps the actual clock register programming.
- Must rewrite to use standard `samsung_clk_register_*()` API from `drivers/clk/samsung/` in mainline.
- Requires extracting the actual PLL configuration values, dividers, and gate registers from the Samsung source.
- The DT clock bindings must match the `samsung,exynos-clk` compatible string format.
- Known sub-clocks: MUX, DIV, GATE, PLL (PLL20xx/PLL23xx type).
- Needs a `clk-s5e8825.h` header defining all clock IDs as an enum.

**Checklist:**
- [ ] Extract PLL register maps and configuration from Samsung source
- [ ] Create clock ID enum header (`clk-s5e8825.h`)
- [ ] Map CAL-IF calls to `samsung_clk_register_mux()` / `_div()` / `_gate()` / `_plln()`
- [ ] Write DT bindings for clock controller node
- [ ] Define fixed-rate clocks (XUSBXTI, etc.)
- [ ] Test all PLL locks and clock gate toggling
- [ ] Verify clock rates against Samsung firmware logs

---

### 2. Display DPU (DECON)

| Field | Value |
|-------|-------|
| **Module** | `samsung_dpu.ko` |
| **Source** | `drivers/gpu/drm/samsung/decon/` |
| **Target** | `vendor/samsung/display/dpu/` |
| **Status** | ⏳ Not Started |
| **Difficulty** | Very Hard |

**Porting Notes:**
- Depends on: Clock Controller, Power Domains (display PD)
- Samsung DECON (Display Engine Controller) is highly customized and diverges from the upstream Exynos DRM driver significantly.
- Atomic commit path uses Samsung-specific state management.
- Requires rewriting the DECON register interface to fit the upstream `exynos_drm_driver` framework or maintaining as an out-of-tree DRM driver.
- Complex clock dependency chain: DECON pixel clock → MIPI DSI byte clock → panel clock.
- Samsung DECON supports multiple windows (planes) with per-window alpha blending.
- Must handle DECON init, partial update, and power sequencing.

**Checklist:**
- [ ] Identify DECON register offsets and field definitions
- [ ] Map to DRM plane/crtc/encoder architecture
- [ ] Port or rewrite atomic commit handler
- [ ] Port DECON interrupt handling (vsync, underrun, etc.)
- [ ] Integrate with samsung_clk for pixel clock
- [ ] Port power domain sequence (init/suspend/resume)
- [ ] DT bindings for DECON node
- [ ] Test with simpledrm or deferred probe to DSIM

---

### 3. Display DSIM (MIPI DSI)

| Field | Value |
|-------|-------|
| **Module** | `samsung_dsim.ko` |
| **Source** | `drivers/gpu/drm/samsung/dsim/` |
| **Target** | `vendor/samsung/display/dsim/` |
| **Status** | ⏳ Not Started |
| **Difficulty** | Hard |

**Porting Notes:**
- Depends on: Display DPU
- Samsung DSIM (Display Serial Interface) handles MIPI DSI command/video modes.
- Must support DSI bus initialization, LP/HS mode switching, and DCS commands.
- Samsung DSIM has proprietary retry and error recovery logic.
- Clock configuration: bit clock, byte clock, escape clock.

**Checklist:**
- [ ] Port DSIM register definitions
- [ ] Port DSI host operations (transfer, prepare, init)
- [ ] Port DSI PHY configuration
- [ ] Port clock configuration ( PLL for DSI bit clock)
- [ ] DT bindings for DSIM node
- [ ] Integration test with panel driver

---

### 4. Display Panel

| Field | Value |
|-------|-------|
| **Module** | `samsung_panel.ko` |
| **Source** | `drivers/gpu/drm/panel/` or `drivers/gpu/drm/samsung/panel/` |
| **Target** | `vendor/samsung/display/panel/` |
| **Status** | ⏳ Not Started |
| **Difficulty** | Medium |

**Porting Notes:**
- Depends on: Display DSIM
- Panel driver should implement `drm_panel_funcs` for `prepare`, `enable`, `disable`, `unprepare`.
- Samsung typically bundles panel-specific init sequences as large DCS command tables.
- Brightness control via DCS or I2C backlight IC.
- Panel may support DSI video mode or DSI command mode.

**Checklist:**
- [ ] Extract panel init sequence (DSC commands)
- [ ] Port to `drm_panel` framework
- [ ] Implement `backlight_device` integration
- [ ] Handle panel reset GPIO and power sequencing
- [ ] DT bindings for panel node (timing, resolution, DSI config)
- [ ] Test display output with test pattern

---

### 5. Mali GPU

| Field | Value |
|-------|-------|
| **Module** | `mali_kbase.ko` |
| **Source** | `drivers/gpu/arm/mali/` |
| **Target** | `vendor/samsung/gpu/mali/` |
| **Status** | ⏳ Not Started |
| **Difficulty** | Very Hard |

**Porting Notes:**
- Depends on: Power Domains (GPU PD), Clock Controller
- Proprietary ARM Midgard/Bifrost driver. Extremely heavy kernel API usage.
- Heavy use of vendor hooks, private DMA-buf, and custom allocator.
- DVFS integration with Samsung's thermal framework and power domain.
- Known to use `copy_from_user`/`copy_to_user` in ways that break with newer hardening.
- Job scheduling, memory management, and JIT are all kernel-internal.
- This is typically the **last** module ported due to complexity.
- Check ARM developer site for updated driver drops that target 6.x kernels.

**Checklist:**
- [ ] Obtain updated ARM Mali driver (r49p0 or later) that supports 6.x
- [ ] Audit kernel API usage against 6.18 changes
- [ ] Port DMA-buf and ion allocator interfaces
- [ ] Fix `vm_fault` / `vm_ops` changes
- [ ] Port DVFS/power domain integration
- [ ] Fix any `ioctl` hardening changes
- [ ] Build test and `glmark2-es2` basic rendering test
- [ ] Stress test with GPU compute workloads

---

### 6. MFC Codec (Multi-Format Codec)

| Field | Value |
|-------|-------|
| **Module** | `samsung_mfc.ko` |
| **Source** | `drivers/media/platform/samsung/mfc/` |
| **Target** | `vendor/samsung/media/mfc/` |
| **Status** | ⏳ Not Started |
| **Difficulty** | Hard |

**Porting Notes:**
- Depends on: Power Domains (MFC PD), Clock Controller
- Samsung MFC handles H.264/H.265/VP8/VP9 decode and encode.
- Uses `v4l2_m2m_device` and `v4l2_ioctl_ops` frameworks.
- Firmware loading from `/lib/firmware` (MFC FW binary).
- Complex buffer management with Samsung-specific DMA memory handling.
- Must support both kernel 5.10 `vb2` and 6.18 `vb2` API differences.

**Checklist:**
- [ ] Port MFC register interface
- [ ] Port V4L2 M2M device registration
- [ ] Port firmware loading mechanism
- [ ] Update `vb2` queue operations for 6.18 API
- [ ] Port DMA buffer allocation (ion → DMA-buf heaps)
- [ ] DT bindings for MFC node
- [ ] Test decode with `v4l2-ctl` and a sample H.264 stream

---

### 7. Audio ABOX (Samsung Audio DSP)

| Field | Value |
|-------|-------|
| **Module** | `samsung_abox.ko` |
| **Source** | `drivers/soc/samsung/abox/` |
| **Target** | `vendor/samsung/audio/abox/` |
| **Status** | ⏳ Not Started |
| **Difficulty** | Very Hard |

**Porting Notes:**
- Depends on: Clock Controller, Power Domains
- ABOX is Samsung's audio DSP co-processor. Complex firmware loading and IPC.
- Uses mailbox framework, DMA engine, and IOMMU for shared memory.
- Supports PCM, compress offload, and multi-channel I2S.
- ABOX firmware binary must be loaded to the DSP core.
- Deep integration with Samsung's IPC and memory-mapped I/O.
- Error recovery and watchdog handling is critical.

**Checklist:**
- [ ] Port ABOX mailbox/IPC interface
- [ ] Port firmware loading mechanism
- [ ] Port DMA and IOMMU integration
- [ ] Port ALSA/ASoC machine driver integration
- [ ] Port DSP register interface
- [ ] Test PCM playback/capture via ALSA
- [ ] Test compress offload (if applicable)

---

### 8. Audio Codecs

| Field | Value |
|-------|-------|
| **Module** | `rt5665.ko`, `rt5691.ko` |
| **Source** | `sound/soc/codecs/` |
| **Target** | `vendor/samsung/audio/codecs/` |
| **Status** | ⏳ Not Started |
| **Difficulty** | Easy |

**Porting Notes:**
- Mainline Linux already has `rt5665` and related Realtek codec drivers.
- May need minor Samsung-specific DAI link configuration or amplifier control.
- Likely minimal or no porting needed; verify mainline versions work.

**Checklist:**
- [ ] Verify mainline `rt5665` and `rt5691` driver compatibility
- [ ] Identify any Samsung-specific register overrides
- [ ] Test DAI link with ABOX

---

### 9. Audio Amplifiers

| Field | Value |
|-------|-------|
| **Module** | `tfa9878.ko`, `aw882xx.ko` |
| **Source** | `sound/soc/codecs/` |
| **Target** | `vendor/samsung/audio/amplifiers/` |
| **Status** | ⏳ Not Started |
| **Difficulty** | Easy |

**Porting Notes:**
- NXP TFA9878 and Awinic AW882xx are common speaker amplifiers.
- Mainline may already have `tfa987x` and `aw882xx` drivers.
- I2C-based, straightforward codec registration.

**Checklist:**
- [ ] Verify mainline amplifier driver compatibility
- [ ] Port I2C probe and DAI integration
- [ ] Test speaker output

---

### 10. WiFi SCSC

| Field | Value |
|-------|-------|
| **Module** | `wlan.ko` |
| **Source** | `drivers/net/wireless/scsc/` |
| **Target** | `vendor/samsung/wifi/scsc/` |
| **Status** | ⏳ Not Started |
| **Difficulty** | Hard |

**Porting Notes:**
- Samsung/ST-Ericsson SCSC WiFi driver (typically Cypress/Infineon chip).
- Heavy use of vendor hooks and proprietary SDIO/PCIe interface.
- Firmware loading and WLAN subsystem management.
- Kernel netdev API changes between 5.10 and 6.18 are significant.
- Locking, RCU, and NAPI changes may require rework.

**Checklist:**
- [ ] Obtain updated SCSC driver source for 6.x kernels
- [ ] Port netdev interface changes
- [ ] Port SDIO/PCIe bus interface
- [ ] Port firmware loading
- [ ] Fix `net_device_ops` and `ndo_start_xmit` changes
- [ ] Test WiFi association and data transfer

---

### 11. Modem CPIF

| Field | Value |
|-------|-------|
| **Module** | `samsung_cpif.ko` |
| **Source** | `drivers/net/samsung/cpif/` |
| **Target** | `vendor/samsung/modem/cpif/` |
| **Status** | ⏳ Not Started |
| **Difficulty** | Hard |

**Porting Notes:**
- Depends on: Power Domains (modem PD)
- Samsung CPIF (Communications Processor InterFace) handles modem data/control channels.
- Uses shared memory, DMA, and IPC interrupts.
- Complex packet routing and multiplexing for data/control/logging channels.
- Must handle modem boot, reset, and error recovery.

**Checklist:**
- [ ] Port CPIF register interface and IPC mechanism
- [ ] Port shared memory and DMA buffer management
- [ ] Port netdev link layer for modem data
- [ ] Port modem boot sequence
- [ ] DT bindings for CPIF node
- [ ] Test modem data channel establishment

---

### 12. Touchscreen

| Field | Value |
|-------|-------|
| **Module** | `sec_input.ko` |
| **Source** | `drivers/input/touchscreen/` |
| **Target** | `vendor/samsung/input/touchscreen/` |
| **Status** | ⏳ Not Started |
| **Difficulty** | Medium |

**Porting Notes:**
- Depends on: I2C (via Pinctrl)
- Samsung common touchscreen input driver framework.
- I2C-based communication, interrupt-driven.
- May use regulator and GPIO for power control.
- Input subsystem API is relatively stable but has minor changes.

**Checklist:**
- [ ] Port I2C communication layer
- [ ] Port input device registration
- [ ] Port firmware update mechanism
- [ ] Port touchscreen-specific IC driver (e.g., FocalTech, Novatek)
- [ ] Test touch input events via `evtest`

---

### 13. Sensor Hub

| Field | Value |
|-------|-------|
| **Module** | `shub.ko` |
| **Source** | `drivers/sensorhub/` |
| **Target** | `vendor/samsung/sensors/shub/` |
| **Status** | ⏳ Not Started |
| **Difficulty** | Hard |

**Porting Notes:**
- Samsung Sensor Hub manages accelerometer, gyroscope, magnetometer, etc.
- Communicates with an I/O co-processor for sensor fusion.
- Uses IIO (Industrial I/O) or custom sysfs interface.
- Firmware loading for the sensor hub co-processor.
- Complex multi-sensor data routing.

**Checklist:**
- [ ] Port IIO framework integration
- [ ] Port sensor hub IPC interface
- [ ] Port firmware loading
- [ ] Port individual sensor drivers
- [ ] Test sensor data output

---

### 14. Battery/Charger

| Field | Value |
|-------|-------|
| **Module** | `s2mf301_charger.ko`, `hl7132.ko` |
| **Source** | `drivers/battery/` |
| **Target** | `vendor/samsung/battery/` |
| **Status** | ⏳ Not Started |
| **Difficulty** | Medium |

**Porting Notes:**
- Samsung S2MF301 PMIC charger and HL7132 charger IC.
- Uses power supply subsystem (`power_supply_class`).
- I2C-based register access.
- Battery fuel gauge integration.
- Charging state machine and thermal protection.

**Checklist:**
- [ ] Port I2C probe and register map
- [ ] Port `power_supply` driver registration
- [ ] Port charging algorithm
- [ ] Port fuel gauge integration
- [ ] Test charging status reporting via `sysfs`

---

### 15. Fingerprint

| Field | Value |
|-------|-------|
| **Module** | `etspis.ko` |
| **Source** | `drivers/input/fingerprint/` |
| **Target** | `vendor/samsung/input/fingerprint/` |
| **Status** | ⏳ Not Started |
| **Difficulty** | Easy |

**Porting Notes:**
- SPI-based fingerprint sensor (e.g., Egis Technology).
- Uses SPI bus and interrupt for capture.
- Character device interface for userspace HAL.
- Relatively self-contained with minimal kernel dependencies.

**Checklist:**
- [ ] Port SPI communication
- [ ] Port character device registration
- [ ] Port IRQ handling
- [ ] Test device node creation and basic communication

---

## Kernel API Changes (5.10 → 6.18)

Major API changes that affect Samsung vendor drivers when porting from Linux 5.10 to 6.18 LTS.

### Clock Framework
| Change | Details |
|--------|---------|
| `clk_hw_register` | Refactored in some platforms; verify `samsung_clk` helpers still work |
| `clk_register_fixed_rate` | Deprecated in favor of `devm_clk_hw_register_fixed_rate` patterns |
| `of_clk_src_onecell_get` | Verify DT binding compatibility |

### DRM/KMS Subsystem
| Change | Details |
|--------|---------|
| `drm_framebuffer` | Increased type safety; `drm_framebuffer_init` changes |
| `atomic commit` | `drm_atomic_helper_commit` improvements; new state validation hooks |
| `struct drm_driver` | Removed several legacy fields; `legacy_fbdev` deprecation |
| `drm_plane` | `DRM_PLANE_TYPE_PRIMARY` vs overlay changes |
| `drm_connector` | `drm_connector_helper_funcs` restructured |
| `devm_drm_panel_init` | New devres-managed panel registration |

### Input Subsystem
| Change | Details |
|--------|---------|
| `input_dev` | `input_set_capability` usage changes |
| `input_alloc_polled_device` | API for polling devices updated |
| `input_mt_init_slots` | Multi-touch protocol improvements |

### Power Management
| Change | Details |
|--------|---------|
| `pm_runtime` | `pm_runtime_force_suspend/resume` new helpers |
| `dev_pm_ops` | `.suspend` / `.resume` vs `.pm` structure changes |
| `regulator_bulk` | `regulator_bulk_enable/disable` new error handling |

### DMA Subsystem
| Change | Details |
|--------|---------|
| `dma_alloc_coherent` | New `dma_alloc_attrs` patterns preferred |
| `dma_map_sg` | Scatter-gather DMA API tightening |
| `ion` | Removed; replaced by DMA-Buf Heaps (`dma_heap`) |
| `dma_buf` | New `dma_buf_export` flags and attachment rules |

### Device Tree
| Change | Details |
|--------|---------|
| `#clock-cells` | Must match binding spec precisely |
| `power-domain` | `power-domains` property phandle format verified strictly |
| `compatible` | Strict compatible string matching; no fallback hacks |
| `reg` | Address cell count enforced more strictly |

### Module Loading
| Change | Details |
|--------|---------|
| `module_init` | Same, but `module_platform_driver` preferred |
| `request_module` | `firmware_request_nowarn` patterns |
| `GPL-only` | Enforced more strictly; non-GPL symbols blocked |

### BPF/LSM
| Change | Details |
|--------|---------|
| BPF hooks | Samsung uses vendor hooks; verify hookpoint compatibility |
| LSM | New `security_hook_heads` structure; verify hook registration |

---

## Porting Workflow

Step-by-step guide for porting a single Samsung vendor module.

### Step 1: Extract Source
```bash
# From Samsung 5.10 kernel tree
cp -r drivers/path/to/module vendor/samsung/destination/
```

### Step 2: Identify Dependencies
```bash
# Check DT bindings and Kconfig
grep -r "depends on" vendor/samsung/destination/Kconfig
# Check for firmware files
find vendor/samsung/destination/ -name "*.fw" -o -name "*.bin"
```

### Step 3: Check API Compatibility
```bash
# Search for removed/changed APIs
rg "copy_from_user|copy_to_user" vendor/samsung/destination/
rg "ioctl_handler|unlocked_ioctl" vendor/samsung/destination/
rg "ion_alloc|ion_buffer" vendor/samsung/destination/
rg "get_user_pages" vendor/samsung/destination/
```

### Step 4: Fix Compilation Errors
- Fix `#include` paths for moved headers
- Replace deprecated APIs with 6.18 equivalents
- Fix `struct` field renames
- Fix DMA-buf and ion migration

### Step 5: Update Device Tree Bindings
- Write YAML bindings for the module's DT node
- Ensure `compatible`, `reg`, clocks, power-domains are correct
- Validate with `make dt_binding_check`

### Step 6: Test Module Loading
```bash
# Build module
make -C vendor/samsung/module M=vendor/samsung/destination modules
# Load
insmod module_name.ko
# Verify
dmesg | tail -20
lsmod | grep module_name
```

### Step 7: Functional Testing
- Verify the module performs its intended function
- Test edge cases and error paths
- Test suspend/resume if applicable

### Step 8: Document Changes
- Record all API changes made in the porting notes
- Update this tracking document
- Note any regressions or known issues

---

## Build Test Matrix

| Module | Kernel Config | Compile | Load | Functional |
|--------|:---:|:---:|:---:|:---:|
| clk-s5e8825 | ☐ | ☐ | ☐ | ☐ |
| samsung_dpu | ☐ | ☐ | ☐ | ☐ |
| samsung_dsim | ☐ | ☐ | ☐ | ☐ |
| samsung_panel | ☐ | ☐ | ☐ | ☐ |
| mali_kbase | ☐ | ☐ | ☐ | ☐ |
| samsung_mfc | ☐ | ☐ | ☐ | ☐ |
| samsung_abox | ☐ | ☐ | ☐ | ☐ |
| rt5665 | ☐ | ☐ | ☐ | ☐ |
| rt5691 | ☐ | ☐ | ☐ | ☐ |
| tfa9878 | ☐ | ☐ | ☐ | ☐ |
| aw882xx | ☐ | ☐ | ☐ | ☐ |
| wlan (SCSC) | ☐ | ☐ | ☐ | ☐ |
| samsung_cpif | ☐ | ☐ | ☐ | ☐ |
| sec_input | ☐ | ☐ | ☐ | ☐ |
| shub | ☐ | ☐ | ☐ | ☐ |
| s2mf301_charger | ☐ | ☐ | ☐ | ☐ |
| hl7132 | ☐ | ☐ | ☐ | ☐ |
| etspis | ☐ | ☐ | ☐ | ☐ |

---

## Notes

1. **Clock Controller is the foundation.** Nothing else can be tested until it is fully ported and verified. Start here.

2. **Samsung CAL-IF vs standard samsung_clk.** Samsung's 5.10 kernel abstracts clock programming behind CAL-IF calls. These must be reverse-engineered to extract the actual register writes, then re-implemented using the standard `samsung_clk` registration API.

3. **Ion to DMA-Buf Heaps.** Linux 5.10 Samsung drivers use the ION allocator. ION was removed upstream; all DMA-buf allocation must use the DMA-Buf Heaps framework (`/dev/dma_heap/`).

4. **Vendor hooks.** Samsung relies heavily on Android GKI vendor hooks for timing-critical paths (sched, thermal, etc.). Verify each hook point exists in the 6.18 ACK kernel.

5. **Out-of-tree vs in-tree.** Some modules (especially display) may need to stay as out-of-tree modules if they cannot integrate cleanly with the 6.18 DRM framework. Document this decision per-module.

6. **Device tree is critical.** Even if a module compiles, it will not probe without correct DT bindings. Test with both `module_platform_driver` probe and `of_match_table` matching.

7. **Firmware files.** Many modules require proprietary firmware blobs. These are typically found in Samsung's `/vendor/firmware/` and are loaded at runtime. Ensure they are copied to the target rootfs.

8. **GKI enforcement.** Android 17 (ACK android17-6.18) enforces GKI module signing. Ensure all vendor modules are signed with the correct vendor signing key.

9. **Cross-compilation.** All build testing should be done with the appropriate ARM64 cross-compiler targeting the Exynos 1280 (arm64, Cortex-A55/A78).

10. **Recovery.** When testing modules that affect display or storage (DECON, UFS), have a recovery mechanism (serial console + TFTP boot) to unbrick the device if the module crashes early in boot.
