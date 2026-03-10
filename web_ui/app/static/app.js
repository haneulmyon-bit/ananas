const MAX_POINTS = 180;

const connDot = document.getElementById("connDot");
const connText = document.getElementById("connText");
const ageText = document.getElementById("ageText");
const serialErr = document.getElementById("serialErr");
const portSelect = document.getElementById("portSelect");
const btnApplyPort = document.getElementById("btnApplyPort");

const speedSlider = document.getElementById("speedSlider");
const speedValue = document.getElementById("speedValue");
const btnSet = document.getElementById("btnSet");
const btnForward = document.getElementById("btnForward");
const btnReverse = document.getElementById("btnReverse");
const btnStop = document.getElementById("btnStop");

const hall1Lamp = document.getElementById("hall1Lamp");
const hall2Lamp = document.getElementById("hall2Lamp");
const hall3Lamp = document.getElementById("hall3Lamp");

const vTs = document.getElementById("vTs");
const vEmg = document.getElementById("vEmg");
const vFsr2 = document.getElementById("vFsr2");
const vFsr1 = document.getElementById("vFsr1");
const vMotorSpd = document.getElementById("vMotorSpd");
const vMotorDuty = document.getElementById("vMotorDuty");
const vMotorDir = document.getElementById("vMotorDir");

speedSlider.addEventListener("input", () => {
  speedValue.textContent = speedSlider.value;
});

function setConnection(connected) {
  connDot.className = connected ? "dot online" : "dot offline";
  connText.textContent = connected ? "Connected" : "Disconnected";
}

function setLamp(node, on) {
  node.className = on ? "lamp on" : "lamp off";
}

function dirText(direction) {
  if (direction === 1) return "FWD";
  if (direction === 2) return "REV";
  return "STOP";
}

const chartCtx = document.getElementById("fsrChart");
const fsrChart = new Chart(chartCtx, {
  type: "line",
  data: {
    labels: [],
    datasets: [
      {
        label: "FSR1",
        data: [],
        borderColor: "#19c2a0",
        backgroundColor: "rgba(25,194,160,0.2)",
        borderWidth: 2,
        pointRadius: 0,
      },
      {
        label: "FSR2",
        data: [],
        borderColor: "#f79a35",
        backgroundColor: "rgba(247,154,53,0.2)",
        borderWidth: 2,
        pointRadius: 0,
      },
    ],
  },
  options: {
    animation: false,
    responsive: true,
    maintainAspectRatio: false,
    scales: {
      x: { ticks: { color: "#8fb0c3" }, grid: { color: "rgba(83,122,148,0.15)" } },
      y: { ticks: { color: "#8fb0c3" }, grid: { color: "rgba(83,122,148,0.15)" } },
    },
    plugins: {
      legend: { labels: { color: "#d5e8f6" } },
    },
  },
});

function pushChartPoint(sample) {
  const labels = fsrChart.data.labels;
  labels.push(sample.timestamp_ms);
  fsrChart.data.datasets[0].data.push(sample.fsr1);
  fsrChart.data.datasets[1].data.push(sample.fsr2);

  if (labels.length > MAX_POINTS) {
    labels.shift();
    fsrChart.data.datasets[0].data.shift();
    fsrChart.data.datasets[1].data.shift();
  }
  fsrChart.update("none");
}

function renderSample(sample) {
  if (!sample || typeof sample !== "object") return;

  vTs.textContent = sample.timestamp_ms ?? 0;
  vEmg.textContent = sample.emg ?? 0;
  vFsr2.textContent = sample.fsr2 ?? 0;
  vFsr1.textContent = sample.fsr1 ?? 0;
  vMotorSpd.textContent = sample.motor_speed_pct ?? 0;
  vMotorDuty.textContent = sample.motor_duty_pct ?? 0;
  vMotorDir.textContent = dirText(sample.motor_direction ?? 0);

  setLamp(hall1Lamp, Boolean(sample.hall1));
  setLamp(hall2Lamp, Boolean(sample.hall2));
  setLamp(hall3Lamp, Boolean(sample.hall3));

  pushChartPoint(sample);
}

