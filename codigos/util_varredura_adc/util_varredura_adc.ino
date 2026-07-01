// ============================================================
// UTILITARIO — Varredura de TODOS os pinos ADC
// ============================================================
// Placa: ESP32-C3
// Objetivo: eliminar 100% a duvida "em qual pino o OUTPUT
//           do AD8232 esta plugado".
//
// Em vez de ler so o GPIO0, le os 5 pinos ADC1 do ESP32-C3
// (GPIO0, GPIO1, GPIO2, GPIO3, GPIO4) ao mesmo tempo.
// Nao importa onde o fio do OUTPUT esteja: a coluna que
// REAGIR (tremer quando voce toca / molha) denuncia o pino.
//
// Como usar:
//   1. Suba este sketch
//   2. Abra o Serial Monitor (115200) — formato em colunas
//      (ou Serial Plotter: cada pino vira uma linha do grafico)
//   3. Toque/molhe a entrada e veja QUAL coluna muda
// ============================================================

const int PINOS[] = {0, 1, 2, 3, 4};
const int N = sizeof(PINOS) / sizeof(PINOS[0]);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== Varredura ADC (GPIO0..GPIO4) ===");
  Serial.println("A coluna que VARIAR = pino onde o sinal entra");
  // Cabecalho (tambem serve de label no Serial Plotter)
  for (int i = 0; i < N; i++) {
    Serial.print("G");
    Serial.print(PINOS[i]);
    Serial.print(i < N - 1 ? "\t" : "\n");
  }
  delay(1500);
}

void loop() {
  for (int i = 0; i < N; i++) {
    Serial.print("G");
    Serial.print(PINOS[i]);
    Serial.print(":");
    Serial.print(analogRead(PINOS[i]));
    Serial.print(i < N - 1 ? "\t" : "\n");
  }
  delay(50);
}
