DEPURAÇÃO DO SENSOR — A TRAVA EM 4095
========================================

Esta foi a parte que mais consumiu tempo e mais nos ensinou. Começou como "o sensor
parou de funcionar" e virou uma caça ao defeito de hardware que durou semanas, boa
parte dela sem multímetro. Registramos como aconteceu, com os erros de diagnóstico
no meio.


A MONTAGEM E O PROBLEMA
----------------------------------------

  - ESP32-C3 com OLED embutido lendo um módulo AD8232 clone
  - OUTPUT → GPIO0; lead-off em GPIO1 e GPIO2; alimentação 3.3V e GND
  - Eletrodos por cabo ECG de três vias (amarelo = LA, vermelho = RA, verde =
    referência, conforme o datasheet)
  - Em maio, o sinal chegou a oscilar entre 3400 e 4100 em alguns testes, mas foi
    pontual: nunca se repetiu, e hoje chutamos que tenha sido alguma interferência,
    não prova de que o conjunto funcionava
  - Fora esse episódio, a saída sempre travou perto de 4095 (o teto do ADC)


PRIMEIRA HIPÓTESE: O CABO
----------------------------------------

  - Mexer no fio amarelo mudava algo; vermelho e verde, nada → apostamos em cabo
    rompido por dentro
  - Compramos um cabo novo e não resolveu: a água salgada continuou travando no
    teto, sem repetir a oscilação pontual de maio
  - Conclusão: se um cabo novo não mudou nada, o problema provavelmente não estava
    só no cabo
  - Decidimos pela bisseção — isolar metade do sistema por vez e parar no primeiro
    teste que variasse


VARREDURA DE ADC (codigos/util_varredura_adc)
----------------------------------------

Para não depender da memória sobre qual pino recebia o sinal, escrevemos um sketch
que lê os cinco pinos de ADC do ESP32-C3 ao mesmo tempo.

  - Pinos soltos: ruído variando à toa → ESP32 e ADC estão bons
  - Pinos ligados ao módulo: presos em 4095 fixo → módulo energizado, segurando os
    pinos em nível alto


O ERRO DO "AMARELO MORTO" E O DATASHEET
----------------------------------------

  - Tocando um eletrodo por vez, vermelho e verde faziam o número tremer, o amarelo
    não. Cravamos: o amarelo está morto. Estava errado.
  - O datasheet desmontou a conclusão: no modo de detecção de eletrodo solto com
    três eletrodos, cada entrada tem um resistor que a puxa para a alimentação
  - Eletrodo solto satura a saída em 4095 — comportamento NORMAL, não defeito
  - O chip só sai do teto quando os três eletrodos fecham o circuito juntos (o verde
    estabelece a referência do corpo)
  - Logo, encostar um botão sozinho não prova nada; o tremor era só ruído de 60 Hz
    captado pelo corpo
  - Lição: quando o instrumento de medida não é confiável, a conclusão também não é

Fonte: datasheet AD8232, seção DC Leads-Off Detection (ver referencia.md).


TESTE VÁLIDO DE LEAD-OFF (codigos/util_teste_eletrodos)
----------------------------------------

Com o datasheet em mãos, escrevemos um sketch que lê os pinos de lead-off (o
indicador correto) e imprime "conectado" ou "solto" por eletrodo, junto da saída.

  - Teste: três snaps com os dedos molhados (atalho da água salgada)
  - Resultado: 4095, com os dois eletrodos marcados como solto, mesmo molhados e com
    o jack encaixado
  - Mudamos o rumo: equipamento novo, cabo novo e zero sinal em todos os testes
    combinava mais com erro de montagem/conexão do que com componente queimado



O TESTE DO CURTO
----------------------------------------

O teste decisivo: curtar os três snaps metálicos entre si, pulando de uma vez o
eletrodo, a pele, a água e a ordem dos fios.

  - Resultado: solto, solto, 4095 — em dois cabos diferentes
  - Eliminou de vez: eletrodos, água, contato da pele, ordem dos fios, alimentação,
    shutdown
  - Restaram dois suspeitos, ambos na placa: o jack de 3.5mm (mau contato/solda
    fria) ou a entrada do chip
  - O jack é comum aos dois cabos → favorito
  - Aposentamos a "prova" do dedo no snap: era acoplamento capacitivo de 60 Hz, que
    atravessa junta meio aberta e não comprova continuidade real


O DESFECHO
----------------------------------------

O que fechou o caso foi finalmente ter um multímetro em mãos. Medindo continuidade
e tensão ponto a ponto, praticamente todos os problemas se explicaram de uma vez: um
dos jumpers que levava energia ao módulo AD8232 não estava fazendo contato. O chip
nunca tinha sido alimentado de fato.

  - Trocamos o jumper por um firme e a saída saiu do teto na hora
  - OUT passou a oscilar por volta de 1500 a 3168, no meio da escala — sinal EMG de
    verdade, sem grudar em 4095
  - Amarelo e vermelho marcaram conectado, e o lead-off finalmente fez sentido (só
    fecha com o chip alimentado)

Isso explica a saga inteira de trás para frente: a trava em 4095 era o par de
entradas parado no trilho, sem o amplificador de instrumentação para trabalhar; o
lead-off vinha sem sentido; a água salgada não variava. A intuição de que era erro
de montagem, e não componente queimado, estava certa o tempo todo. A alimentação
tinha sido "descartada" mais de uma vez conferindo só o pino certo (3.3V no lugar),
sem nunca medir se o jumper realmente encostava — foi o multímetro que expôs isso.

Códigos usados: codigos/util_varredura_adc, codigos/util_teste_eletrodos,
codigos/util_diagnostico. Referências completas em referencia.md.