async function sendSpeed(speed) {
  const response = await fetch("/api/cmd/speed", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ speed }),
  });
  if (!response.ok) {
    const data = await response.json().catch(() => ({}));
    throw new Error(data.detail || `HTTP ${response.status}`);
  }
}

async function sendStop() {
  const response = await fetch("/api/cmd/stop", { method: "POST" });
  if (!response.ok) {
    const data = await response.json().catch(() => ({}));
    throw new Error(data.detail || `HTTP ${response.status}`);
  }
}

btnSet.addEventListener("click", async () => {
  const speed = parseInt(speedSlider.value, 10);
  try {
    await sendSpeed(speed);
  } catch (err) {
    alert(`Failed to send speed: ${err.message}`);
  }
});

btnForward.addEventListener("click", async () => {
  const speed = Math.abs(parseInt(speedSlider.value, 10));
  speedSlider.value = String(speed);
  speedValue.textContent = String(speed);
  try {
    await sendSpeed(speed);
  } catch (err) {
    alert(`Failed to send speed: ${err.message}`);
  }
});

btnReverse.addEventListener("click", async () => {
  const speed = -Math.abs(parseInt(speedSlider.value, 10));
  speedSlider.value = String(speed);
  speedValue.textContent = String(speed);
  try {
    await sendSpeed(speed);
  } catch (err) {
    alert(`Failed to send speed: ${err.message}`);
  }
});

btnStop.addEventListener("click", async () => {
  try {
    await sendStop();
    speedSlider.value = "0";
    speedValue.textContent = "0";
  } catch (err) {
    alert(`Failed to stop motor: ${err.message}`);
  }
});

async function refreshPorts(selectedFromStatus = null) {
  try {
    const response = await fetch("/api/ports");
    const data = await response.json();
    const selected = selectedFromStatus ?? data.selected;
    const ports = Array.isArray(data.ports) ? data.ports : [];

    portSelect.innerHTML = "";
    for (const p of ports) {
      const opt = document.createElement("option");
      opt.value = p.device;
      opt.textContent = `${p.device} ${p.description ? `- ${p.description}` : ""}`;
      if (p.device === selected) {
        opt.selected = true;
      }
      portSelect.appendChild(opt);
    }
  } catch {
    // Keep last known options.
  }
}

btnApplyPort.addEventListener("click", async () => {
  const port = portSelect.value;
  if (!port) return;
  try {
    const response = await fetch("/api/port", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ port }),
    });
    if (!response.ok) {
      const data = await response.json().catch(() => ({}));
      throw new Error(data.detail || `HTTP ${response.status}`);
    }
    await refreshStatus();
  } catch (err) {
    alert(`Failed to set port: ${err.message}`);
  }
});

async function refreshStatus() {
  try {
    const response = await fetch("/api/status");
    const status = await response.json();
    setConnection(Boolean(status.connected));
    ageText.textContent =
      status.last_frame_age_ms == null ? "age: n/a" : `age: ${status.last_frame_age_ms} ms`;
    serialErr.textContent = status.serial_error ? `serial error: ${status.serial_error}` : "";
    refreshPorts(status.port);
    if (status.latest && Object.keys(status.latest).length > 0) {
      renderSample(status.latest);
    }
  } catch {
    setConnection(false);
    ageText.textContent = "age: n/a";
    serialErr.textContent = "serial error: failed to fetch /api/status";
  }
}

function openWebSocket() {
  const wsProto = window.location.protocol === "https:" ? "wss" : "ws";
  const ws = new WebSocket(`${wsProto}://${window.location.host}/ws`);

  ws.onopen = () => {
    setConnection(true);
  };

  ws.onmessage = (event) => {
    if (event.data === "pong") return;
    try {
      const sample = JSON.parse(event.data);
      renderSample(sample);
    } catch {
      // Ignore malformed messages.
    }
  };

  ws.onclose = () => {
    setConnection(false);
    setTimeout(openWebSocket, 1000);
  };

  setInterval(() => {
    if (ws.readyState === WebSocket.OPEN) {
      ws.send("ping");
    }
  }, 10000);
}

setInterval(refreshStatus, 1000);
refreshStatus();
refreshPorts();
openWebSocket();
