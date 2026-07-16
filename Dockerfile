# ============================================================================
# ExynosNext Kernel — Docker Build Environment
# Complete Linux build environment for building Exynos 1280 kernel on any OS
# ============================================================================

FROM ubuntu:24.04 AS builder

ARG DEBIAN_FRONTEND=noninteractive
ARG JOBS=8
ARG VARIANT=ksu

LABEL maintainer="ExynosNext Project"
LABEL description="ExynosNext Kernel Build Environment (Linux 6.18 LTS for Exynos 1280)"

# ── Install build dependencies ──────────────────────────────────────────
RUN apt-get update -qq && apt-get install -y -qq --no-install-recommends \
    bc bison build-essential ccache curl flex git gnupg2 \
    libncurses5-dev libssl-dev python3 python3-pip python3-git \
    lz4 zip unzip tar xz-utils cpio device-tree-compiler \
    gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu \
    wget ca-certificates sudo nano vim \
    && rm -rf /var/lib/apt/lists/*

# ── Install repo tool ───────────────────────────────────────────────────
RUN mkdir -p /usr/local/bin && \
    curl -s https://storage.googleapis.com/git-repo-downloads/repo > /usr/local/bin/repo && \
    chmod a+x /usr/local/bin/repo

# ── Install Bazelisk (for Kleaf) ────────────────────────────────────────
RUN curl -Lo /usr/local/bin/bazel https://github.com/bazelbuild/bazelisk/releases/latest/download/bazelisk-linux-amd64 && \
    chmod +x /usr/local/bin/bazel

# ── Setup ccache ─────────────────────────────────────────────────────────
RUN mkdir -p /root/.ccache/exynosnext && \
    ccache -M 50G 2>/dev/null || true

# ── Create non-root user ────────────────────────────────────────────────
RUN useradd -m -s /bin/bash builder && \
    echo "builder ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

# ── Set environment ─────────────────────────────────────────────────────
ENV HOME=/home/builder
ENV SHELL=/bin/bash
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8
ENV PATH="/home/builder/.local/bin:/usr/local/bin:${PATH}"
ENV CCACHE_DIR="/root/.ccache/exynosnext"

WORKDIR /workspace

# ── Default entrypoint ──────────────────────────────────────────────────
ENTRYPOINT ["/bin/bash"]
