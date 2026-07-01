AVALIAÇÃO DE EXEQUIBILIDADE
========================================

Lá pela metade do caminho, paramos para avaliar com frieza se o projeto tinha
futuro ou era perda de tempo. A resposta curta foi "tem, com ressalvas". As
ressalvas seguem abaixo.


O PONTO DE PARTIDA
----------------------------------------

  - O hardware nunca foi validado de forma confiável. Em maio, o sinal chegou a
    oscilar entre ~3400 e 4100 em alguns testes (dedos nos botões, dedão no fio, três
    snaps na água salgada), o que na hora pareceu prova de que o módulo amplificava e
    o ADC lia — mas foi pontual, não se repetiu, e hoje chutamos que tenha sido
    alguma interferência, não funcionamento real.
  - O que parecia o único bloqueio da época eram os adesivos de eletrodo com gel
    ressecado.
  - Firmware mais maduro do que se esperaria de três iniciantes: amostragem a
    1000 Hz com timer de hardware, RMS sobre 256 amostras, calibração automática de
    linha de base, interface web no celular com gráfico em tempo real e detecção de
    eletrodo solto.


A FÓRMULA DE FADIGA INVERTIDA
----------------------------------------

A ressalva mais séria não era de engenharia, e sim de fisiologia.

  - O código assumia que, na fadiga, a amplitude do sinal cai e o RMS cai junto. A
    fadiga subia à medida que o RMS descia abaixo da linha de base.
  - A literatura aponta o contrário para boa parte dos casos: numa contração
    isométrica sustentada, a amplitude SOBE com a fadiga (recrutamento
    compensatório); o que cai é a frequência mediana.
  - Consequência prática: ao segurar uma contração até cansar, a barra de fadiga
    ficaria parada em zero enquanto o RMS subia — leitura imediata de "não
    funciona", quando era a fórmula invertida.
  - O próprio material do projeto se contradizia: num trecho dizia que o RMS sobe e
    a MDF cai; em outro, o oposto.

Fontes: De Luca (1997); Healthcare Bulletin (2023); PubMed 492202; PMC5405568
(ver referencia.md).


A BANDA DO AD8232
----------------------------------------

  - O AD8232 é, na origem, um sensor de batimento cardíaco; o módulo clone vem
    filtrado para a banda do coração (~0,5 a 40 Hz).
  - O EMG ocupa de 20 a 500 Hz, então o módulo corta a maior parte da energia útil —
    o que chega ao ESP32 é mais uma versão filtrada do EMG do que o sinal completo.
  - Para detectar se o músculo está contraído ou não, ainda funciona. Para medir
    fadiga com precisão ou fazer análise de frequência, o filtro atrapalha.
  - Resolver exigiria trocar componentes minúsculos no módulo ou um sensor de EMG
    dedicado — fora do escopo.

Fontes: datasheet AD8232; Pflanzer et al. (2023) (ver referencia.md).


DOCUMENTAÇÃO DESATUALIZADA
----------------------------------------

  - Um texto teórico antigo dizia que o sistema amostrava a ~50 Hz, com janelas
    pequenas e delay, quando o firmware já estava em 1000 Hz com timer.
  - Em algum momento a abordagem mudou e o texto não acompanhou. Nada grave, só
    documentação velha contando uma história que o código não vivia mais.


PROBABILIDADE DE SUCESSO
----------------------------------------

  - Estas estimativas foram feitas assumindo que o sensor daria sinal — o que a
    depuração posterior colocou em dúvida (ver depuracao.md).
  - Aparecer alguma coisa no gráfico (demonstração visual que reage à contração):
    alta, em torno de 85%.
  - Medir fadiga corretamente, com a fórmula e o filtro como estavam: bem menor.
  - Corrigir a fórmula elevava bastante esse segundo número; mexer no filtro
    elevaria mais, mas a um custo de risco incompatível com o tamanho do projeto.


RESUMO
----------------------------------------

  - O firmware estava sólido; o hardware nunca deu sinal estável — a oscilação de
    maio foi pontual e provavelmente interferência.
  - O maior deles, a fórmula invertida, é também a lição mais valiosa: descobrir que
    a previsão contraria os dados reais ensina mais do que acertar de primeira.
  - O projeto sempre foi exploração, não TCC nem produto — e por essa régua, deu
    certo.

Referências completas em referencia.md.
