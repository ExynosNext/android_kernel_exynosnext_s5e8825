# Exynos 1280 Device Tree Makefile
# ExynosNext Kernel — Linux 6.18 LTS (ACK android17-6.18)
#
# exynos1280.dtsi is an include-only file and is NOT built as a DTB.
# The board DTS files below include it and produce per-device DTBs.
#
# CONFIG_OF is used instead of CONFIG_ARCH_EXYNOS because Exynos 1280 is
# not a mainline-supported SoC and GKI does not define ARCH_EXYNOS. The
# DTBs are device overlays that compile independently of SoC platform
# code; CONFIG_OF is always set in a GKI arm64 build.

dtb-$(CONFIG_OF) += exynos1280-a53x.dtb
dtb-$(CONFIG_OF) += exynos1280-a25x.dtb
dtb-$(CONFIG_OF) += exynos1280-a26x.dtb
dtb-$(CONFIG_OF) += exynos1280-a33x.dtb
dtb-$(CONFIG_OF) += exynos1280-m33x.dtb
dtb-$(CONFIG_OF) += exynos1280-m34x.dtb
dtb-$(CONFIG_OF) += exynos1280-gta4xls.dtb
