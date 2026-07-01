// ============================================================
// UTILITARIO — Teste VALIDO de eletrodos (le lead-off)
// ============================================================
// Placa: ESP32-C3
// Base: datasheet AD8232, secao "DC Leads Off Detection".
//
// POR QUE este teste e o correto:
//   O AD8232 (modo DC leads-off, 3 eletrodos) tem pull-up para
//   +VS em cada entrada. Eletrodo solto = entrada no trilho =
//   saida satura em 4095. Isso e NORMAL, nao defeito.
//   Tocar 1 botao isolado NAO testa nada: os outros 2 soltos
//   mantem a saturacao. O chip so sai de 4095 quando os 3
//   eletrodos fecham circuito juntos (o verde/RLD da a refer.).
//
//   Os pinos lead-off dizem a verdade sobre cada eletrodo:
//     LOD+ (GPIO1) HIGH = AMARELO (LA, +IN) solto
//     LOD- (GPIO2) HIGH = VERMELHO (RA, -IN) solto
//     (datasheet, pinos 11 e 12)
//
// COMO USAR (teste agua salgada, metodo correto):
//   1. Sal bem dissolvido em agua morna, tigela rasa
//   2. Submergir os 3 botoes JUNTOS, SEM encostar um no outro
//   3. Esperar ~15 s (settling / fast restore)
//   4. Ler abaixo: os 2 lead-off devem virar CONECTADO e a
//      saida (OUT) deve sair de 4095 e oscilar
// ============================================================

#define PINO_OUT  0   // OUTPUT do AD8232
#define PINO_LOP  1   // LOD+  -> amarelo (LA)
#define PINO_LOM  2   // LOD-  -> vermelho (RA)

void setup() {
  Serial.begin(115200);
  pinMode(PINO_LOP, INPUT);
  pinMode(PINO_LOM, INPUT);
  delay(1000);
  Serial.println("=== Teste de eletrodos (lead-off) ===");
  Serial.println("Submerja os 3 botoes juntos na agua salgada");
  delay(1500);
}

void loop() {
  int out = analogRead(PINO_OUT);
  bool amareloSolto  = digitalRead(PINO_LOP);  // HIGH = solto
  bool vermelhoSolto = digitalRead(PINO_LOM);

  Serial.print("OUT=");
  Serial.print(out);
  Serial.print("\tAMARELO(LA)=");
  Serial.print(amareloSolto  ? "SOLTO" : "CONECTADO");
  Serial.print("\tVERMELHO(RA)=");
  Serial.print(vermelhoSolto ? "SOLTO" : "CONECTADO");

  if (!amareloSolto && !vermelhoSolto && out > 200 && out < 3900) {
    Serial.print("\t<-- SINAL OK");
  }
  Serial.println();

  delay(100);
}
