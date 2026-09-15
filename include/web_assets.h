#pragma once

#ifndef NATIVE_TEST
#include <Arduino.h>
#else
#ifndef PROGMEM
#define PROGMEM
#endif
#endif

// Single-page responsive HTML/CSS/JS Application (No internet/CDN needed)
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ErgoBike BLE - Painel de Configuração</title>
<style>
:root {
  --bg: #0f172a; --card: #1e293b; --text: #f8fafc; --text-muted: #94a3b8;
  --primary: #38bdf8; --primary-hover: #0ea5e9; --accent: #22c55e; --danger: #ef4444;
  --border: #334155; --input-bg: #0f172a;
}
* { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
body { background: var(--bg); color: var(--text); padding: 16px; min-height: 100vh; }
.container { max-width: 680px; margin: 0 auto; display: flex; flex-direction: column; gap: 16px; }
header { text-align: center; padding: 12px 0; border-bottom: 1px solid var(--border); }
header h1 { font-size: 1.5rem; color: var(--primary); display: flex; align-items: center; justify-content: center; gap: 8px; }
header p { font-size: 0.85rem; color: var(--text-muted); margin-top: 4px; }
.card { background: var(--card); border: 1px solid var(--border); border-radius: 12px; padding: 16px; }
.card-title { font-size: 1.1rem; font-weight: 600; margin-bottom: 12px; display: flex; align-items: center; justify-content: space-between; }
.grid-2 { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }
.metric-box { background: var(--input-bg); border: 1px solid var(--border); border-radius: 8px; padding: 12px; text-align: center; }
.metric-val { font-size: 2rem; font-weight: 700; color: var(--primary); }
.metric-lbl { font-size: 0.75rem; color: var(--text-muted); text-transform: uppercase; margin-top: 2px; }
.pulse-indicator { width: 12px; height: 12px; border-radius: 50%; background: #475569; display: inline-block; transition: background 0.1s; }
.pulse-indicator.active { background: var(--accent); box-shadow: 0 0 10px var(--accent); }
.btn { background: var(--primary); color: #0f172a; font-weight: 600; border: none; border-radius: 8px; padding: 10px 16px; cursor: pointer; transition: 0.2s; font-size: 0.95rem; width: 100%; text-align: center; }
.btn:hover { background: var(--primary-hover); }
.btn-success { background: var(--accent); color: #0f172a; }
.btn-success:hover { background: #16a34a; }
.btn-danger { background: var(--danger); color: #fff; }
.btn-danger:hover { background: #dc2626; }
.btn-secondary { background: var(--border); color: var(--text); }
.btn-secondary:hover { background: #475569; }
.form-group { margin-bottom: 12px; }
label { display: block; font-size: 0.85rem; color: var(--text-muted); margin-bottom: 4px; }
input[type="text"], input[type="number"], select { width: 100%; padding: 10px; border-radius: 8px; border: 1px solid var(--border); background: var(--input-bg); color: var(--text); font-size: 0.95rem; outline: none; }
input:focus { border-color: var(--primary); }
.hint { font-size: 0.75rem; color: var(--text-muted); margin-top: 4px; }
.wizard-box { background: var(--input-bg); border: 1px dashed var(--primary); border-radius: 8px; padding: 16px; text-align: center; margin-top: 8px; }
.status-badge { font-size: 0.75rem; padding: 4px 8px; border-radius: 999px; background: #334155; color: var(--text); }
.status-badge.connected { background: #166534; color: #4ade80; }
.progress-bar { width: 100%; height: 8px; background: var(--input-bg); border-radius: 4px; overflow: hidden; margin-top: 8px; display: none; }
.progress-fill { height: 100%; width: 0%; background: var(--accent); transition: width 0.2s; }
.alert { padding: 10px; border-radius: 8px; font-size: 0.85rem; margin-top: 10px; display: none; }
.alert.show { display: block; }
.alert-success { background: #14532d; color: #86efac; }
.alert-danger { background: #7f1d1d; color: #fca5a5; }
</style>
</head>
<body>
<div class="container">
  <header>
    <h1>🚴 ErgoBike BLE <span id="pulseDot" class="pulse-indicator"></span></h1>
    <p>Painel de Controle e Calibração Wireless</p>
  </header>

  <!-- DASHBOARD AO VIVO -->
  <div class="card">
    <div class="card-title">
      <span>Dashboard em Tempo Real</span>
      <span id="connStatus" class="status-badge">Conectando...</span>
    </div>
    <div class="grid-2">
      <div class="metric-box">
        <div class="metric-val" id="valCadence">0.0</div>
        <div class="metric-lbl">Cadência (RPM)</div>
      </div>
      <div class="metric-box">
        <div class="metric-val" id="valSpeed">0.0</div>
        <div class="metric-lbl">Velocidade (km/h)</div>
      </div>
      <div class="metric-box">
        <div class="metric-val" id="valPulses">0</div>
        <div class="metric-lbl">Pulsos Roda Inércia</div>
      </div>
      <div class="metric-box">
        <div class="metric-val" id="valBattery">--%</div>
        <div class="metric-lbl" id="valBatteryV">Bateria 18650 (--V)</div>
      </div>
    </div>
  </div>

  <!-- ASSISTENTE DE CALIBRAÇÃO (10 VOLTAS) -->
  <div class="card">
    <div class="card-title">Assistente de Auto-Calibração</div>
    <p style="font-size:0.85rem; color:var(--text-muted);">
      Descubra a relação exata da sua bike sem precisar abrir a carenagem.
    </p>
    <div class="wizard-box" id="wizardBox">
      <div id="wizStep1">
        <p style="margin-bottom:12px;">Clique abaixo e depois dê exatamente <b>10 voltas completas</b> no pedal.</p>
        <button class="btn btn-success" onclick="startCalibration()">1. Iniciar Contagem de Calibração</button>
      </div>
      <div id="wizStep2" style="display:none;">
        <h3 style="color:var(--primary); margin-bottom:8px;">Pedalando...</h3>
        <p style="font-size:0.9rem;">Pulsos detectados no volante: <b id="calibCount" style="font-size:1.4rem; color:var(--accent);">0</b></p>
        <p class="hint" style="margin:12px 0;">Assim que completar as 10 voltas no pedal, clique em Concluir:</p>
        <button class="btn btn-primary" onclick="finishCalibration()">2. Concluir Calibração (10 Voltas)</button>
      </div>
    </div>
    <div id="calibAlert" class="alert"></div>
  </div>

  <!-- CONFIGURAÇÕES NVS -->
  <div class="card">
    <div class="card-title">Configurações e Parâmetros</div>
    <form id="cfgForm" onsubmit="saveSettings(event)">
      <div class="form-group">
        <label>Nome Bluetooth do Dispositivo</label>
        <input type="text" id="cfgName" name="name" maxlength="31" required>
        <div class="hint">Nome exibido no CycleGo, Zwift e apps de treino.</div>
      </div>
      <div class="grid-2">
        <div class="form-group">
          <label>Relação de Transmissão</label>
          <input type="number" step="0.01" min="0.5" max="25" id="cfgRatio" name="ratio" required>
          <div class="hint">Pulsos do volante por volta do pedal.</div>
        </div>
        <div class="form-group">
          <label>Distância Efetiva por Pulso (mm)</label>
          <input type="number" min="500" max="5000" id="cfgCirc" name="circ" required>
          <div class="hint">Distância efetiva por pulso; neste projeto o padrão é 5000mm por volta do pedal.</div>
        </div>
      </div>
      <div class="grid-2">
        <div class="form-group">
          <label>Filtro Debounce (ms)</label>
          <input type="number" min="2" max="100" id="cfgDebounce" name="debounce" required>
          <div class="hint">Anti-repique mecânico do P2.</div>
        </div>
        <div class="form-group">
          <label>Auto Deep Sleep (segundos)</label>
          <input type="number" min="30" max="1800" id="cfgSleep" name="sleep" required>
          <div class="hint">Tempo inativo para dormir.</div>
        </div>
      </div>
      <div class="form-group">
        <label>Multiplicador Calibração ADC Bateria</label>
        <input type="number" step="0.01" min="1.0" max="5.0" id="cfgAdc" name="adc" required>
        <div class="hint">Fator do divisor 100k/100k (Padrão 2.00).</div>
      </div>
      <button type="submit" class="btn" style="margin-top:8px;">Salvar Configurações</button>
    </form>
    <div id="saveAlert" class="alert"></div>
    <div style="display:flex; gap:10px; margin-top:12px;">
      <button class="btn btn-secondary" onclick="rebootDevice()">Reiniciar em Modo BLE</button>
      <button class="btn btn-danger" onclick="resetFactory()">Restaurar Padrões</button>
    </div>
  </div>

  <!-- ATUALIZAÇÃO OTA -->
  <div class="card">
    <div class="card-title">Atualização de Firmware OTA</div>
    <form id="otaForm" onsubmit="uploadOTA(event)">
      <div class="form-group">
        <label>Selecione o arquivo firmware.bin</label>
        <input type="file" id="otaFile" accept=".bin" required>
      </div>
      <button type="submit" class="btn btn-secondary">Atualizar Firmware Wireless</button>
      <div class="progress-bar" id="otaBar"><div class="progress-fill" id="otaFill"></div></div>
      <div id="otaAlert" class="alert"></div>
    </form>
  </div>
</div>

<script>
let calibActive = false;
let lastPulseTotal = 0;

function updateStatus() {
  fetch('/api/status')
    .then(r => r.json())
    .then(d => {
      document.getElementById('connStatus').innerText = 'Online';
      document.getElementById('connStatus').className = 'status-badge connected';
      document.getElementById('valCadence').innerText = d.cadence.toFixed(1);
      document.getElementById('valSpeed').innerText = d.speed.toFixed(1);
      document.getElementById('valPulses').innerText = d.pulses;
      if (d.batteryV >= 2.0) {
        document.getElementById('valBattery').innerText = d.batteryPct + '%';
        document.getElementById('valBatteryV').innerText = 'Bateria 18650 (' + d.batteryV.toFixed(2) + 'V)';
      } else {
        document.getElementById('valBattery').innerText = 'OK';
        document.getElementById('valBatteryV').innerText = 'Placa Expansão (Porta BAT)';
      }

      if (d.pulses !== lastPulseTotal) {
        lastPulseTotal = d.pulses;
        const dot = document.getElementById('pulseDot');
        dot.classList.add('active');
        setTimeout(() => dot.classList.remove('active'), 150);
      }

      if (d.calibrating) {
        document.getElementById('wizStep1').style.display = 'none';
        document.getElementById('wizStep2').style.display = 'block';
        document.getElementById('calibCount').innerText = d.calibPulses;
      }
    })
    .catch(() => {
      document.getElementById('connStatus').innerText = 'Desconectado';
      document.getElementById('connStatus').className = 'status-badge';
    });
}

function loadConfig() {
  fetch('/api/config')
    .then(r => r.json())
    .then(c => {
      document.getElementById('cfgName').value = c.name;
      document.getElementById('cfgRatio').value = c.ratio.toFixed(2);
      document.getElementById('cfgCirc').value = c.circ;
      document.getElementById('cfgDebounce').value = c.debounce;
      document.getElementById('cfgSleep').value = c.sleep;
      document.getElementById('cfgAdc').value = c.adc.toFixed(2);
    });
}

function startCalibration() {
  fetch('/api/calibrate/start', { method: 'POST' })
    .then(() => {
      document.getElementById('wizStep1').style.display = 'none';
      document.getElementById('wizStep2').style.display = 'block';
      showAlert('calibAlert', 'Contagem iniciada! Dê 10 voltas completas no pedal.', 'alert-success');
    });
}

function finishCalibration() {
  fetch('/api/calibrate/finish?turns=10', { method: 'POST' })
    .then(r => r.json())
    .then(res => {
      document.getElementById('wizStep1').style.display = 'block';
      document.getElementById('wizStep2').style.display = 'none';
      document.getElementById('cfgRatio').value = res.ratio.toFixed(2);
      showAlert('calibAlert', 'Calibração concluída com sucesso! Relação calculada: ' + res.ratio.toFixed(2) + ':1 (' + res.pulses + ' pulsos em 10 voltas).', 'alert-success');
    });
}

function saveSettings(e) {
  e.preventDefault();
  const data = {
    name: document.getElementById('cfgName').value,
    ratio: parseFloat(document.getElementById('cfgRatio').value),
    circ: parseInt(document.getElementById('cfgCirc').value),
    debounce: parseInt(document.getElementById('cfgDebounce').value),
    sleep: parseInt(document.getElementById('cfgSleep').value),
    adc: parseFloat(document.getElementById('cfgAdc').value)
  };
  fetch('/api/save', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(data)
  }).then(r => r.json()).then(res => {
    showAlert('saveAlert', 'Configurações salvas com sucesso na memória Flash!', 'alert-success');
  }).catch(() => {
    showAlert('saveAlert', 'Erro ao salvar configurações.', 'alert-danger');
  });
}

function rebootDevice() {
  if (confirm('Deseja reiniciar o ESP32 em Modo Normal de Treino (BLE)?')) {
    fetch('/api/reboot', { method: 'POST' });
    alert('Reiniciando... O ESP32 agora estará visível no CycleGo e apps BLE!');
  }
}

function resetFactory() {
  if (confirm('Tem certeza que deseja restaurar as configurações padrão de fábrica?')) {
    fetch('/api/reset', { method: 'POST' }).then(() => {
      alert('Padrões restaurados!');
      loadConfig();
    });
  }
}

function uploadOTA(e) {
  e.preventDefault();
  const fileInput = document.getElementById('otaFile');
  if (!fileInput.files.length) return;
  const file = fileInput.files[0];
  const formData = new FormData();
  formData.append('update', file);

  const bar = document.getElementById('otaBar');
  const fill = document.getElementById('otaFill');
  bar.style.display = 'block';

  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/update', true);
  xhr.upload.onprogress = (evt) => {
    if (evt.lengthComputable) {
      const pct = Math.round((evt.loaded / evt.total) * 100);
      fill.style.width = pct + '%';
    }
  };
  xhr.onload = () => {
    if (xhr.status === 200) {
      showAlert('otaAlert', 'Atualização concluída com sucesso! Reiniciando...', 'alert-success');
      setTimeout(() => location.reload(), 4000);
    } else {
      showAlert('otaAlert', 'Falha na atualização: ' + xhr.responseText, 'alert-danger');
    }
  };
  xhr.send(formData);
}

function showAlert(id, msg, type) {
  const el = document.getElementById(id);
  el.innerText = msg;
  el.className = 'alert show ' + type;
  setTimeout(() => { el.className = 'alert'; }, 6000);
}

window.onload = () => {
  loadConfig();
  updateStatus();
  setInterval(updateStatus, 500);
};
</script>
</body>
</html>
)rawliteral";
