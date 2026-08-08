# =============================================================================
# ADI MCU Hackathon — Dev Container
# =============================================================================
# The single dev container for the ADI MCU Hackathon. It provides the full
# toolchain used by the tutorial workbook and the resources/software/scripts
# pipeline (compile / simulate / synthesize / build-compiler):
#
#   Compiler   — LLVM 17 (clang, llc, llvm-link, opt, lld) built from source,
#                with the source + build trees preserved so a custom backend
#                can be registered and rebuilt incrementally (build_compiler.py)
#   RTL sim    — Icarus Verilog (iverilog/vvp), Verilator
#   Waveforms  — GTKWave
#   Synthesis  — Yosys, nextpnr-ice40 (open source); Intel Quartus (optional)
#   Python EDA — cocotb, PyYAML, pytest
#
# Build (context = resources/docker/):
#   docker build -t hackathon-mcu-dev .
#
# With Quartus (place the .run installer in ./quartus/ first):
#   docker build \
#     --build-arg QUARTUS_INSTALLER=QuartusLiteSetup-23.1std.0.991-linux.run \
#     -t hackathon-mcu-dev .
#
# NOTE: the LLVM source build makes the first build take a while (depends on
# host CPU/RAM); it is cached after the first build.
# =============================================================================


# =============================================================================
# Stage 1 — LLVM base: clone source, configure, initial X86-only build
#
# The source tree + pre-configured build directory are preserved so that
# registering a custom LLVM backend only needs an incremental Ninja rebuild
# (~5–10 min) rather than a full configure + compile.
# =============================================================================
FROM ubuntu:22.04 AS llvm-base

