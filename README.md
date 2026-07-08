# Detector de fadiga muscular — registro do projeto

Registro de um projeto da disciplina CFA (Computação Física e Aplicações): montar um
detector de fadiga muscular caseiro, com um ESP32 e um sensor AD8232, lendo o sinal
elétrico do músculo pela pele. É um trabalho de disciplina de caráter exploratório.

São três textos, e podem ser lidos em qualquer ordem:

- [base_teorica.md](base_teorica.md) — o que entendemos sobre o sinal EMG, sobre o
  que acontece no músculo durante a fadiga e por que as escolhas de cálculo ficaram
  como ficaram.
- [exequibilidade.md](exequibilidade.md) — a avaliação de se o projeto funcionaria,
  com as ressalvas que apareceram, incluindo uma fórmula de fadiga invertida.
- [depuracao.md](depuracao.md) — a investigação do sensor que travava em 4095, com
  os becos sem saída e os erros de diagnóstico que ensinaram pelo caminho.

A pasta [codigos/](codigos/) traz os sketches que usamos para depurar o sensor, e a
pasta [firmware/](firmware/) guarda o programa que ficou rodando no ESP32 — a fase 4,
que calibra sozinha, calcula o RMS do sinal e mostra a fadiga em tempo real no OLED
embutido. Por fim, [referencia.md](referencia.md) reúne todas as referências e os
códigos citados.
