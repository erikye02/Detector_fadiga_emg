// ============================================================
// FASE 4 — Deteccao de fadiga com OLED embutido
// ============================================================
// Placa: ESP32-C3 com OLED 72x40 embutido
// Objetivo: calibracao automatica + deteccao de fadiga em
//           tempo real com barra de progresso na tela
// Biblioteca: U8g2
// ============================================================

#include <U8g2lib.h>
#include <Wire.h>
#include <esp_timer.h>
#include <math.h>

// --- Pinos ---
#define PINO_EMG      0
#define PINO_LO_PLUS  2
#define PINO_LO_MINUS 1
#define PINO_SDA      5
#define PINO_SCL      6

// --- Display ---
U8G2_SSD1306_72X40_ER_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);

// --- Timer ---
volatile int amostraAtual = 0;
volatile bool novaAmostra = false;
volatile bool eletrodoOK = true;

// --- RMS ---
#define TAMANHO_JANELA 256
int amostras[TAMANHO_JANELA];
int indiceAmostra = 0;
float rmsAtual = 0;

// --- Fadiga ---
enum Estado { AGUARDANDO, CALIBRANDO, MONITORANDO };
Estado estado = AGUARDANDO;

float rmsBaseline = 0;
#define JANELAS_AQUECIMENTO 4   // ~1s descartado (rampa de subida da forca)
#define JANELAS_CALIBRACAO 24   // 24 janelas x 256ms = ~6 segundos
#define CV_MAXIMO 0.35          // coef. de variacao maximo aceito no baseline
float calibRMS[JANELAS_CALIBRACAO];
int contAquecimento = 0;
int contCalibracaoJanelas = 0;

float fadigaPorcentagem = 0;

// --- Historico para grafico ---
#define LARGURA_GRAFICO 72
float historicoFadiga[LARGURA_GRAFICO];
int posHistorico = 0;

// --- Tempo ---
unsigned long tempoInicio = 0;

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

float calcularRMS(int* buf, int n) {
  float media = 0;
  for (int i = 0; i < n; i++) media += buf[i];
  media /= n;

  float soma = 0;
  for (int i = 0; i < n; i++) {
    float diff = buf[i] - media;
    soma += diff * diff;
  }
  return sqrt(soma / n);
}

// Mediana: robusta a picos/artefatos isolados no baseline
float calcularMediana(float* v, int n) {
  float tmp[JANELAS_CALIBRACAO];
  for (int i = 0; i < n; i++) tmp[i] = v[i];
  for (int i = 1; i < n; i++) {
    float chave = tmp[i];
    int j = i - 1;
    while (j >= 0 && tmp[j] > chave) { tmp[j + 1] = tmp[j]; j--; }
    tmp[j + 1] = chave;
  }
  if (n % 2) return tmp[n / 2];
  return (tmp[n / 2 - 1] + tmp[n / 2]) / 2.0;
}

// Coeficiente de variacao (desvio/media): mede a estabilidade do baseline
float coefVariacao(float* v, int n) {
  float media = 0;
  for (int i = 0; i < n; i++) media += v[i];
  media /= n;
  if (media <= 0) return 999;

  float soma = 0;
  for (int i = 0; i < n; i++) {
    float d = v[i] - media;
    soma += d * d;
  }
  return sqrt(soma / n) / media;
}

void desenharTelaAguardando() {
  display.clearBuffer();
  display.setFont(u8g2_font_5x7_tr);
  display.drawStr(0, 8,  "EMG Fadiga");
  display.drawStr(0, 20, "Contraia o");
  display.drawStr(0, 30, "musculo p/");
  display.drawStr(0, 40, "calibrar...");
  display.sendBuffer();
}

void desenharTelaCalibrando(int progresso) {
  display.clearBuffer();
  display.setFont(u8g2_font_5x7_tr);
  display.drawStr(0, 8, "CALIBRANDO");

  // Barra de progresso da calibracao
  int larguraBarra = map(progresso, 0, JANELAS_CALIBRACAO, 0, 68);
  display.drawFrame(2, 14, 68, 10);
  display.drawBox(2, 14, larguraBarra, 10);

  char buf[20];
  snprintf(buf, sizeof(buf), "%d%%", (progresso * 100) / JANELAS_CALIBRACAO);
  display.drawStr(25, 38, buf);
  display.sendBuffer();
}