ARG LLVM_VERSION=17.0.6
# Compile parallelism. 0 = auto-detect a memory-safe job count at build time
# (~2 GB per job) so the build won't OOM on small machines. Set a number to pin it.
ARG LLVM_BUILD_JOBS=0
# Link jobs are the memory bottleneck. lld (below) keeps each link light, so 1
# serialized link job builds reliably even on ~8 GB machines. Raise on big hosts.
ARG LLVM_LINK_JOBS=1
ARG LLVM_SRC=/llvm-src
ARG LLVM_BUILD=/llvm-build
ARG LLVM_INSTALL=/opt/llvm

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build lld \
    python3 git curl ca-certificates \
    zlib1g-dev libzstd-dev libxml2-dev \
    && rm -rf /var/lib/apt/lists/*

# Shallow clone — just the tag we need, saves ~3 GB vs full history
RUN git clone --depth 1 \
    --branch llvmorg-${LLVM_VERSION} \
    https://github.com/llvm/llvm-project.git \
    ${LLVM_SRC}

# Configure with X86 only. Custom targets are added later without reconfiguring.
RUN cmake -G Ninja \
    -S ${LLVM_SRC}/llvm \
    -B ${LLVM_BUILD} \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=${LLVM_INSTALL} \
    -DLLVM_TARGETS_TO_BUILD="X86" \
    -DLLVM_ENABLE_PROJECTS="clang;lld" \
    -DLLVM_BUILD_TESTS=OFF \
    -DLLVM_INCLUDE_TESTS=OFF \
    -DLLVM_BUILD_EXAMPLES=OFF \
    -DLLVM_INCLUDE_EXAMPLES=OFF \
    -DLLVM_BUILD_DOCS=OFF \
    -DLLVM_INCLUDE_DOCS=OFF \
    -DLLVM_ENABLE_ASSERTIONS=OFF \
    -DLLVM_USE_LINKER=lld \
    -DLLVM_PARALLEL_LINK_JOBS=${LLVM_LINK_JOBS} \
    -DLLVM_OPTIMIZED_TABLEGEN=ON

# Build + install. Compile parallelism is chosen to fit available RAM (~2 GB per
# job) unless LLVM_BUILD_JOBS pins it, so the build completes rather than OOMs on
# memory-constrained machines. Cached by Docker — only reruns on input changes.
RUN set -eu; \
    cpus="$(nproc)"; \
    mem_kb="$(grep -m1 MemTotal /proc/meminfo | tr -dc '0-9')"; \
    mem_gb="$(( mem_kb / 1024 / 1024 ))"; \
    jobs="$(( mem_gb / 2 ))"; \
    if [ "${jobs}" -lt 1 ]; then jobs=1; fi; \
    if [ "${jobs}" -gt "${cpus}" ]; then jobs="${cpus}"; fi; \
    if [ "${LLVM_BUILD_JOBS}" -gt 0 ] 2>/dev/null; then jobs="${LLVM_BUILD_JOBS}"; fi; \
    echo "LLVM build: -j${jobs} (cpus=${cpus} mem=${mem_gb}GB link_jobs=${LLVM_LINK_JOBS})"; \
    ninja -C "${LLVM_BUILD}" -j"${jobs}"; \
    ninja -C "${LLVM_BUILD}" install

# Strip installed binaries (significantly reduces layer size)
RUN find ${LLVM_INSTALL}/bin -type f -executable \
    -exec strip --strip-debug {} \; 2>/dev/null || true

ENV LLVM_SRC=${LLVM_SRC} LLVM_BUILD=${LLVM_BUILD} LLVM_INSTALL=${LLVM_INSTALL}


# =============================================================================
# Stage 2 — Full development image
# =============================================================================
FROM ubuntu:22.04 AS dev

LABEL maintainer="ADI MCU Hackathon Team"
LABEL description="Dev container for the ADI MCU Hackathon — LLVM + FPGA/RTL tools"

ENV DEBIAN_FRONTEND=noninteractive
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8

# Copy LLVM install (runtime tools) AND source+build trees (for backend dev)
# Assign ownership up front so the later dev user can use them without a
# slow recursive chown over the entire LLVM tree.
COPY --from=llvm-base --chown=1000:1000 /opt/llvm     /opt/llvm
COPY --from=llvm-base --chown=1000:1000 /llvm-src     /llvm-src
COPY --from=llvm-base --chown=1000:1000 /llvm-build   /llvm-build

# ---------------------------------------------------------------------------
# System packages
# ---------------------------------------------------------------------------
RUN apt-get update && apt-get install -y --no-install-recommends \
    \
    # Build essentials (cmake + ninja are required by build_compiler.py;
    # lld is the linker the /llvm-build tree is configured to use)
    build-essential cmake ninja-build make lld \
    git curl wget ca-certificates pkg-config \
    \
    # Python
    python3 python3-pip python3-dev python-is-python3 \
    \
    # RTL simulation
    iverilog \
    verilator \
    \
    # Waveform viewer (pass -e DISPLAY or use VNC — see docker-compose.yml)
    gtkwave \
    \
    # Open-source synthesis (no licence required)
    yosys \
    \
    # Debugging / utilities
    gdb valgrind \
    xxd file less vim jq sudo \
    \
    # X11 / Quartus runtime libraries
    libx11-6 libxext6 libxtst6 libxi6 \
    libxrender1 libfreetype6 libfontconfig1 \
    libgl1 libglu1-mesa \
    \
    # LLVM runtime deps
    zlib1g libzstd1 libxml2 \
    \
    # LLVM build-time deps — required by build_compiler.py, which runs an
    # incremental CMake reconfigure + Ninja rebuild against the /llvm-build
    # tree copied from the llvm-base stage.
    zlib1g-dev libzstd-dev libxml2-dev \
    \
    && rm -rf /var/lib/apt/lists/*

# ---------------------------------------------------------------------------
# Open-source FPGA place-and-route (iCE40) — for synthesis without Quartus
# ---------------------------------------------------------------------------
RUN apt-get update && apt-get install -y --no-install-recommends \
    nextpnr-ice40 fpga-icestorm \
    && rm -rf /var/lib/apt/lists/* \
    || echo "nextpnr/icestorm not in apt for this distro — skipping"

# ---------------------------------------------------------------------------
# Python EDA tooling (cocotb testbenches, PyYAML for the script configs)
# ---------------------------------------------------------------------------
RUN pip3 install --no-cache-dir \
    cocotb \
    cocotb-test \
    pyyaml \
    pytest \
    pyelftools \
    capstone \
    numpy

# ---------------------------------------------------------------------------
# Intel Quartus Prime Lite (optional — command-line EDA for Intel FPGAs)
#
# To include Quartus:
#   1. Download from: https://www.intel.com/content/www/us/en/software-kit/
#   2. Place the .run installer in:  ./quartus/
#   3. Build with: --build-arg QUARTUS_INSTALLER=<filename>.run
# ---------------------------------------------------------------------------
ARG QUARTUS_INSTALLER=""
ARG QUARTUS_INSTALL_DIR=/opt/quartus

RUN mkdir -p "${QUARTUS_INSTALL_DIR}"

COPY quartus/ /tmp/quartus-installer/

RUN if [ -n "${QUARTUS_INSTALLER}" ] \
    && [ -f "/tmp/quartus-installer/${QUARTUS_INSTALLER}" ]; then \
    echo "Installing Quartus: ${QUARTUS_INSTALLER}" \
    && chmod +x "/tmp/quartus-installer/${QUARTUS_INSTALLER}" \
    && "/tmp/quartus-installer/${QUARTUS_INSTALLER}" \
    --mode unattended \
    --installdir "${QUARTUS_INSTALL_DIR}" \
    --accept_eula 1 \
    && printf 'export PATH="%s/quartus/bin:$PATH"\nexport QUARTUS_ROOTDIR="%s/quartus"\n' \
    "${QUARTUS_INSTALL_DIR}" "${QUARTUS_INSTALL_DIR}" \
    > /etc/profile.d/quartus.sh ; \
    else \
    echo "Quartus not requested — skipping (--build-arg QUARTUS_INSTALLER=<file> to enable)" ; \
    fi \
    && rm -rf /tmp/quartus-installer

# ---------------------------------------------------------------------------
# Helper scripts
# ---------------------------------------------------------------------------
COPY scripts/entrypoint.sh      /usr/local/bin/entrypoint
COPY scripts/install_quartus.sh /usr/local/bin/install-quartus

RUN chmod +x /usr/local/bin/entrypoint /usr/local/bin/install-quartus

# ---------------------------------------------------------------------------
# Environment
# ---------------------------------------------------------------------------
ENV PATH="/opt/llvm/bin:/opt/quartus/quartus/bin:${PATH}" \
    LLVM_SRC=/llvm-src \
    LLVM_BUILD=/llvm-build \
    LLVM_INSTALL=/opt/llvm

# ---------------------------------------------------------------------------
# Non-root dev user (uid 1000 matches the VS Code Dev Container config)
# ---------------------------------------------------------------------------
RUN useradd -m -s /bin/bash -u 1000 dev \
    && echo "dev ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

WORKDIR /workspace
USER dev

ENTRYPOINT ["/usr/local/bin/entrypoint"]
CMD ["/bin/bash"]
