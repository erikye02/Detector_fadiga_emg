// ============================================================
// FASE 5D — Menu web com 3 modos (Fadiga / Jogo / Osciloscopio)
// ============================================================
// Placa: ESP32-C3 com OLED 72x40 embutido
//
// Um unico firmware que serve UMA pagina web com um MENU inicial.
// A partir do menu escolhe-se um dos modos, cada um com sua tela:
//
//   1. FADIGA (RMS)   -> monitor de fadiga por amplitude (fase5b)
//   2. JOGO DO DINO   -> pula por contracao muscular  (fase5c)
//   3. OSCILOSCOPIO   -> sinal EMG bruto rolando ao vivo
//
// O ESP so processa o modo ativo. A troca de modo e feita pela
// web (rota /modo) e tambem reflete no OLED embutido. A estrutura
// e extensivel: para um 4o modo, basta um card no HTML, um ramo
// em /modo e uma funcao processarXxx().
//
// Bibliotecas: U8g2 + WiFi/WebServer.
// ============================================================

#include <WiFi.h>
#include <WebServer.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <esp_timer.h>
#include <math.h>

// ===================== CONFIGURACAO WIFI =====================
const char* SSID  = "NOME_DA_SUA_REDE";    // <-- ALTERE AQUI
const char* SENHA = "SENHA_DA_SUA_REDE";   // <-- ALTERE AQUI
// ============================================================

// --- Pinos (mesmos do fase4b validado) ---
#define PINO_EMG      0
#define PINO_LO_PLUS  2
#define PINO_LO_MINUS 1
#define PINO_SDA      5
#define PINO_SCL      6

// --- Display ---
U8G2_SSD1306_72X40_ER_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);

// --- WiFi ---
WebServer server(80);

// ===================== TIMER / AMOSTRAGEM ===================
volatile int  amostraAtual = 0;
volatile bool novaAmostra = false;
volatile bool eletrodoOK = true;

// ===================== MODO ATIVO ===========================
enum Modo { MENU, FADIGA, JOGO, OSC };
volatile Modo modo = MENU;

// ===================== MODO FADIGA (RMS) ====================
#define N_FAD               256    // janela ~256ms a 1000Hz
#define RMS_MINIMO          30.0   // abaixo disso = sem contracao
#define JANELAS_AQUECIMENTO 4      // ~1s descartado (rampa)
#define JANELAS_CALIBRACAO  24     // ~6s de coleta
#define CV_MAXIMO           0.35   // coef. variacao maximo aceito

int   bufFad[N_FAD];
int   idxFad = 0;

enum EstadoFad { FAD_PARADO, FAD_CAPTURANDO, FAD_CALIBRANDO, FAD_MONITORANDO };
EstadoFad estadoFad = FAD_PARADO;
volatile bool pedidoCalibrarFad = false;

float calibRMS[JANELAS_CALIBRACAO];
int   contAquecimento = 0;
int   contCalibJanelas = 0;
float rmsBaseline = 0;
float rmsAtual = 0;
float rmsSuave = 0;
float fadigaPct = 0;
String msgFad = "Parado";

#define TAMANHO_MEDIA 5
float bufferMedia[TAMANHO_MEDIA];
int   indiceMedia = 0;
bool  mediaCheia = false;

// historico de fadiga para o mini-grafico do OLED
#define LARGURA_GRAFICO 72
float historicoFadiga[LARGURA_GRAFICO];
int   posHistorico = 0;

// ===================== MODO JOGO (onset) ====================
#define N_JOGO          64    // janela curta ~64ms para reflexo
#define JANELAS_REPOUSO 30    // ~2s relaxado

int   bufJogo[N_JOGO];
int   idxJogo = 0;

enum EstadoJogo { JOGO_PARADO, JOGO_CALIBRANDO, JOGO_JOGANDO };
EstadoJogo estadoJogo = JOGO_PARADO;
volatile bool pedidoCalibrarJogo = false;

float repousoBuf[JANELAS_REPOUSO];
int   contRepouso = 0;
float repMedia = 0;
float repDesvio = 0;

float rmsCurto = 0;
volatile int limiarDelta = 80;
float limiarAlto = 0;
float limiarBaixo = 0;
bool  armado = true;
volatile unsigned long contPulos = 0;
unsigned long ultimoPuloMs = 0;

// ===================== MODO OSCILOSCOPIO ====================
#define N_SCOPE 240          // amostras mantidas para exibir
int   scopeBuf[N_SCOPE];
int   scopePos = 0;

