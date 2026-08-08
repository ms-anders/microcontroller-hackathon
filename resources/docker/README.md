# ADI MCU Hackathon Dev Container

The single pre-configured Docker environment for the ADI MCU Hackathon. It
provides every tool used by the tutorial workbook and the
`resources/software/scripts` pipeline.

## What's Inside

| Category | Tools |
|----------|-------|
| **Compiler** | LLVM 17 — `clang`, `llc`, `llvm-link`, `opt`, `lld` (built from source, with source + build trees preserved for custom-backend rebuilds) |
| **RTL Simulation** | Icarus Verilog (`iverilog`/`vvp`), Verilator |
| **Waveforms** | GTKWave |
| **Synthesis** | Yosys, nextpnr-ice40 (open source); Intel Quartus Lite (optional) |
| **Python EDA** | cocotb, PyYAML, pytest |
| **General** | Python 3, pip, git, make, cmake, ninja |

The LLVM source + build trees are kept in the image (`/llvm-src`, `/llvm-build`,
`/opt/llvm`) so `scripts/build_compiler.py` can register a custom backend and
rebuild `llc` incrementally rather than from scratch.

### Not Included (too large / licence restrictions)

| Tool | Size | How to get it |
|------|------|---------------|
| Intel Quartus Lite | ~15 GB | Optional build arg — see below, or install natively on lab machines |
| Xilinx Vivado WebPACK | ~30 GB | Install natively |

## Quick Start

### Option A: VS Code Dev Container (recommended)

1. Install [Docker Desktop](https://www.docker.com/products/docker-desktop/) and the [Dev Containers extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers)
2. Open this repo in VS Code
3. Press `F1` → **Dev Containers: Reopen in Container**
4. Wait for the build (the first build takes a little while — the LLVM source build; cached after that)
5. You're in — all tools available in the integrated terminal

### Option B: Docker Compose

```bash
cd resources/docker
docker compose build
docker compose up -d
docker compose exec dev bash

# Verify
clang --version
llc --version | head -1
iverilog -V
yosys -V
python3 -c "import yaml, cocotb; print('PyYAML + cocotb OK')"
```

## Adding a Custom LLVM Backend

The container ships with an X86-only LLVM plus the full source + build tree.
To register your own backend and rebuild `llc`:

```bash
python3 resources/software/scripts/build_compiler.py <your.build-compiler.yml>
```

Because the build tree is pre-configured, only your new target is compiled
(~5–10 min) rather than the whole of LLVM.

## Quartus Lite (Optional)

If you want Quartus inside the container (check Intel's EULA first):

1. Download the Quartus Lite installer from [Intel](https://www.intel.com/content/www/us/en/software-kit/)
2. Place `QuartusLiteSetup-*.run` in `./quartus/`
3. Build with the installer name as a build arg:

```bash
docker build --build-arg QUARTUS_INSTALLER=QuartusLiteSetup-23.1std.0.991-linux.run -t hackathon-mcu-dev .
```

You can also install it into a running container with `install-quartus <installer.run>`.

> **Legal note:** Quartus Lite is free-of-charge but confirm Intel's EULA permits
> containerised distribution before sharing the image externally.

## File Structure

```
resources/docker/
├── Dockerfile              # Single dev image — LLVM + FPGA/RTL tools
├── docker-compose.yml      # Compose config
├── scripts/
│   ├── entrypoint.sh       # Container entry point (tool inventory banner)
│   └── install_quartus.sh  # Optional Quartus installer helper
├── quartus/                # Drop the Quartus .run installer here (git-ignored)
└── README.md               # This file

.devcontainer/
└── devcontainer.json       # VS Code Dev Container config (builds this image)
```