void desenharTelaMonitorando() {
  display.clearBuffer();
  display.setFont(u8g2_font_5x7_tr);

  // Linha 1: RMS atual
  char buf[24];
  snprintf(buf, sizeof(buf), "RMS:%.0f", rmsAtual);
  display.drawStr(0, 8, buf);

  // Linha 1 direita: fadiga %
  snprintf(buf, sizeof(buf), "F:%d%%", (int)fadigaPorcentagem);
  display.drawStr(42, 8, buf);

  // Barra de fadiga (y=11 ate y=18)
  display.drawFrame(0, 11, 72, 7);
  int largura = constrain((int)(fadigaPorcentagem * 70 / 100), 0, 70);
  display.drawBox(1, 12, largura, 5);

  // Grafico de historico de fadiga (y=21 ate y=39)
  int areaY = 21;
  int areaH = 18;
  for (int i = 0; i < LARGURA_GRAFICO - 1; i++) {
    int idx1 = (posHistorico + i) % LARGURA_GRAFICO;
    int idx2 = (posHistorico + i + 1) % LARGURA_GRAFICO;
    int y1 = areaY + areaH - (int)((historicoFadiga[idx1] / 100.0) * areaH);
    int y2 = areaY + areaH - (int)((historicoFadiga[idx2] / 100.0) * areaH);
    y1 = constrain(y1, areaY, areaY + areaH);
    y2 = constrain(y2, areaY, areaY + areaH);
    display.drawLine(i, y1, i + 1, y2);
  }

  display.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  pinMode(PINO_LO_PLUS, INPUT);
  pinMode(PINO_LO_MINUS, INPUT);

  Wire.begin(PINO_SDA, PINO_SCL);
  display.begin();

  for (int i = 0; i < LARGURA_GRAFICO; i++) historicoFadiga[i] = 0;

  desenharTelaAguardando();

  // Timer
  esp_timer_handle_t timer;
  esp_timer_create_args_t args = {};
  args.callback = callbackTimer;
  args.name = "emg_timer";
  esp_timer_create(&args, &timer);
  esp_timer_start_periodic(timer, 1000);

  delay(1000);
  Serial.println("=== FASE 4: Deteccao de Fadiga ===");
  Serial.println("Contraia o musculo para iniciar a calibracao");
}

void loop() {
  if (!novaAmostra) return;
  novaAmostra = false;

  // Acumula amostra
  amostras[indiceAmostra] = amostraAtual;
  indiceAmostra++;

  if (indiceAmostra < TAMANHO_JANELA) return;
  indiceAmostra = 0;

  rmsAtual = calcularRMS(amostras, TAMANHO_JANELA);

  switch (estado) {

    case AGUARDANDO:
      // Espera o usuario comecar a contrair (RMS sobe acima de um minimo)
      if (eletrodoOK && rmsAtual > 30) {
        estado = CALIBRANDO;
        contAquecimento = 0;
        contCalibracaoJanelas = 0;
        tempoInicio = millis();
        Serial.println("Contracao detectada! Estabilize a forca...");
      }
      break;

    case CALIBRANDO:
      if (eletrodoOK) {
        // 1) Descarta a rampa de subida da forca antes de medir o baseline
        if (contAquecimento < JANELAS_AQUECIMENTO) {
          contAquecimento++;
          desenharTelaCalibrando(0);
          break;
        }

        // Se a forca caiu, o usuario soltou: aborta e volta a aguardar
        if (rmsAtual < 30) {
          estado = AGUARDANDO;
          Serial.println("Contracao interrompida. Recomece.");
          desenharTelaAguardando();
          break;
        }

        // 2) Coleta as janelas ja com forca estabilizada
        calibRMS[contCalibracaoJanelas] = rmsAtual;
        contCalibracaoJanelas++;
        desenharTelaCalibrando(contCalibracaoJanelas);

        Serial.print("Calibrando... janela ");
        Serial.print(contCalibracaoJanelas);
        Serial.print("/");
        Serial.print(JANELAS_CALIBRACAO);
        Serial.print(" RMS=");
        Serial.println(rmsAtual);

        if (contCalibracaoJanelas >= JANELAS_CALIBRACAO) {
          // 3) Checa a qualidade: baseline instavel = medida de fadiga sem valor
          float cv = coefVariacao(calibRMS, JANELAS_CALIBRACAO);
          if (cv > CV_MAXIMO) {
            estado = AGUARDANDO;
            Serial.print("Baseline instavel (CV=");
            Serial.print(cv);
            Serial.println("). Recalibrar com forca constante.");

            display.clearBuffer();
            display.setFont(u8g2_font_5x7_tr);
            display.drawStr(0, 12, "Instavel!");
            display.drawStr(0, 24, "Recalibrar");
            display.drawStr(0, 36, "forca estavel");
            display.sendBuffer();
            delay(2000);
            desenharTelaAguardando();
            break;
          }

          // Mediana no lugar da media: robusta a picos isolados
          rmsBaseline = calcularMediana(calibRMS, JANELAS_CALIBRACAO);
          estado = MONITORANDO;
          Serial.print("Calibracao completa! Baseline (mediana) RMS = ");
          Serial.print(rmsBaseline);
          Serial.print(" | CV=");
          Serial.println(cv);
          Serial.println("Monitorando fadiga... mantenha o exercicio");
        }
      }
      break;

    case MONITORANDO:
      if (eletrodoOK && rmsBaseline > 0) {
        // Fadiga (contracao submaxima sustentada): o RMS SOBE acima do baseline
        // conforme o musculo recruta mais fibras pra manter a mesma forca.
        float razao = rmsAtual / rmsBaseline;
        fadigaPorcentagem = (razao - 1.0) * 100.0;
        fadigaPorcentagem = constrain(fadigaPorcentagem, 0, 100);

        // Guarda no historico
        historicoFadiga[posHistorico] = fadigaPorcentagem;
        posHistorico = (posHistorico + 1) % LARGURA_GRAFICO;

        // Serial
        Serial.print("RMS:");
        Serial.print(rmsAtual);
        Serial.print(" Baseline:");
        Serial.print(rmsBaseline);
        Serial.print(" Fadiga:");
        Serial.print(fadigaPorcentagem);
        Serial.println("%");

        desenharTelaMonitorando();
      } else if (!eletrodoOK) {
        display.clearBuffer();
        display.setFont(u8g2_font_5x7_tr);
        display.drawStr(5, 22, "ELETRODO SOLTO");
        display.sendBuffer();
      }
      break;
  }
}
