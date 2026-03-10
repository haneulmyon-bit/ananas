# Modular Architecture Scaffold

This repository contains:
- Firmware (STM32CubeIDE/HAL) in the existing root project (`Core`, `Drivers`, `Makefile`).
- PC host block in `pc_host/`.
- Web UI block (MVC with FastAPI + WS) in `web_ui/`.
- Shared protocol and tests in `shared/`.

## Quick Start (Windows)

1. **Firmware build**
   - `make -j4`
   - Flash using your usual ST-Link workflow.

2. **PC host CLI**
   - `cd pc_host`
   - `python -m venv .venv`
   - `.venv\\Scripts\\activate`
   - `pip install -r requirements.txt`
   - `python cli.py --port COM7 monitor`

3. **Web UI**
   - `cd web_ui`
   - `python -m venv .venv`
   - `.venv\\Scripts\\activate`
   - `pip install -r requirements.txt`
   - `set HOST_COM_PORT=COM7`
   - `uvicorn app.main:app --reload --host 127.0.0.1 --port 8000`
   - Open `http://127.0.0.1:8000`

