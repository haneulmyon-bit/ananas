# Ananas: STM32 Motor Control + Telemetry Stack

![MCU](https://img.shields.io/badge/MCU-STM32F407VE-03234B?logo=stmicroelectronics&logoColor=white)
![Firmware](https://img.shields.io/badge/Firmware-C%20%2B%20STM32%20HAL-0A7EA4)
![Host](https://img.shields.io/badge/Host-Python%203-3776AB?logo=python&logoColor=white)
![Web UI](https://img.shields.io/badge/Web-FastAPI%20%2B%20WebSocket-009688?logo=fastapi&logoColor=white)
![Protocol](https://img.shields.io/badge/Protocol-Binary%20CRC16-444444)
![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)

End-to-end project for controlling a DC motor from a PC and browser UI through an STM32 firmware link.

## What Is Inside

- STM32 firmware (C, HAL, CubeMX-generated base) for `STM32F407VE`
- UART binary protocol with CRC16 framing
- Python host layer for serial transport, command sending, and telemetry parsing
- FastAPI dashboard with WebSocket live updates
- Shared protocol documentation and parser tests

## System Overview

```text
Browser UI (FastAPI + WebSocket)
            |
            v
       Python host layer
            |
            v
      UART @ 115200 baud
            |
            v
 STM32F407 firmware (motor + telemetry)
```

## Repository Layout

```text
Core/              STM32 application code
Drivers/           STM32 HAL + CMSIS
pc_host/           Python host CLI and transport layer
web_ui/            FastAPI app and frontend dashboard
shared/protocol/   Protocol spec
shared/tests/      Protocol tests
Makefile           Firmware build/flash entrypoint
```

## Firmware Highlights

- PWM motor drive on `TIM4 CH1`
- Direction control via `GPIOE0/GPIOE1`
- ADC DMA sampling (EMG, FSR2, FSR1)
- Hall sensors on `PC6/PC7/PC8`
- Telemetry stream at 20 Hz
- Speed ramping and safe direction switching

## Quick Start

### 1) Build firmware

Prerequisites:
- `arm-none-eabi-gcc`
- `make`
- `openocd` (for `make flash`)

From repository root:

```bash
make -j4
```

Output is generated in `build_F407VE/` (`.elf`, `.hex`, `.bin`).

### 2) Flash firmware

```bash
make flash
```

### 3) Run host CLI

```bash
python -m venv .venv
.venv\Scripts\activate
python -m pip install -r pc_host/requirements.txt

python pc_host/cli.py --port COM7 monitor
python pc_host/cli.py --port COM7 set-speed 30 --duration 3
python pc_host/cli.py --port COM7 stop
```

### 4) Run web dashboard

```bash
python -m venv .venv
.venv\Scripts\activate
python -m pip install -r web_ui/requirements.txt

$env:HOST_COM_PORT="COM7"
uvicorn web_ui.app.main:app --reload --host 127.0.0.1 --port 8000
```

Open: `http://127.0.0.1:8000`

### 5) Run protocol tests

```bash
python -m pip install -r requirements-dev.txt
python -m pytest shared/tests -q
```

## Protocol

- Spec: [`shared/protocol/protocol_v1.md`](shared/protocol/protocol_v1.md)
- Includes binary frame format, message types, and examples.

## Additional Docs

- Modular architecture notes: [`README_modular_architecture.md`](README_modular_architecture.md)

## Publish To GitHub

If this folder is not yet a Git repository:

```bash
git init
git add .
git commit -m "Initial commit"
git branch -M main
git remote add origin https://github.com/<your-user>/<your-repo>.git
git push -u origin main
```

## License

This project is licensed under the MIT License. See [`LICENSE`](LICENSE).

Third-party components (STM32 HAL/CMSIS) keep their original licenses under `Drivers/**/LICENSE.txt`.
