from __future__ import annotations

import asyncio
import os
import sys
from pathlib import Path
from typing import Set

from fastapi import FastAPI, HTTPException, Request, WebSocket, WebSocketDisconnect
from fastapi.responses import HTMLResponse
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates
from pydantic import BaseModel, Field

ROOT_DIR = Path(__file__).resolve().parents[2]
PC_HOST_DIR = ROOT_DIR / "pc_host"
if str(PC_HOST_DIR) not in sys.path:
    sys.path.insert(0, str(PC_HOST_DIR))

from host.controller import HostController  # noqa: E402
from host.logger import TelemetryLogger  # noqa: E402
from host.serial_transport import SerialTransport  # noqa: E402
from host.telemetry_store import TelemetryStore  # noqa: E402
from serial.tools import list_ports  # noqa: E402

APP_DIR = Path(__file__).resolve().parent
templates = Jinja2Templates(directory=str(APP_DIR / "templates"))

app = FastAPI(title="STM32 Motor Telemetry UI")
app.mount("/static", StaticFiles(directory=str(APP_DIR / "static")), name="static")

_com_port = os.getenv("HOST_COM_PORT", "").strip()
_baud = int(os.getenv("HOST_BAUD", "115200"))
_timeout = float(os.getenv("HOST_TIMEOUT", "0.02"))
_log_jsonl = os.getenv("HOST_LOG_JSONL", "")
_log_csv = os.getenv("HOST_LOG_CSV", "")

_transport = SerialTransport(_com_port, _baud, _timeout)
_store = TelemetryStore(history_size=3000)
_logger = TelemetryLogger(jsonl_path=_log_jsonl or None, csv_path=_log_csv or None)
_controller = HostController(_transport, _store, _logger)

_loop: asyncio.AbstractEventLoop | None = None
_ws_clients: Set[WebSocket] = set()
_unsubscribe = None
_reconnect_task: asyncio.Task | None = None
_shutdown_event: asyncio.Event | None = None
_last_serial_error: str | None = None


class SpeedCommand(BaseModel):
    speed: int = Field(..., ge=-100, le=100)


class PortCommand(BaseModel):
    port: str = Field(..., min_length=3, max_length=32)


def _list_ports() -> list[dict]:
    ports = []
    for p in list_ports.comports():
        ports.append({"device": p.device, "description": p.description or ""})
    return ports


def _pick_auto_port() -> str | None:
    ports = _list_ports()
    if not ports:
        return None

    # Prefer common USB-TTL / VCP adapters first.
    preferred_keywords = ("ch340", "usb-serial", "usb serial", "stlink", "vcp", "cp210")
    for p in ports:
        desc = p["description"].lower()
        if any(k in desc for k in preferred_keywords):
            return p["device"]

    return ports[0]["device"]


def _set_transport_port(port: str) -> None:
    global _com_port
    _com_port = port
    _transport.port = port


async def _broadcast(sample: dict) -> None:
    stale = []
    for ws in list(_ws_clients):
        try:
            await ws.send_json(sample)
        except Exception:
            stale.append(ws)
    for ws in stale:
        _ws_clients.discard(ws)


def _on_telemetry(sample: dict) -> None:
    if _loop is None:
        return
    _loop.call_soon_threadsafe(lambda: asyncio.create_task(_broadcast(sample)))


async def _reconnect_loop() -> None:
    global _last_serial_error

    while _shutdown_event is not None and not _shutdown_event.is_set():
        if not _controller.is_connected:
            if _controller.is_running and not _controller.is_connected:
                _controller.stop()
            if not _com_port:
                auto_port = _pick_auto_port()
                if auto_port:
                    _set_transport_port(auto_port)
            try:
                _controller.start()
                _last_serial_error = None
            except Exception as exc:
                _last_serial_error = str(exc)
                # If the current port fails, try auto fallback on next iteration.
                auto_port = _pick_auto_port()
                if auto_port and auto_port != _com_port:
                    _set_transport_port(auto_port)
        await asyncio.sleep(1.0)


@app.on_event("startup")
async def on_startup() -> None:
    global _loop, _unsubscribe, _reconnect_task, _shutdown_event
    _loop = asyncio.get_running_loop()
    _shutdown_event = asyncio.Event()
    if not _com_port:
        auto_port = _pick_auto_port()
        if auto_port:
            _set_transport_port(auto_port)
    _unsubscribe = _store.subscribe(_on_telemetry)
    _reconnect_task = asyncio.create_task(_reconnect_loop())


@app.on_event("shutdown")
async def on_shutdown() -> None:
    global _unsubscribe, _reconnect_task
    if _shutdown_event is not None:
        _shutdown_event.set()
    if _reconnect_task is not None:
        _reconnect_task.cancel()
        _reconnect_task = None
    if _unsubscribe is not None:
        _unsubscribe()
        _unsubscribe = None
    _controller.stop()


@app.get("/", response_class=HTMLResponse)
async def index(request: Request) -> HTMLResponse:
    return templates.TemplateResponse("index.html", {"request": request})


@app.get("/api/status")
def api_status() -> dict:
    return {
        "connected": _controller.is_connected,
        "running": _controller.is_running,
        "port": _com_port,
        "baud": _baud,
        "serial_error": _last_serial_error,
        "last_frame_age_ms": _controller.get_last_frame_age_ms(),
        "latest": _controller.get_latest(),
    }


@app.get("/api/ports")
def api_ports() -> dict:
    return {
        "selected": _com_port,
        "ports": _list_ports(),
    }


@app.post("/api/port")
def api_set_port(cmd: PortCommand) -> dict:
    global _last_serial_error
    port = cmd.port.strip()
    if not port:
        raise HTTPException(status_code=400, detail="port is empty")

    if _controller.is_running:
        _controller.stop()
    _set_transport_port(port)
    _last_serial_error = None
    return {"ok": True, "port": _com_port}


@app.post("/api/cmd/speed")
def api_cmd_speed(cmd: SpeedCommand) -> dict:
    try:
        _controller.set_speed(cmd.speed)
    except Exception as exc:
        raise HTTPException(status_code=503, detail=str(exc)) from exc
    return {"ok": True, "speed": cmd.speed}


@app.post("/api/cmd/stop")
def api_cmd_stop() -> dict:
    try:
        _controller.stop_motor()
    except Exception as exc:
        raise HTTPException(status_code=503, detail=str(exc)) from exc
    return {"ok": True}


@app.websocket("/ws")
async def ws_stream(ws: WebSocket) -> None:
    await ws.accept()
    _ws_clients.add(ws)
    latest = _controller.get_latest()
    if latest:
        await ws.send_json(latest)

    try:
        while True:
            msg = await ws.receive_text()
            if msg.strip().lower() == "ping":
                await ws.send_text("pong")
    except WebSocketDisconnect:
        pass
    finally:
        _ws_clients.discard(ws)
