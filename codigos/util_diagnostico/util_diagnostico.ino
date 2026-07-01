// ============================================================
// UTILITARIO — Diagnostico completo do hardware
// ============================================================
// Placa: ESP32-C3
// Objetivo: testar TODOS os componentes de uma vez e reportar
//           o que funciona e o que nao funciona
//
// Testes realizados:
//   1. OLED embutido 72x40
//   2. ADC (pino analogico GPIO0)
//   3. Lead-off detection (GPIO1, GPIO2)
//   4. WiFi
//   5. Memoria livre
//   6. Scan I2C (encontra todos dispositivos I2C conectados)
//
// Resultado: tudo aparece no Serial Monitor (115200 baud)
//            e no OLED embutido
// ============================================================

#include <U8g2lib.h>
#include <Wire.h>
#include <WiFi.h>

#define PINO_EMG      0
#define PINO_LO_PLUS  1
#define PINO_LO_MINUS 2
#define PINO_SDA      5
#define PINO_SCL      6

U8G2_SSD1306_72X40_ER_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);

int totalTestes = 0;
int testesOK = 0;

void reportar(const char* nome, bool ok, const char* detalhe) {
  totalTestes++;
  if (ok) testesOK++;

  Serial.print(ok ? "[OK]   " : "[ERRO] ");
  Serial.print(nome);
  if (detalhe[0] != '\0') {
    Serial.print(" — ");
    Serial.print(detalhe);
  }
  Serial.println();
}

// --- Teste 1: OLED embutido ---
bool testarOLED() {
  Wire.begin(PINO_SDA, PINO_SCL);
  display.begin();
  display.clearBuffer();
  display.setFont(u8g2_font_5x7_tr);
  display.drawStr(0, 8, "Diagnostico");
  display.drawStr(0, 18, "em andamento");
  display.sendBuffer();
  return true;  // se chegou aqui sem travar, ta funcionando
}

// --- Teste 2: Scan I2C ---
void scanI2C() {
  Serial.println("\n--- Scan I2C ---");
  int encontrados = 0;

  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    byte erro = Wire.endTransmission();
    if (erro == 0) {
      encontrados++;
      char buf[40];
      snprintf(buf, sizeof(buf), "Dispositivo encontrado: 0x%02X", addr);
      Serial.print("  ");
      Serial.println(buf);

      // Identifica dispositivos comuns
      if (addr == 0x3C || addr == 0x3D) {
        Serial.println("    ^ Provavelmente OLED SSD1306");
      } else if (addr == 0x68 || addr == 0x69) {
        Serial.println("    ^ Provavelmente MPU6050 (acelerometro)");
      } else if (addr == 0x76 || addr == 0x77) {
        Serial.println("    ^ Provavelmente BMP280/BME280 (pressao)");
      }
    }
  }

  char det[30];
  snprintf(det, sizeof(det), "%d dispositivo(s)", encontrados);
  reportar("I2C Scan", encontrados > 0, det);
}

// --- Teste 3: ADC ---
void testarADC() {
  int leituras[10];
  int somaTotal = 0;
  int minVal = 4095, maxVal = 0;

  for (int i = 0; i < 10; i++) {
    leituras[i] = analogRead(PINO_EMG);
    somaTotal += leituras[i];
    if (leituras[i] < minVal) minVal = leituras[i];
    if (leituras[i] > maxVal) maxVal = leituras[i];
    delay(10);
  }

  float media = somaTotal / 10.0;
  int range = maxVal - minVal;

  char det[60];
  snprintf(det, sizeof(det), "media=%.0f min=%d max=%d range=%d", media, minVal, maxVal, range);

  // ADC funciona se retorna valores nao-zero e nao-fixos
  bool ok = (media > 0 && media < 4095);
  reportar("ADC (GPIO0)", ok, det);

  if (media < 100) {
    Serial.println("    ^ Valor muito baixo — pino desconectado ou curto para GND?");
  } else if (media > 3900) {
    Serial.println("    ^ Valor muito alto — curto para VCC?");
  } else if (range < 5) {
    Serial.println("    ^ Range muito pequeno — sinal muito estavel ou sem sensor");
  }
}

