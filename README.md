# Detector de Fadiga Muscular com ESP32-C3 e AD8232

Guia completo para montar, do zero, um **detector de fadiga muscular caseiro**. O
aparelho lê o sinal elétrico do seu músculo pela pele (eletromiografia de superfície,
ou EMG), calcula a intensidade desse sinal e mostra, em tempo real, uma barra de
fadiga na telinha OLED — **sem precisar de computador depois de programado**.

Este README foi escrito para quem **nunca mexeu com Arduino ou eletrônica**. Se você
seguir os passos na ordem, chega no aparelho funcionando. Ao final há uma seção de
solução de problemas com os erros reais que enfrentamos, e links para a documentação
que explica a teoria e as decisões do projeto.

> **Aviso importante.** Isto **não é um equipamento médico**. A leitura é qualitativa
> (serve para *ver* o músculo trabalhando e cansando), não fornece nenhum número
> clínico e não deve ser usada para diagnóstico. É um projeto educacional e
> exploratório da disciplina CFA (Computação Física e Aplicações).

---

## Índice

- [O que você vai construir](#o-que-você-vai-construir)
- [Materiais necessários](#materiais-necessários)
- [Sobre o sensor AD8232](#sobre-o-sensor-ad8232)
- [Montagem do hardware](#montagem-do-hardware)
- [Preparando o ambiente de programação](#preparando-o-ambiente-de-programação)
- [Enviando o firmware para a placa](#enviando-o-firmware-para-a-placa)
- [Testando o hardware antes de usar](#testando-o-hardware-antes-de-usar)
- [Colocando os eletrodos](#colocando-os-eletrodos)
- [Como usar: passo a passo](#como-usar-passo-a-passo)
- [Como funciona por dentro](#como-funciona-por-dentro)
- [Solução de problemas](#solução-de-problemas)
- [Limitações e o que esperar](#limitações-e-o-que-esperar)
- [Indo além](#indo-além)
- [Documentação do projeto](#documentação-do-projeto)
- [Referências](#referências)

---

## O que você vai construir

Um dispositivo pequeno, alimentado por um cabo USB, que:

- Lê o sinal elétrico de um músculo (por exemplo, o bíceps) através de três eletrodos
  colados na pele.
- Amostra esse sinal 1000 vezes por segundo e calcula o **RMS** (uma medida de
  intensidade — explicada mais adiante).
- Faz uma **calibração automática** quando você começa a se esforçar: mede seu nível
  de base durante alguns segundos.
- Mostra na tela OLED embutida, em tempo real: o valor do sinal, a porcentagem de
  fadiga, uma barra de progresso e um pequeno gráfico do histórico.

Tudo roda dentro do próprio ESP32. Depois de programar, você só precisa do cabo USB
para ligar.

---

## Materiais necessários

| Componente | Para que serve |
|---|---|
| **ESP32-C3 com OLED 0.42" embutido** (tela de 72×40 pixels) | É o "cérebro" e a tela ao mesmo tempo. Lê o sensor, faz as contas e mostra o resultado. |
| **Módulo sensor AD8232** | Amplifica o sinal fraquíssimo do músculo até um nível que o ESP32 consegue ler. |
| **Cabo de eletrodos ECG de 3 vias** (com plugue P2 / 3.5 mm) | Liga os eletrodos ao módulo AD8232. Vem com três garras (snaps) coloridas. |
| **3 eletrodos descartáveis de ECG/EMG** (com gel condutor) | Fazem o contato com a pele. Use eletrodos com o gel ainda úmido — gel ressecado não conduz. |
| **Jumpers fêmea-fêmea (Dupont)** | Fazem as ligações entre o módulo e a placa. Você vai precisar de 5. |
| **Cabo USB-C** | Programa e alimenta o ESP32-C3. |

> **Dica.** Esses itens são vendidos em qualquer loja de eletrônica para maker
> (Eletrogate, Mercado Livre, etc.). Ao comprar o AD8232, prefira o kit que já vem
> com o cabo de eletrodos e alguns adesivos.

---

## Sobre o sensor AD8232

O AD8232 é um circuito integrado da Analog Devices feito para captar **biopotenciais**
— sinais elétricos muito pequenos gerados pelo corpo. Na origem ele é um sensor de
batimento cardíaco (ECG), mas o mesmo princípio serve para o EMG do músculo.

O módulo que você vai usar expõe alguns pinos. Os que interessam para este projeto
são:

| Pino do módulo | Função |
|---|---|
| **3.3V** | Alimentação (positivo) |
| **GND** | Alimentação (terra) |
| **OUTPUT** | Sinal analógico de saída — é o que o ESP32 vai ler |
| **LO+** | Detector de "eletrodo solto" (lead-off), entrada positiva |
| **LO-** | Detector de "eletrodo solto" (lead-off), entrada negativa |
| **SDN** | Desliga o chip quando aterrado. **Não conecte** (deixe livre). |

Os pinos **LO+** e **LO-** avisam quando um eletrodo perde contato. O firmware usa
isso para mostrar "ELETRODO SOLTO" na tela em vez de exibir um sinal falso.

---

## Montagem do hardware

A tela OLED **já é embutida** no ESP32-C3 e é ligada internamente — você **não precisa
ligar nada da tela**. Só é preciso conectar o módulo AD8232.

Faça as cinco ligações abaixo com jumpers, seguindo exatamente esta tabela:

| Pino do AD8232 | Pino do ESP32-C3 |
|---|---|
| 3.3V | **3V3** |
| GND | **GND** |
| OUTPUT | **GPIO0** |
| LO+ | **GPIO2** |
| LO- | **GPIO1** |

Por fim, encaixe o **cabo de eletrodos** no conector P2 (3.5 mm) do módulo AD8232.

> **Atenção — este é o erro que mais custou tempo no projeto.** Garanta que os jumpers
> de **3.3V** e **GND** estão bem firmes. Um jumper de alimentação folgado faz o chip
> **parecer** ligado (os pinos até têm tensão), mas o sensor nunca funciona de verdade
> e a saída fica travada em 4095. Se puder, confirme com um multímetro que os 3.3V
> chegam de fato ao módulo. (A história completa está em
> [depuracao.md](depuracao.md).)

---

## Preparando o ambiente de programação

Você vai usar a **Arduino IDE** para enviar o programa à placa.

### 1. Instale a Arduino IDE

Baixe em [arduino.cc/en/software](https://www.arduino.cc/en/software) e instale a
versão 2.x.

### 2. Adicione o suporte às placas ESP32

1. Abra **Arquivo → Preferências**.
2. No campo **URLs adicionais para Gerenciadores de Placas**, cole:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Vá em **Ferramentas → Placa → Gerenciador de Placas**, procure por **esp32** e
   instale o pacote **"esp32" da Espressif**.

### 3. Instale a biblioteca da tela

1. Abra **Ferramentas → Gerenciar Bibliotecas**.
2. Procure por **U8g2** (autor: oliver) e instale.

### 4. Selecione a placa e as configurações

Em **Ferramentas**, ajuste:

- **Placa:** `ESP32C3 Dev Module`
- **USB CDC On Boot:** `Enabled` — *essencial*, sem isso o Monitor Serial não funciona
  no ESP32-C3.
- **Upload Speed:** `115200`
- **Porta:** selecione a porta COM que aparece quando você conecta a placa.

> Os demais parâmetros podem ficar no padrão.

---

## Enviando o firmware para a placa

O programa principal está em
[`firmware/fase4_fadiga_oled/`](firmware/fase4_fadiga_oled). Ele calibra sozinho,
calcula o RMS e mostra a fadiga na tela.

1. Abra o arquivo `firmware/fase4_fadiga_oled/fase4_fadiga_oled.ino` na Arduino IDE.
2. Conecte o ESP32-C3 pelo cabo USB-C.
3. Confira que a **Placa** e a **Porta** estão corretas (seção anterior).
4. Clique em **Carregar** (a seta →) e aguarde a mensagem "Concluído o carregamento".

Se der certo, a tela vai acender mostrando **"EMG Fadiga — Contraia o musculo p/
calibrar..."**.

---

## Testando o hardware antes de usar

Antes de colar eletrodos em você, vale confirmar que a montagem está correta. A pasta
[`codigos/`](codigos) traz três utilitários simples usados durante a depuração:

- **`codigos/util_diagnostico/`** — testa de uma vez os componentes principais (OLED,
  leitura do ADC, lead-off, I2C, WiFi, memória). Bom primeiro teste da montagem.
- **`codigos/util_teste_eletrodos/`** — imprime, para cada eletrodo, "conectado" ou
  "solto", junto do valor de saída. É o teste correto de contato, baseado no
  datasheet.
- **`codigos/util_varredura_adc/`** — lê os cinco pinos de ADC ao mesmo tempo, útil se
  você não tiver certeza de em qual pino o sinal entra.

Carregue um deles do mesmo jeito que o firmware principal e abra o **Monitor Serial**
(115200 baud) para ver as leituras.

---

## Colocando os eletrodos

O cabo de eletrodos tem três garras coloridas. Pela convenção do datasheet:

- **Amarelo (LA)** e **Vermelho (RA)** — vão sobre o **ventre do músculo** que você
  quer medir (por exemplo, ao longo do bíceps), separados alguns centímetros.
- **Verde (referência)** — vai num ponto **elétricamente neutro**, de preferência
  sobre um osso próximo (cotovelo ou pulso). Ele estabelece a referência do corpo, e
  o sensor **só sai do valor travado quando os três eletrodos estão em contato**.

Dicas de contato:
- Limpe a pele (pele oleosa ou com creme atrapalha).
- Use eletrodos com gel úmido.
- Evite mexer no cabo durante a medição: movimento do fio vira ruído.

---

## Como usar: passo a passo

Com os eletrodos colados e a placa ligada:

1. **Repouso.** A tela mostra "Contraia o musculo p/ calibrar...". O aparelho está
   esperando você começar.
2. **Contraia.** Faça força no músculo. Quando o sinal sobe acima de um mínimo, o
   aparelho entra em **CALIBRANDO** e uma barra de progresso começa a encher.
3. **Segure a força constante** por cerca de 6 a 7 segundos, até a barra encher. O
   primeiro segundo é descartado (a "subida" da força); o resto vira sua **linha de
   base**.
   - Se você soltar a força no meio, a calibração é abortada e volta ao repouso.
   - Se a força variar demais, aparece **"Instavel! Recalibrar"** — recomece
     segurando com mais firmeza e constância.
4. **Monitore.** Com a base fixada, a tela passa a mostrar em tempo real:
   - `RMS:` o valor atual do sinal;
   - `F:` a porcentagem de fadiga;
   - uma **barra** de fadiga e um **gráfico** do histórico.
5. **Mantenha o exercício.** À medida que o músculo cansa numa contração sustentada,
   o RMS tende a **subir** acima da linha de base — e é isso que a barra de fadiga
   mostra.

Se algum eletrodo perder contato, a tela avisa **"ELETRODO SOLTO"**.

---

## Como funciona por dentro

Um resumo rápido (a explicação completa está em [base_teorica.md](base_teorica.md)):

- **Amostragem a 1000 Hz.** Um timer de hardware lê o sinal a cada 1 ms. O EMG vive
  entre 20 e 500 Hz, então 1000 Hz atende ao critério de Nyquist com folga.
- **RMS em janelas de 256 amostras (~256 ms).** O sinal bruto parece ruído; ler ponto
  a ponto não diz nada. O RMS mede a *intensidade* da oscilação — é o estimador padrão
  de amplitude em EMG.
- **Calibração por mediana.** A linha de base é a mediana das janelas coletadas
  (robusta a picos isolados), e só é aceita se for estável o bastante (coeficiente de
  variação abaixo de 0,35).
- **Fadiga = quanto o RMS subiu.** Numa contração submáxima sustentada, o músculo
  recruta mais fibras para manter a força, e o RMS sobe. A fadiga em % mede esse
  aumento sobre a base.

---

## Solução de problemas

| Sintoma | Causa provável | O que fazer |
|---|---|---|
| A tela OLED não acende | Placa/porta erradas, ou cabo USB só de carga | Confira a seleção da placa (`ESP32C3 Dev Module`) e use um cabo USB de dados |
| O Monitor Serial não mostra nada | `USB CDC On Boot` desabilitado | Habilite em **Ferramentas → USB CDC On Boot** e recarregue |
| O valor fica **travado em 4095** | Eletrodo solto **ou** o módulo AD8232 sem alimentação de verdade (jumper 3.3V/GND folgado) | Confirme com multímetro que os 3.3V chegam ao módulo; refaça os jumpers de alimentação; verifique os eletrodos. **Esta foi a causa raiz no nosso caso.** |
| Aparece "ELETRODO SOLTO" o tempo todo | Um dos três eletrodos sem contato | Recole os eletrodos, limpe a pele, garanta que o **verde (referência)** está bem colado |
| "Instavel! Recalibrar" na calibração | A força variou demais durante os ~7 s | Recalibre segurando uma força **constante** |
| A barra de fadiga não sai do zero | Força caindo, ou limitação do sensor (veja abaixo) | Mantenha uma contração submáxima **constante**; entenda as limitações |

> A investigação completa do defeito de hardware (a saga da trava em 4095, com os
> becos sem saída) está em [depuracao.md](depuracao.md). Vale a leitura: mostra como
> um jumper de alimentação folgado enganou o diagnóstico por semanas.

---

## Limitações e o que esperar

Seja realista sobre o que este aparelho faz. Detalhes em
[exequibilidade.md](exequibilidade.md).

- **É qualitativo.** Serve para *ver* o músculo ativar e cansar, não para medir fadiga
  em número clínico.
- **A banda do módulo atrapalha.** O módulo AD8232 clone vem filtrado para a faixa do
  coração (~0,5 a 40 Hz), enquanto o EMG ocupa de 20 a 500 Hz. Ou seja, boa parte da
  energia útil é cortada. Para detectar *se* o músculo está contraído, funciona; para
  medir fadiga com precisão, o filtro limita.
- **A fadiga real é sutil.** Amplitude (RMS) sobe ou desce dependendo do protocolo de
  contração. Este firmware assume contração submáxima sustentada (RMS sobe). O método
  mais robusto seria acompanhar a **frequência mediana**, que não foi implementado.
- **Sensível ao contato.** Eletrodo mal colado, pele oleosa ou cabo em movimento viram
  ruído.

---

## Indo além

Alguns caminhos naturais para quem quiser continuar:

- **Análise de frequência (MDF/FFT).** A frequência mediana cai de forma mais
  confiável com a fadiga do que a amplitude. A 1000 Hz com 256 pontos, uma FFT é
  viável.
- **Sensor de EMG dedicado.** Trocar o AD8232 por um sensor com a banda correta (20–500
  Hz) capturaria o sinal completo.
- **Interface web.** Dá para servir um gráfico em tempo real no celular/PC pela mesma
  rede WiFi, em vez de só na tela OLED.

---

## Documentação do projeto

Além deste guia, o projeto tem três textos que registram o raciocínio por trás das
escolhas. Podem ser lidos em qualquer ordem:

- **[base_teorica.md](base_teorica.md)** — o que é o sinal EMG, o que acontece no
  músculo durante a fadiga e por que os cálculos ficaram como ficaram.
- **[exequibilidade.md](exequibilidade.md)** — a avaliação de se o projeto funcionaria,
  com as ressalvas (incluindo uma fórmula de fadiga que estava invertida).
- **[depuracao.md](depuracao.md)** — a investigação do sensor que travava em 4095, com
  os erros de diagnóstico que ensinaram pelo caminho.

Estrutura das pastas:

- [`firmware/`](firmware) — o programa que roda no ESP32 (fase 4: calibração
  automática + RMS + fadiga no OLED).
- [`codigos/`](codigos) — sketches utilitários usados para testar e depurar o sensor.

---

## Referências

A lista completa de referências (datasheet do AD8232, artigos sobre EMG e fadiga,
bibliotecas usadas) está em **[referencia.md](referencia.md)**.
