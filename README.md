# Detector de fadiga muscular — registro do projeto

Registro de um projeto exploratório: montar um detector de fadiga muscular caseiro,
com um ESP32 e um sensor AD8232, lendo o sinal elétrico do músculo pela pele. Não é
trabalho acadêmico nem produto; é um grupo aprendendo eletrônica, microcontrolador e
processamento de sinais na prática, errando e corrigindo pelo caminho.

São três textos, e podem ser lidos em qualquer ordem:

- [base_teorica.md](base_teorica.md) — o que entendemos sobre o sinal EMG, sobre o
  que acontece no músculo durante a fadiga e por que as escolhas de cálculo ficaram
  como ficaram.
- [exequibilidade.md](exequibilidade.md) — a avaliação de se o projeto funcionaria,
  com as ressalvas que apareceram, incluindo uma fórmula de fadiga invertida.
- [depuracao.md](depuracao.md) — a investigação do sensor que travava em 4095, com
  os becos sem saída e os erros de diagnóstico que ensinaram pelo caminho.

A pasta [codigos/](codigos/) traz os sketches que usamos para depurar o sensor, e
[referencia.md](referencia.md) reúne todas as referências e os códigos citados.