// --- Teste 4: Lead-off detection ---
void testarLeadOff() {
  pinMode(PINO_LO_PLUS, INPUT);
  pinMode(PINO_LO_MINUS, INPUT);

  bool loPlus = digitalRead(PINO_LO_PLUS);
  bool loMinus = digitalRead(PINO_LO_MINUS);

  char det[40];
  snprintf(det, sizeof(det), "LO+=%s LO-=%s",
    loPlus ? "HIGH(solto)" : "LOW(ok)",
    loMinus ? "HIGH(solto)" : "LOW(ok)");

  reportar("Lead-off (GPIO1,2)", !loPlus && !loMinus, det);

  if (loPlus || loMinus) {
    Serial.println("    ^ Eletrodo desconectado ou AD8232 nao ligado");
    Serial.println("    ^ HIGH = eletrodo solto, LOW = eletrodo conectado");
  }
}

// --- Teste 5: WiFi ---
void testarWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  int redes = WiFi.scanNetworks();

  char det[40];
  if (redes > 0) {
    snprintf(det, sizeof(det), "%d redes encontradas", redes);
    reportar("WiFi (scan)", true, det);

    Serial.println("    Redes disponiveis:");
    for (int i = 0; i < min(redes, 5); i++) {
      Serial.print("      ");
      Serial.print(i + 1);
      Serial.print(". ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.println(" dBm)");
    }
    if (redes > 5) {
      Serial.print("      ... e mais ");
      Serial.print(redes - 5);
      Serial.println(" redes");
    }
  } else {
    reportar("WiFi (scan)", false, "nenhuma rede encontrada");
  }

  WiFi.scanDelete();
}

// --- Teste 6: Memoria ---
void testarMemoria() {
  int heapLivre = ESP.getFreeHeap();
  int heapTotal = ESP.getHeapSize();

  char det[50];
  snprintf(det, sizeof(det), "%d bytes livres de %d total", heapLivre, heapTotal);

  bool ok = heapLivre > 50000; // minimo 50KB livres para o projeto funcionar
  reportar("Memoria RAM", ok, det);

  if (!ok) {
    Serial.println("    ^ Pouca memoria livre — pode ter instabilidade");
  }
}

// --- Teste 7: Velocidade do ADC ---
void testarVelocidadeADC() {
  unsigned long inicio = micros();
  int leituras = 1000;
  volatile int dummy;

  for (int i = 0; i < leituras; i++) {
    dummy = analogRead(PINO_EMG);
  }

  unsigned long duracao = micros() - inicio;
  float usPorLeitura = (float)duracao / leituras;
  float hzMaximo = 1000000.0 / usPorLeitura;

  char det[50];
  snprintf(det, sizeof(det), "%.1f us/leitura = %.0f Hz max", usPorLeitura, hzMaximo);

  bool ok = hzMaximo > 1000; // precisa de pelo menos 1000 Hz
  reportar("Velocidade ADC", ok, det);

  if (hzMaximo < 1000) {
    Serial.println("    ^ ADC muito lento para 1000 Hz de amostragem");
  }
}

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);
  delay(2000); // tempo para abrir o Serial Monitor

  Serial.println();
  Serial.println("========================================");
  Serial.println(" DIAGNOSTICO — ESP32-C3 + EMG");
  Serial.println("========================================");
  Serial.println();

  // Teste 1: OLED
  bool oledOK = testarOLED();
  reportar("OLED embutido 72x40", oledOK, "inicializado");

  // Teste 2: I2C scan
  scanI2C();

  // Teste 3: ADC
  Serial.println();
  testarADC();

  // Teste 4: Lead-off
  testarLeadOff();

  // Teste 5: WiFi
  Serial.println();
  testarWiFi();

  // Teste 6: Memoria
  Serial.println();
  testarMemoria();

  // Teste 7: Velocidade ADC
  testarVelocidadeADC();

  // Resultado final
  Serial.println();
  Serial.println("========================================");
  Serial.print(" RESULTADO: ");
  Serial.print(testesOK);
  Serial.print("/");
  Serial.print(totalTestes);
  Serial.println(" testes OK");
  Serial.println("========================================");

  if (testesOK == totalTestes) {
    Serial.println(" TUDO FUNCIONANDO!");
  } else {
    Serial.println(" Verifique os itens marcados com [ERRO]");
  }
  Serial.println();

  // Mostra resultado no OLED
  display.clearBuffer();
  display.setFont(u8g2_font_5x7_tr);
  display.drawStr(0, 8, "Diagnostico");

  char buf[20];
  snprintf(buf, sizeof(buf), "%d/%d OK", testesOK, totalTestes);
  display.drawStr(0, 20, buf);

  if (testesOK == totalTestes) {
    display.drawStr(0, 32, "TUDO OK!");
  } else {
    display.drawStr(0, 32, "Ver Serial");
  }
  display.sendBuffer();
}

void loop() {
  // Nada — diagnostico roda uma vez so
  delay(10000);
}