// ===================== PAGINA HTML ==========================
const char PAGINA_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>EMG ESP32-C3</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
      background: #0f0f1a; color: #e0e0e0; min-height: 100vh;
      padding: 18px; max-width: 760px; margin: 0 auto;
    }
    h1 { text-align: center; font-size: 1.4em; color: #7c8aff; }
    .sub { text-align: center; color: #666; font-size: 0.78em; margin-bottom: 14px; }
    .topbar { display: flex; align-items: center; gap: 10px; margin-bottom: 12px; }
    .topbar h1 { flex: 1; text-align: left; font-size: 1.15em; }
    .btn-voltar { background: #2a2a3e; color: #b8b8d0; border: none;
                  padding: 8px 14px; border-radius: 8px; cursor: pointer;
                  font-size: 0.9em; font-weight: bold; }
    .btn-voltar:active { transform: scale(0.95); }

    .status { text-align: center; padding: 9px; border-radius: 8px;
              margin-bottom: 12px; font-weight: bold; }
    .status.ok    { background: #1a3a1a; color: #4ade80; }
    .status.aviso { background: #3a3a1a; color: #fbbf24; }
    .status.erro  { background: #3a1a1a; color: #f87171; }

    .hidden { display: none !important; }

    /* ---- Menu ---- */
    .cards { display: grid; gap: 14px; }
    .card { background: #1a1a2e; border: 1px solid #2a2a3e; border-radius: 14px;
            padding: 20px; cursor: pointer; display: flex; align-items: center;
            gap: 16px; transition: transform 0.1s, border-color 0.2s, background 0.2s; }
    .card:hover { border-color: #7c8aff; background: #20203a; }
    .card:active { transform: scale(0.98); }
    .card .ico { font-size: 2.4em; line-height: 1; }
    .card .txt h2 { font-size: 1.1em; color: #e0e0e0; margin-bottom: 3px; }
    .card .txt p { font-size: 0.82em; color: #888; }

    /* ---- Botoes ---- */
    .botoes { display: flex; gap: 10px; justify-content: center;
              margin-bottom: 14px; flex-wrap: wrap; }
    .btn { padding: 11px 20px; font-size: 0.95em; font-weight: bold; border: none;
           border-radius: 10px; cursor: pointer; transition: transform 0.1s; }
    .btn:active { transform: scale(0.95); }
    .btn-iniciar  { background: #16c784; color: #fff; }
    .btn-calibrar { background: #7c8aff; color: #fff; }
    .btn-parar    { background: #ea3943; color: #fff; }
    .btn-reset    { background: #555; color: #fff; }
    .btn-cinza    { background: #555; color: #fff; }

    /* ---- Fadiga ---- */
    .metricas { display: grid; grid-template-columns: repeat(3, 1fr);
                gap: 10px; margin-bottom: 16px; }
    .metrica { background: #1a1a2e; border-radius: 10px; padding: 12px; text-align: center; }
    .metrica .rotulo { font-size: 0.7em; color: #888; text-transform: uppercase; letter-spacing: 1px; }
    .metrica .valor { font-size: 1.6em; font-weight: bold; margin-top: 4px; }
    .caixa { background: #1a1a2e; border-radius: 10px; padding: 14px; margin-bottom: 16px; }
    .caixa h3 { color: #888; font-size: 0.78em; text-transform: uppercase;
                letter-spacing: 1px; margin-bottom: 8px; }
    .fadiga-label { display: flex; justify-content: space-between; margin-bottom: 8px; align-items: baseline; }
    .fadiga-label .porcent { font-size: 1.8em; font-weight: bold; }
    .barra-fundo { width: 100%; height: 22px; background: #2a2a3e; border-radius: 11px; overflow: hidden; }
    .barra-preenchida { height: 100%; border-radius: 11px; transition: width 0.3s, background 0.3s; }
    canvas { width: 100%; border-radius: 6px; background: #12122a; display: block; }
    #graficoRMS, #graficoFadiga { height: 150px; }

    /* ---- Jogo ---- */
    canvas#jogo { height: 220px; image-rendering: pixelated; margin-bottom: 12px; }
    .medidor-topo { display: flex; justify-content: space-between; font-size: 0.8em; color: #888; margin-bottom: 6px; }
    .medidor { position: relative; height: 20px; background: #2a2a3e; border-radius: 10px; overflow: hidden; }
    #barraRms { height: 100%; width: 0%; background: #16c784; transition: width 0.05s linear; }
    #marcaLimiar { position: absolute; top: -3px; width: 3px; height: 26px; background: #f87171; }
    .info { text-align: center; font-size: 0.8em; color: #888; margin-top: 8px; }

    /* ---- Osciloscopio ---- */
    canvas#scope { height: 260px; }
  </style>
</head>
<body>

  <!-- ============ MENU ============ -->
  <section id="viewMenu">
    <h1>EMG · ESP32-C3</h1>
    <div class="sub">Escolha um modo</div>
    <div class="cards">
      <div class="card" onclick="irPara('fadiga')">
        <div class="ico">&#128170;</div>
        <div class="txt"><h2>Monitor de Fadiga</h2>
          <p>Amplitude do sinal (RMS) e nivel de fadiga ao vivo.</p></div>
      </div>
      <div class="card" onclick="irPara('jogo')">
        <div class="ico">&#129430;</div>
        <div class="txt"><h2>Jogo do Dino</h2>
          <p>Contraia o musculo para o dinossauro pular.</p></div>
      </div>
      <div class="card" onclick="irPara('osc')">
        <div class="ico">&#12336;&#65039;</div>
        <div class="txt"><h2>Osciloscopio</h2>
          <p>Sinal EMG bruto rolando em tempo real.</p></div>
      </div>
    </div>
  </section>

  <!-- ============ FADIGA ============ -->
  <section id="viewFadiga" class="hidden">
    <div class="topbar">
      <button class="btn-voltar" onclick="irPara('menu')">&#8592; Menu</button>
      <h1>Monitor de Fadiga</h1>
    </div>
    <div id="statusFad" class="status aviso">Conectando...</div>
    <div class="botoes">
      <button class="btn btn-iniciar"  onclick="fetch('/fad_iniciar')">INICIAR</button>
      <button class="btn btn-calibrar" onclick="fetch('/fad_calibrar')">CALIBRAR</button>
      <button class="btn btn-parar"    onclick="fetch('/fad_parar')">PARAR</button>
      <button class="btn btn-reset"    onclick="fetch('/fad_reset')">RESET</button>
    </div>
    <div class="metricas">
      <div class="metrica"><div class="rotulo">RMS Atual</div><div class="valor" id="rmsValor">--</div></div>
      <div class="metrica"><div class="rotulo">Baseline</div><div class="valor" id="baselineValor">--</div></div>
      <div class="metrica"><div class="rotulo">Fadiga</div><div class="valor" id="fadigaValor">--%</div></div>
    </div>
    <div class="caixa"><h3>RMS ao longo do tempo</h3><canvas id="graficoRMS"></canvas></div>
    <div class="caixa">
      <div class="fadiga-label"><span style="color:#888;font-size:0.85em">Fadiga Muscular</span>
        <span class="porcent" id="fadigaTexto">--%</span></div>
      <div class="barra-fundo"><div class="barra-preenchida" id="fadigaBarra" style="width:0%;background:#16c784;"></div></div>
    </div>
    <div class="caixa"><h3>Fadiga (%)</h3><canvas id="graficoFadiga"></canvas></div>
  </section>

  <!-- ============ JOGO ============ -->
  <section id="viewJogo" class="hidden">
    <div class="topbar">
      <button class="btn-voltar" onclick="irPara('menu')">&#8592; Menu</button>
      <h1>Jogo do Dino</h1>
    </div>
    <div id="statusJogo" class="status aviso">Conectando...</div>
    <canvas id="jogo" width="720" height="220"></canvas>
    <div class="botoes">
      <button class="btn btn-calibrar" onclick="fetch('/jog_calibrar')">CALIBRAR REPOUSO</button>
      <button class="btn btn-cinza"    onclick="fetch('/jog_sens?d=1')">- SENSIVEL</button>
      <button class="btn btn-cinza"    onclick="fetch('/jog_sens?d=-1')">+ SENSIVEL</button>
    </div>
    <div class="caixa" style="margin-bottom:8px">
      <div class="medidor-topo"><span>Sinal muscular (RMS)</span><span id="limiarTxt">limiar --</span></div>
      <div class="medidor"><div id="barraRms"></div><div id="marcaLimiar" style="left:0%"></div></div>
    </div>
    <div class="info">Barra vermelha = limiar de pulo. (Espaco tambem pula, para testar.)</div>
  </section>

  <!-- ============ OSCILOSCOPIO ============ -->
  <section id="viewOsc" class="hidden">
    <div class="topbar">
      <button class="btn-voltar" onclick="irPara('menu')">&#8592; Menu</button>
      <h1>Osciloscopio</h1>
    </div>
    <div id="statusOsc" class="status aviso">Conectando...</div>
    <canvas id="scope" width="720" height="260"></canvas>
    <div class="info">Sinal EMG bruto (ADC 0-4095). Contraia para ver a atividade aumentar.</div>
  </section>

  <script>
    // ---------------- Navegacao entre views ----------------
    let view = 'menu';
    let poll = null;

    function mostrar(id) {
      ['viewMenu','viewFadiga','viewJogo','viewOsc'].forEach(v =>
        document.getElementById(v).classList.toggle('hidden', v !== id));
    }

    function irPara(v) {
      view = v;
      const m = (v === 'fadiga') ? 'fadiga' : (v === 'jogo') ? 'jogo' : (v === 'osc') ? 'osc' : 'menu';
      fetch('/modo?m=' + m).catch(() => {});
      mostrar(v === 'menu' ? 'viewMenu' :
              v === 'fadiga' ? 'viewFadiga' :
              v === 'jogo' ? 'viewJogo' : 'viewOsc');
      if (poll) { clearInterval(poll); poll = null; }
      if (v === 'fadiga') poll = setInterval(pollFadiga, 250);
      else if (v === 'jogo') poll = setInterval(pollJogo, 50);
      else if (v === 'osc') poll = setInterval(pollOsc, 60);
    }

    function statusEletrodo(el, ok) {
      if (ok) return false;
      el.className = 'status erro'; el.textContent = 'Eletrodo solto!';
      return true;
    }

    // ---------------- FADIGA ----------------
    const maxPontos = 200;
    const dadosRMS = [], dadosFadiga = [];

    function corFadiga(f) { return f > 70 ? '#ea3943' : f > 40 ? '#fbbf24' : '#16c784'; }

    function desenharGrafico(id, dados, cor, maxY) {
      const c = document.getElementById(id), ctx = c.getContext('2d');
      c.width = c.offsetWidth * 2; c.height = c.offsetHeight * 2; ctx.scale(2, 2);
      const w = c.offsetWidth, h = c.offsetHeight;
      ctx.clearRect(0, 0, w, h);
      ctx.strokeStyle = '#2a2a3e'; ctx.lineWidth = 0.5;
      for (let i = 0; i < 4; i++) { const y = (h / 4) * i; ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(w, y); ctx.stroke(); }
      if (dados.length < 2) return;
      const escY = maxY || Math.max(...dados, 1);
      ctx.strokeStyle = cor; ctx.lineWidth = 1.5; ctx.beginPath();
      for (let i = 0; i < dados.length; i++) {
        const x = (i / (maxPontos - 1)) * w;
        const y = h - (dados[i] / escY) * h * 0.9 - h * 0.05;
        i ? ctx.lineTo(x, y) : ctx.moveTo(x, y);
      }
      ctx.stroke();
      ctx.fillStyle = '#888'; ctx.font = '11px monospace';
      ctx.fillText(dados[dados.length - 1].toFixed(1), w - 44, 14);
    }

    function pollFadiga() {
      fetch('/dados').then(r => r.json()).then(d => {
        const sb = document.getElementById('statusFad');
        if (!statusEletrodo(sb, d.eletrodo)) {
          if (d.estado === 'calibrando') { sb.className = 'status aviso'; sb.textContent = 'Calibrando... ' + d.progCal + '%  (mantenha a forca)'; }
          else if (d.estado === 'monitorando') { sb.className = 'status ok'; sb.textContent = 'Monitorando'; }
          else if (d.estado === 'capturando') { sb.className = 'status aviso'; sb.textContent = 'Segure a forca e clique CALIBRAR'; }
          else { sb.className = 'status aviso'; sb.textContent = d.msg || 'Parado'; }
        }
        document.getElementById('rmsValor').textContent = d.rms.toFixed(0);
        document.getElementById('baselineValor').textContent = d.baseline > 0 ? d.baseline.toFixed(0) : '--';
        document.getElementById('fadigaValor').textContent = d.fadiga.toFixed(0) + '%';
        const f = d.fadiga;
        document.getElementById('fadigaTexto').textContent = f.toFixed(0) + '%';
        const barra = document.getElementById('fadigaBarra');
        barra.style.width = f + '%'; barra.style.background = corFadiga(f);
        dadosRMS.push(d.rms); if (dadosRMS.length > maxPontos) dadosRMS.shift();
        dadosFadiga.push(d.fadiga); if (dadosFadiga.length > maxPontos) dadosFadiga.shift();
        desenharGrafico('graficoRMS', dadosRMS, '#7c8aff', 0);
        desenharGrafico('graficoFadiga', dadosFadiga, corFadiga(f), 100);
      }).catch(() => { const sb = document.getElementById('statusFad'); sb.className = 'status erro'; sb.textContent = 'Sem conexao'; });
    }

    // ---------------- JOGO ----------------
    const cv = document.getElementById('jogo'), gtx = cv.getContext('2d');
    const GW = cv.width, GH = cv.height, CHAO = GH - 30;
    let dino, obst, vel, score, best = 0, gameOver, spawnT;
    function resetJogo() { dino = { x: 60, y: CHAO, vy: 0, no_chao: true }; obst = []; vel = 3.5; score = 0; gameOver = false; spawnT = 0; }
    resetJogo();
    function pular() { if (gameOver) { resetJogo(); return; } if (dino.no_chao) { dino.vy = -14; dino.no_chao = false; } }
    document.addEventListener('keydown', e => {
      if (view === 'jogo' && (e.code === 'Space' || e.code === 'ArrowUp')) { e.preventDefault(); pular(); }
    });
    function spawn() { obst.push({ x: GW + 10, w: 14 + Math.random() * 12, h: 20 + Math.random() * 16 }); }
    function passo() {
      requestAnimationFrame(passo);
      if (view !== 'jogo') return;
      gtx.clearRect(0, 0, GW, GH);
      gtx.strokeStyle = '#3a3a5e'; gtx.lineWidth = 2;
      gtx.beginPath(); gtx.moveTo(0, CHAO + 2); gtx.lineTo(GW, CHAO + 2); gtx.stroke();
      dino.vy += 0.6; dino.y += dino.vy;
      if (dino.y >= CHAO) { dino.y = CHAO; dino.vy = 0; dino.no_chao = true; }
      gtx.font = '30px serif'; gtx.textBaseline = 'bottom';
      gtx.fillText('\u{1F996}', dino.x - 4, dino.y + 6);
      if (!gameOver) { vel += 0.0012; spawnT--; if (spawnT <= 0) { spawn(); spawnT = 85 + Math.random() * 70; } score++; }
      gtx.fillStyle = '#16c784';
      for (let i = obst.length - 1; i >= 0; i--) {
        const o = obst[i];
        if (!gameOver) o.x -= vel;
        gtx.fillRect(o.x, CHAO - o.h + 6, o.w, o.h);
        const dx = dino.x - 2, dw = 26, dtop = dino.y - 24;
        if (dx < o.x + o.w && dx + dw > o.x && dtop + 30 > CHAO - o.h + 6) { if (!gameOver) { gameOver = true; best = Math.max(best, score); } }
        if (o.x + o.w < 0) obst.splice(i, 1);
      }
      gtx.fillStyle = '#888'; gtx.font = '14px monospace'; gtx.textBaseline = 'top';
      gtx.fillText('Pontos: ' + score + '   Recorde: ' + best, 12, 10);
      if (gameOver) {
        gtx.fillStyle = '#f87171'; gtx.font = 'bold 22px sans-serif';
        gtx.textBaseline = 'middle'; gtx.textAlign = 'center';
        gtx.fillText('GAME OVER - contraia para reiniciar', GW / 2, GH / 2 - 10);
        gtx.textAlign = 'left';
      }
    }
    requestAnimationFrame(passo);

    let ultimoPulos = -1;
    function pollJogo() {
      fetch('/dados').then(r => r.json()).then(d => {
        const sb = document.getElementById('statusJogo');
        if (!statusEletrodo(sb, d.eletrodo)) {
          if (d.estado === 'calibrando') { sb.className = 'status aviso'; sb.textContent = 'Calibrando repouso... relaxe o braco'; }
          else if (d.estado === 'jogando') { sb.className = 'status ok'; sb.textContent = 'Pronto! Contraia para pular'; }
          else { sb.className = 'status aviso'; sb.textContent = 'Relaxe e clique CALIBRAR REPOUSO'; }
        }
        if (ultimoPulos < 0) ultimoPulos = d.pulos;
        if (d.pulos > ultimoPulos) { pular(); ultimoPulos = d.pulos; }
        const esc = Math.max(d.limiar * 1.6, 200);
        document.getElementById('barraRms').style.width = Math.min(100, d.rmsJ / esc * 100) + '%';
        document.getElementById('marcaLimiar').style.left = Math.min(100, d.limiar / esc * 100) + '%';
        document.getElementById('limiarTxt').textContent = 'limiar ' + d.limiar.toFixed(0) + ' (rep ' + d.rep.toFixed(0) + ')';
      }).catch(() => { const sb = document.getElementById('statusJogo'); sb.className = 'status erro'; sb.textContent = 'Sem conexao'; });
    }

    // ---------------- OSCILOSCOPIO ----------------
    function pollOsc() {
      fetch('/scope').then(r => r.json()).then(d => {
        const sb = document.getElementById('statusOsc');
        if (!statusEletrodo(sb, d.eletrodo)) { sb.className = 'status ok'; sb.textContent = 'Lendo sinal ao vivo'; }
        desenharScope(d.s);
      }).catch(() => { const sb = document.getElementById('statusOsc'); sb.className = 'status erro'; sb.textContent = 'Sem conexao'; });
    }
    function desenharScope(s) {
      const c = document.getElementById('scope'), ctx = c.getContext('2d');
      const w = c.width, h = c.height;
      ctx.clearRect(0, 0, w, h);
      ctx.strokeStyle = '#2a2a3e'; ctx.lineWidth = 1;
      for (let i = 0; i <= 4; i++) { const y = (h / 4) * i; ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(w, y); ctx.stroke(); }
      if (!s || s.length < 2) return;
      let mn = Math.min(...s), mx = Math.max(...s);
      if (mx - mn < 40) { const c0 = (mx + mn) / 2; mn = c0 - 20; mx = c0 + 20; }
      const pad = (mx - mn) * 0.1; mn -= pad; mx += pad;
      ctx.strokeStyle = '#16c784'; ctx.lineWidth = 1.5; ctx.beginPath();
      for (let i = 0; i < s.length; i++) {
        const x = (i / (s.length - 1)) * w;
        const y = h - ((s[i] - mn) / (mx - mn)) * h;
        i ? ctx.lineTo(x, y) : ctx.moveTo(x, y);
      }
      ctx.stroke();
      ctx.fillStyle = '#888'; ctx.font = '12px monospace';
      ctx.fillText('min ' + Math.round(mn) + '  max ' + Math.round(mx), 10, 16);
    }

    // arranca no menu
    irPara('menu');
  </script>
</body>
</html>
)rawliteral";

// ===================== CALLBACK DO TIMER ====================
void IRAM_ATTR callbackTimer(void* arg) {
  if (digitalRead(PINO_LO_PLUS) || digitalRead(PINO_LO_MINUS)) {
    amostraAtual = 0;
    eletrodoOK = false;
  } else {
    amostraAtual = analogRead(PINO_EMG);
    eletrodoOK = true;
  }
  novaAmostra = true;
}

// ===================== FUNCOES DE SINAL =====================
float calcularRMS(int* buf, int n) {
  float media = 0;
  for (int i = 0; i < n; i++) media += buf[i];
  media /= n;
  float soma = 0;
  for (int i = 0; i < n; i++) { float d = buf[i] - media; soma += d * d; }
  return sqrt(soma / n);
}

float suavizarRMS(float valor) {
  bufferMedia[indiceMedia] = valor;
  indiceMedia = (indiceMedia + 1) % TAMANHO_MEDIA;
  if (indiceMedia == 0) mediaCheia = true;
  int n = mediaCheia ? TAMANHO_MEDIA : indiceMedia;
  float soma = 0;
  for (int i = 0; i < n; i++) soma += bufferMedia[i];
  return soma / n;
}

float calcularMediana(float* v, int n) {
  float tmp[JANELAS_CALIBRACAO];
  for (int i = 0; i < n; i++) tmp[i] = v[i];
  for (int i = 1; i < n; i++) {
    float chave = tmp[i]; int k = i - 1;
    while (k >= 0 && tmp[k] > chave) { tmp[k + 1] = tmp[k]; k--; }
    tmp[k + 1] = chave;
  }
  if (n % 2) return tmp[n / 2];
  return (tmp[n / 2 - 1] + tmp[n / 2]) / 2.0;
}

float coefVariacao(float* v, int n) {
  float media = 0;
  for (int i = 0; i < n; i++) media += v[i];
  media /= n;
  if (media <= 0) return 999;
  float soma = 0;
  for (int i = 0; i < n; i++) { float d = v[i] - media; soma += d * d; }
  return sqrt(soma / n) / media;
}

void resetMediaMovel() { indiceMedia = 0; mediaCheia = false; }

void recalcularLimiares() {
  float margem = limiarDelta + 2.0 * repDesvio;
  limiarAlto  = repMedia + margem;
  limiarBaixo = repMedia + margem * 0.4;
}

// ===================== TROCA DE MODO ========================
void entrarModo(Modo m) {
  modo = m;
  if (m == FADIGA) {
    estadoFad = FAD_CAPTURANDO;   // ja mostra RMS ao vivo ao entrar
    rmsBaseline = 0; fadigaPct = 0; idxFad = 0;
    resetMediaMovel();
    msgFad = "Capturando";
  } else if (m == JOGO) {
    estadoJogo = JOGO_PARADO;
    idxJogo = 0; contPulos = 0; armado = true;
  } else if (m == OSC) {
    scopePos = 0;
    for (int i = 0; i < N_SCOPE; i++) scopeBuf[i] = 0;
  }
}

// ===================== WIFI + ROTAS =========================
void conectarWiFi() {
  display.clearBuffer();
  display.setFont(u8g2_font_5x7_tr);
  display.drawStr(0, 8, "Conectando");
  display.drawStr(0, 18, "WiFi...");
  display.sendBuffer();

  WiFi.begin(SSID, SENHA);
  Serial.print("Conectando WiFi");
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 40) { delay(500); Serial.print("."); tentativas++; }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("Conectado! IP: ");
    Serial.println(WiFi.localIP());
    display.clearBuffer();
    display.drawStr(0, 8, "WiFi OK!");
    display.drawStr(0, 20, WiFi.localIP().toString().c_str());
    display.drawStr(0, 32, "Abra no PC");
    display.sendBuffer();
    delay(3000);
  } else {
    Serial.println("\nFalha no WiFi!");
    display.clearBuffer();
    display.drawStr(0, 8, "WiFi FALHOU");
    display.drawStr(0, 20, "Verifique");
    display.drawStr(0, 30, "SSID/senha");
    display.sendBuffer();
    delay(5000);
  }
}

void configurarRotas() {
  server.on("/", []() { server.send_P(200, "text/html", PAGINA_HTML); });

  server.on("/modo", []() {
    String m = server.arg("m");
    if (m == "fadiga") entrarModo(FADIGA);
    else if (m == "jogo") entrarModo(JOGO);
    else if (m == "osc") entrarModo(OSC);
    else entrarModo(MENU);
    server.send(200, "text/plain", "ok");
    Serial.print("[web] modo="); Serial.println(m);
  });

  // ---- Fadiga ----
  server.on("/fad_iniciar", []() {
    estadoFad = FAD_CAPTURANDO;
    rmsBaseline = 0; fadigaPct = 0; idxFad = 0;
    resetMediaMovel();
    msgFad = "Capturando";
    server.send(200, "text/plain", "ok");
  });
  server.on("/fad_calibrar", []() {
    pedidoCalibrarFad = true;
    server.send(200, "text/plain", "ok");
  });
  server.on("/fad_parar", []() {
    estadoFad = FAD_PARADO; msgFad = "Parado";
    server.send(200, "text/plain", "ok");
  });
  server.on("/fad_reset", []() {
    estadoFad = FAD_PARADO;
    rmsBaseline = 0; fadigaPct = 0; rmsAtual = 0; rmsSuave = 0;
    for (int i = 0; i < LARGURA_GRAFICO; i++) historicoFadiga[i] = 0;
    resetMediaMovel();
    msgFad = "Resetado";
    server.send(200, "text/plain", "ok");
  });

  // ---- Jogo ----
  server.on("/jog_calibrar", []() {
    pedidoCalibrarJogo = true;
    server.send(200, "text/plain", "ok");
  });
  server.on("/jog_sens", []() {
    if (server.hasArg("d")) {
      int d = server.arg("d").toInt();
      limiarDelta += d * 15;
      if (limiarDelta < 15) limiarDelta = 15;
      if (limiarDelta > 400) limiarDelta = 400;
      recalcularLimiares();
    }
    server.send(200, "text/plain", "ok");
  });

  // ---- Dados (fadiga + jogo) ----
  server.on("/dados", []() {
    String estadoStr; int progCal = 0;
    if (modo == FADIGA) {
      switch (estadoFad) {
        case FAD_PARADO:      estadoStr = "parado"; break;
        case FAD_CAPTURANDO:  estadoStr = "capturando"; break;
        case FAD_CALIBRANDO:  estadoStr = "calibrando";
          progCal = (contCalibJanelas * 100) / JANELAS_CALIBRACAO; break;
        case FAD_MONITORANDO: estadoStr = "monitorando"; break;
      }
    } else {
      switch (estadoJogo) {
        case JOGO_PARADO:     estadoStr = "parado"; break;
        case JOGO_CALIBRANDO: estadoStr = "calibrando"; break;
        case JOGO_JOGANDO:    estadoStr = "jogando"; break;
      }
    }
    String json = "{";
    json += "\"eletrodo\":" + String(eletrodoOK ? "true" : "false");
    json += ",\"estado\":\"" + estadoStr + "\"";
    // fadiga
    json += ",\"rms\":" + String(rmsSuave, 1);
    json += ",\"baseline\":" + String(rmsBaseline, 1);
    json += ",\"fadiga\":" + String(fadigaPct, 1);
    json += ",\"progCal\":" + String(progCal);
    json += ",\"msg\":\"" + msgFad + "\"";
    // jogo
    json += ",\"rmsJ\":" + String(rmsCurto, 1);
    json += ",\"rep\":" + String(repMedia, 1);
    json += ",\"limiar\":" + String(limiarAlto, 1);
    json += ",\"pulos\":" + String(contPulos);
    json += "}";
    server.send(200, "application/json", json);
  });

  // ---- Scope (osciloscopio) ----
  server.on("/scope", []() {
    String json = "{\"s\":[";
    for (int i = 0; i < N_SCOPE; i++) {
      int idx = (scopePos + i) % N_SCOPE;
      if (i) json += ",";
      json += String(scopeBuf[idx]);
    }
    json += "],\"eletrodo\":" + String(eletrodoOK ? "true" : "false") + "}";
    server.send(200, "application/json", json);
  });

  server.begin();
  Serial.println("Servidor web iniciado");
}

// ===================== PROCESSAMENTO POR MODO ===============
void processarFadiga(int amostra, bool ok) {
  if (estadoFad == FAD_PARADO) return;

  bufFad[idxFad++] = amostra;
  if (idxFad < N_FAD) return;
  idxFad = 0;

  rmsAtual = calcularRMS(bufFad, N_FAD);
  rmsSuave = suavizarRMS(rmsAtual);

  switch (estadoFad) {
    case FAD_CAPTURANDO:
      if (pedidoCalibrarFad) {
        pedidoCalibrarFad = false;
        if (!ok || rmsAtual < RMS_MINIMO) {
          msgFad = "Contraia p/ calibrar";
        } else {
          estadoFad = FAD_CALIBRANDO;
          contAquecimento = 0; contCalibJanelas = 0;
          msgFad = "Calibrando";
        }
      }
      break;

    case FAD_CALIBRANDO:
      if (!ok) break;
      if (contAquecimento < JANELAS_AQUECIMENTO) { contAquecimento++; break; }
      if (rmsAtual < RMS_MINIMO) {
        estadoFad = FAD_CAPTURANDO; msgFad = "Forca solta, recomece"; break;
      }
      calibRMS[contCalibJanelas++] = rmsAtual;
      if (contCalibJanelas >= JANELAS_CALIBRACAO) {
        float cv = coefVariacao(calibRMS, JANELAS_CALIBRACAO);
        if (cv > CV_MAXIMO) {
          estadoFad = FAD_CAPTURANDO; msgFad = "Baseline instavel, recalibre"; break;
        }
        rmsBaseline = calcularMediana(calibRMS, JANELAS_CALIBRACAO);
        estadoFad = FAD_MONITORANDO; msgFad = "Monitorando";
      }
      break;

    case FAD_MONITORANDO:
      if (ok && rmsBaseline > 0) {
        float razao = rmsSuave / rmsBaseline;
        fadigaPct = constrain((razao - 1.0) * 100.0, 0, 100);
        historicoFadiga[posHistorico] = fadigaPct;
        posHistorico = (posHistorico + 1) % LARGURA_GRAFICO;
      }
      break;

    default: break;
  }
}

void processarJogo(int amostra, bool ok) {
  if (pedidoCalibrarJogo) {
    pedidoCalibrarJogo = false;
    estadoJogo = JOGO_CALIBRANDO;
    contRepouso = 0; idxJogo = 0;
  }
  if (estadoJogo == JOGO_PARADO) return;

  bufJogo[idxJogo++] = amostra;
  if (idxJogo < N_JOGO) return;
  idxJogo = 0;

  rmsCurto = calcularRMS(bufJogo, N_JOGO);

  if (estadoJogo == JOGO_CALIBRANDO) {
    if (ok) {
      repousoBuf[contRepouso++] = rmsCurto;
      if (contRepouso >= JANELAS_REPOUSO) {
        float soma = 0;
        for (int i = 0; i < JANELAS_REPOUSO; i++) soma += repousoBuf[i];
        repMedia = soma / JANELAS_REPOUSO;
        float sq = 0;
        for (int i = 0; i < JANELAS_REPOUSO; i++) { float dd = repousoBuf[i] - repMedia; sq += dd * dd; }
        repDesvio = sqrt(sq / JANELAS_REPOUSO);
        recalcularLimiares();
        armado = true;
        estadoJogo = JOGO_JOGANDO;
      }
    }
  } else if (estadoJogo == JOGO_JOGANDO) {
    if (ok) {
      if (armado && rmsCurto > limiarAlto) {
        contPulos++; armado = false; ultimoPuloMs = millis();
      } else if (!armado && rmsCurto < limiarBaixo) {
        armado = true;
      }
    }
  }
}

void processarScope(int amostra, bool ok) {
  scopeBuf[scopePos] = amostra;
  scopePos = (scopePos + 1) % N_SCOPE;
}

// ===================== OLED =================================
void atualizarOLED() {
  display.clearBuffer();
  display.setFont(u8g2_font_5x7_tr);

  if (modo == MENU) {
    display.drawStr(0, 8, WiFi.localIP().toString().c_str());
    display.drawStr(0, 22, "Escolha o");
    display.drawStr(0, 32, "modo no PC");
    display.sendBuffer();
    return;
  }

  bool ok = eletrodoOK;

  if (modo == FADIGA) {
    if (!ok && estadoFad != FAD_PARADO) { display.drawStr(5, 22, "ELETRODO SOLTO"); }
    else if (estadoFad == FAD_CALIBRANDO) {
      display.drawStr(0, 8, "Calibrando");
      int largura = map(contCalibJanelas, 0, JANELAS_CALIBRACAO, 0, 68);
      display.drawFrame(2, 14, 68, 8); display.drawBox(2, 14, largura, 8);
    } else if (estadoFad == FAD_MONITORANDO) {
      char buf[24];
      snprintf(buf, sizeof(buf), "RMS:%.0f", rmsSuave); display.drawStr(0, 8, buf);
      snprintf(buf, sizeof(buf), "F:%d%%", (int)fadigaPct); display.drawStr(42, 8, buf);
      display.drawFrame(0, 11, 72, 7);
      int larg = constrain((int)(fadigaPct * 70 / 100), 0, 70);
      display.drawBox(1, 12, larg, 5);
      int areaY = 21, areaH = 18;
      for (int i = 0; i < LARGURA_GRAFICO - 1; i++) {
        int idx1 = (posHistorico + i) % LARGURA_GRAFICO;
        int idx2 = (posHistorico + i + 1) % LARGURA_GRAFICO;
        int y1 = constrain(areaY + areaH - (int)((historicoFadiga[idx1] / 100.0) * areaH), areaY, areaY + areaH);
        int y2 = constrain(areaY + areaH - (int)((historicoFadiga[idx2] / 100.0) * areaH), areaY, areaY + areaH);
        display.drawLine(i, y1, i + 1, y2);
      }
    } else {
      char buf[24];
      snprintf(buf, sizeof(buf), "RMS:%.0f", rmsSuave); display.drawStr(0, 8, buf);
      display.drawStr(0, 22, "Segure a forca");
      display.drawStr(0, 34, "e CALIBRAR");
    }
  }

  else if (modo == JOGO) {
    if (!ok && estadoJogo != JOGO_PARADO) { display.drawStr(5, 22, "ELETRODO SOLTO"); }
    else if (estadoJogo == JOGO_CALIBRANDO) {
      display.drawStr(0, 8, "Calibrando");
      display.drawStr(0, 20, "repouso...");
      int largura = map(contRepouso, 0, JANELAS_REPOUSO, 0, 68);
      display.drawFrame(2, 26, 68, 8); display.drawBox(2, 26, largura, 8);
    } else if (estadoJogo == JOGO_JOGANDO) {
      char buf[24];
      snprintf(buf, sizeof(buf), "RMS:%.0f", rmsCurto); display.drawStr(0, 8, buf);
      snprintf(buf, sizeof(buf), "Lim:%.0f", limiarAlto); display.drawStr(0, 18, buf);
      snprintf(buf, sizeof(buf), "Pulos:%lu", contPulos); display.drawStr(0, 30, buf);
      if (millis() - ultimoPuloMs < 250) display.drawStr(48, 30, "^");
    } else {
      display.drawStr(0, 8, "Jogo do Dino");
      display.drawStr(0, 22, "Relaxe e");
      display.drawStr(0, 34, "CALIBRAR");
    }
  }

  else if (modo == OSC) {
    // desenha o sinal bruto ocupando os 72px, auto-escalado
    int mn = 4095, mx = 0;
    for (int i = 0; i < N_SCOPE; i++) { int v = scopeBuf[i]; if (v < mn) mn = v; if (v > mx) mx = v; }
    if (mx - mn < 40) { int c0 = (mx + mn) / 2; mn = c0 - 20; mx = c0 + 20; }
    for (int x = 0; x < 71; x++) {
      int i1 = (scopePos + (x * N_SCOPE / 72)) % N_SCOPE;
      int i2 = (scopePos + ((x + 1) * N_SCOPE / 72)) % N_SCOPE;
      int y1 = 39 - (int)((float)(scopeBuf[i1] - mn) / (mx - mn) * 38);
      int y2 = 39 - (int)((float)(scopeBuf[i2] - mn) / (mx - mn) * 38);
      y1 = constrain(y1, 1, 39); y2 = constrain(y2, 1, 39);
      display.drawLine(x, y1, x + 1, y2);
    }
  }

  display.sendBuffer();
}

// ===================== SETUP ===============================
void setup() {
  Serial.begin(115200);
  pinMode(PINO_LO_PLUS, INPUT);
  pinMode(PINO_LO_MINUS, INPUT);

  Wire.begin(PINO_SDA, PINO_SCL);
  display.begin();

  for (int i = 0; i < LARGURA_GRAFICO; i++) historicoFadiga[i] = 0;
  for (int i = 0; i < N_SCOPE; i++) scopeBuf[i] = 0;

  conectarWiFi();
  configurarRotas();

  esp_timer_handle_t timer;
  esp_timer_create_args_t args = {};
  args.callback = callbackTimer;
  args.name = "emg_timer";
  esp_timer_create(&args, &timer);
  esp_timer_start_periodic(timer, 1000);  // 1000Hz

  Serial.println("=== FASE 5D: Menu (Fadiga / Jogo / Osciloscopio) ===");
}

// ===================== LOOP ================================
unsigned long ultimoOLED = 0;

void loop() {
  server.handleClient();

  if (!novaAmostra) return;
  novaAmostra = false;

  int  amostra = amostraAtual;
  bool ok = eletrodoOK;

  switch (modo) {
    case FADIGA: processarFadiga(amostra, ok); break;
    case JOGO:   processarJogo(amostra, ok);   break;
    case OSC:    processarScope(amostra, ok);  break;
    default: break;  // MENU: ocioso
  }

  unsigned long intervalo = (modo == OSC) ? 80 : 150;
  if (millis() - ultimoOLED > intervalo) {
    ultimoOLED = millis();
    atualizarOLED();
  }
}
