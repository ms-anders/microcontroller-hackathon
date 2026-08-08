# ADI MCU Hackathon

The materials for the **ADI MCU Hackathon**. Participating teams
design their own **CPU** — from the instruction set, to the Verilog hardware, to
an LLVM compiler backend that turns C into programs their processor can run — and
verify it in simulation (and optionally on FPGA).

This repository contains everything a team needs to take part:

- **Tutorials** — a getting-started guide and a staged hands-on workbook,
  published as PDFs in [docs/](docs/).
- **Resources** — a fill-in-the-blanks LLVM backend template and the
  build/simulate/synthesize tooling ([resources/software/scripts/](resources/software/scripts/)).
- **Dev container** — a single Docker image with the full toolchain
  pre-installed (LLVM, Icarus Verilog, Verilator, Yosys, GTKWave, and more).

---

## Documents

Read these before the event — building the dev container for the first time takes
a while.

| Document | File | Description |
|---|---|---|
| **Getting Started** | [docs/getting-started.pdf](docs/getting-started.pdf) | Read first: environment setup, tools, and readiness checklist |
| **Tutorial Workbook** | [docs/tutorial-workbook.pdf](docs/tutorial-workbook.pdf) | Hands-on lab guide: stages 0–3 and stretch goals |

---

## Quick Start

You need [VS Code](https://code.visualstudio.com/),
[Docker](https://www.docker.com/products/docker-desktop/), and the
[Dev Containers extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers).

Open the project folder in VS Code:

```bash
code microcontroller-hackathon
```

Then in VS Code: `F1` → **Dev Containers: Reopen in Container**. The first build
compiles LLVM from source (this takes a little while) and is cached afterwards.
Verify the toolchain inside the container:

```bash
clang --version
iverilog -V
yosys -V
python3 -c "import yaml, cocotb; print('OK')"
```

Full setup instructions are in the Getting Started PDF and the
[dev container README](resources/docker/README.md).

---

## Repository Structure

```
microcontroller-hackathon/
├── .devcontainer/            # VS Code dev container config (builds the docker image)
├── docs/                     # Pre-built tutorial PDFs (getting started + workbook)
└── resources/
    ├── docker/               # The single dev container (Dockerfile, compose, scripts)
    └── software/
        ├── scripts/          # compile / simulate / build_compiler / synthesize
        └── template/         # Fill-in-the-blanks LLVM backend skeleton + assembler
```

---

## License

Apache License 2.0, except the LLVM backend sources under
`resources/software/template/llvm-backend/`, which are Apache-2.0 **WITH
LLVM-exception**. Every source file carries an SPDX `License-Identifier` header.
Full details in [LICENSE.md](LICENSE.md).
