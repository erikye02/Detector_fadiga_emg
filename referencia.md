# Referências

## Códigos usados na depuração (nesta pasta)

- `codigos/util_varredura_adc/` — lê os cinco pinos de ADC do ESP32-C3 (GPIO0 a
  GPIO4) ao mesmo tempo, para identificar em qual pino o sinal entra. Citado em
  depuracao.md.
- `codigos/util_teste_eletrodos/` — lê os pinos de lead-off do AD8232 e imprime
  "conectado" ou "solto" por eletrodo, junto do valor de saída. É o teste correto
  para contato de eletrodo, baseado no datasheet. Citado em depuracao.md.
- `codigos/util_diagnostico/` — testa de uma vez os componentes do conjunto (OLED,
  ADC, lead-off, I2C, WiFi, memória), usado para validar a montagem.

## Documentação do hardware

- AD8232 Datasheet Rev. D — Analog Devices.
  https://www.analog.com/media/en/technical-documentation/data-sheets/ad8232.pdf
- AD8232 Product Page — Analog Devices.
  https://www.analog.com/en/products/ad8232.html
- SparkFun AD8232 Heart Rate Monitor Hookup Guide.
  https://learn.sparkfun.com/tutorials/ad8232-heart-rate-monitor-hookup-guide/all
- Monitoramento da atividade elétrica do coração com AD8232 — Blog Eletrogate.
  https://blog.eletrogate.com/monitoramento-da-atividade-eletrica-do-coracao/
- Documentação oficial ESP32 — Espressif.
  https://docs.espressif.com/projects/esp-idf/en/latest/esp32/

## Validação do AD8232 para EMG

- Pflanzer et al. (2023). AD8232 to Biopotentials Sensors: Open Source Project and
  Benchmark. MDPI Electronics, 12(4):833. https://www.mdpi.com/2079-9292/12/4/833

## Fisiologia da fadiga e EMG

- De Luca, C.J. (1997). The use of surface electromyography in biomechanics.
  Journal of Applied Biomechanics, 13(2), 135-163.
- Konrad, P. (2005). The ABC of EMG: A Practical Introduction to Kinesiological
  Electromyography. Noraxon Inc.
- Healthcare Bulletin (2023). Evaluation of Muscle Fatigue Using Surface
  Electromyography during Isometric Contractions.
  https://healthcare-bulletin.co.uk/article/evaluation-of-muscle-fatigue-using-surface-electromyography-during-isometric-contractions-in-athletes-and-non-athletes-3201/
- Amplitude of the surface electromyogram during fatiguing isometric contractions.
  PubMed 492202. https://pubmed.ncbi.nlm.nih.gov/492202/
- Effects of Force Load, Muscle Fatigue on sEMG. PMC5405568.
  https://pmc.ncbi.nlm.nih.gov/articles/PMC5405568/

## Projetos semelhantes (Arduino/ESP32 + EMG + fadiga)

- Ayaz & Ayub (2020). Arduino Based Fatigue Level Measurement in Muscular Activity
  using RMS Technique. IEEE. https://ieeexplore.ieee.org/document/9280184/
- Gym training muscle fatigue monitoring using EMG MyoWare and Arduino.
  ResearchGate, 2023. https://www.researchgate.net/publication/373292132
- Real time detection of muscle fatigue using Arduino based surface EMG frequency
  and amplitude measurements. JSAR. https://jsar.fsha.org/index.php/jsar/article/view/68

## Tutoriais e referências práticas

- Amplitude Analysis: Root-mean-square EMG Envelope — Delsys.
  https://delsys.com/amplitude-analysis-root-mean-square-emg-envelope/
- Designing an Arduino-based EMG monitor — EngineersGarage.
  https://www.engineersgarage.com/arduino-based-emg-monitor-ad8226/

## Bibliotecas de software

- U8g2 — displays OLED. https://github.com/olikraus/u8g2
- arduinoFFT — transformada de Fourier. https://github.com/kosme/arduinoFFT
