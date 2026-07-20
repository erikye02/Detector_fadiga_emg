# Detector de Fadiga Muscular com ESP32-C3 e AD8232

Guia completo para montar, do zero, um **detector de fadiga muscular caseiro**. O
aparelho lê o sinal elétrico do seu músculo pela pele (eletromiografia de superfície,
ou EMG), calcula a intensidade desse sinal e mostra o resultado em tempo real de duas
formas: na telinha OLED embutida e numa **interface web** que você abre pelo celular
ou pelo PC na mesma rede WiFi. A página tem três modos: um **monitor de fadiga**, um
**jogo do dinossauro** controlado pela contração do músculo e um **osciloscópio** que
mostra o sinal bruto ao vivo.

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
- [Configurando o WiFi](#configurando-o-wifi)
- [Enviando o firmware para a placa](#enviando-o-firmware-para-a-placa)
- [Abrindo a interface web](#abrindo-a-interface-web)
- [Testando o hardware antes de usar](#testando-o-hardware-antes-de-usar)
- [Colocando os eletrodos](#colocando-os-eletrodos)
- [Como usar: os três modos](#como-usar-os-três-modos)
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
- Se conecta à sua rede WiFi e serve uma **página web** com um menu de três modos:
  - **Monitor de Fadiga** — barra e gráficos de fadiga ao vivo, com calibração
    automática do seu nível de base.
  - **Jogo do Dino** — o clássico dinossauro do Chrome, que **pula quando você
    contrai o músculo**.
  - **Osciloscópio** — o sinal EMG bruto rolando na tela em tempo real.
- Também mostra o modo ativo na **tela OLED embutida**, então dá para acompanhar sem
  olhar para o navegador.

O mesmo firmware faz tudo isso. Você escolhe o modo pela página web, e o ESP processa
só o modo ativo.

---

## Materiais necessários

| Componente | Para que serve |
|---|---|
| **ESP32-C3 com OLED 0.42" embutido** (tela de 72×40 pixels) | É o "cérebro" e a tela ao mesmo tempo. Lê o sensor, faz as contas, serve a página web e mostra o resultado. |
| **Módulo sensor AD8232** | Amplifica o sinal fraquíssimo do músculo até um nível que o ESP32 consegue ler. |
| **Cabo de eletrodos ECG de 3 vias** (com plugue P2 / 3.5 mm) | Liga os eletrodos ao módulo AD8232. Vem com três garras (snaps) coloridas. |
| **3 eletrodos descartáveis de ECG/EMG** (com gel condutor) | Fazem o contato com a pele. Use eletrodos com o gel ainda úmido — gel ressecado não conduz. |
| **Jumpers fêmea-fêmea (Dupont)** | Fazem as ligações entre o módulo e a placa. Você vai precisar de 5. |
| **Cabo USB-C** | Programa e alimenta o ESP32-C3. |
| **Uma rede WiFi 2.4 GHz** | O ESP32-C3 se conecta a ela para servir a página. **Precisa ser 2.4 GHz** — o ESP32-C3 não enxerga redes 5 GHz. O celular/PC que vai abrir a página tem que estar na **mesma rede**. |

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
isso para mostrar "ELETRODO SOLTO" na tela e na página em vez de exibir um sinal falso.

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

> As bibliotecas de rede (`WiFi` e `WebServer`) já vêm junto com o pacote da ESP32 —
> não precisa instalar nada além do U8g2.

### 4. Selecione a placa e as configurações

Em **Ferramentas**, ajuste:

- **Placa:** `ESP32C3 Dev Module`
- **USB CDC On Boot:** `Enabled` — *essencial*, sem isso o Monitor Serial não funciona
  no ESP32-C3.
- **Upload Speed:** `115200`
- **Porta:** selecione a porta COM que aparece quando você conecta a placa.

> Os demais parâmetros podem ficar no padrão.

---

## Configurando o WiFi

Antes de enviar o firmware, você precisa dizer a ele o nome e a senha da sua rede.

1. Abra `firmware/fase5d_menu/fase5d_menu.ino` na Arduino IDE.
2. Logo no começo do arquivo, ache estas duas linhas:
   ```cpp
   const char* SSID  = "NOME_DA_SUA_REDE";    // <-- ALTERE AQUI
   const char* SENHA = "SENHA_DA_SUA_REDE";   // <-- ALTERE AQUI
   ```
3. Troque pelo nome e senha da sua rede WiFi **2.4 GHz**, mantendo as aspas. Exemplo:
   ```cpp
   const char* SSID  = "MinhaCasa_2G";
   const char* SENHA = "minhasenha123";
   ```

> Se o nome ou a senha estiverem errados, a placa mostra **"WiFi FALHOU"** na tela ao
> ligar. É só corrigir as linhas e enviar de novo.

---

## Enviando o firmware para a placa

O firmware do projeto é o
[`firmware/fase5d_menu/`](firmware/fase5d_menu). Ele conecta no WiFi, serve a página
com o menu e roda os três modos.

1. Com o arquivo `firmware/fase5d_menu/fase5d_menu.ino` aberto e o **WiFi já
   configurado** (seção anterior).
2. Conecte o ESP32-C3 pelo cabo USB-C.
3. Confira que a **Placa** e a **Porta** estão corretas (seção anterior).
4. Clique em **Carregar** (a seta →) e aguarde a mensagem "Concluído o carregamento".

Ao ligar, a tela mostra **"Conectando WiFi..."** e, quando conecta, exibe o
**endereço IP** da placa e a mensagem **"Abra no PC"**. Guarde esse IP — é por ele que
você abre a interface.

---

## Abrindo a interface web

1. Confira que o seu celular ou PC está na **mesma rede WiFi** que você configurou.
2. Olhe o **IP** que aparece na tela OLED (algo como `192.168.0.42`). Se perder,
   ele também é impresso no **Monitor Serial** (115200 baud) ao ligar.
3. Abra o navegador e digite esse IP na barra de endereço (ex.: `http://192.168.0.42`).

A página abre num **menu com três cards**. Toque num deles para entrar no modo. O botão
**← Menu** no canto superior volta para a escolha. Ao trocar de modo na web, o ESP passa
a processar aquele modo e a tela OLED acompanha.

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

## Como usar: os três modos

Com os eletrodos colados, a placa ligada e a página aberta no menu, escolha um modo.

### 💪 Monitor de Fadiga

Mede quanto o sinal do músculo sobe ao longo de uma contração sustentada. Ao entrar, a
página já mostra o RMS ao vivo. Os botões no topo controlam o fluxo:

1. **INICIAR** — (re)começa a captura do sinal ao vivo.
2. **Contraia** o músculo e **segure a força**; então clique **CALIBRAR**. O aparelho
   descarta o primeiro segundo (a "subida" da força) e mede sua **linha de base**
   durante alguns segundos, com uma barra de progresso.
   - Se a força variar demais, aparece **"Baseline instavel, recalibre"** — recomece
     segurando com mais firmeza e constância.
   - Se você soltar a força no meio, ele avisa **"Forca solta, recomece"**.
3. **Monitore.** Com a base fixada, a página mostra em tempo real o **RMS atual**, o
   **baseline**, a **porcentagem de fadiga**, uma barra colorida e dois gráficos
   (RMS e fadiga ao longo do tempo).
4. **PARAR** congela a medição; **RESET** limpa tudo e volta ao início.

À medida que o músculo cansa numa contração sustentada, o RMS tende a **subir** acima
da linha de base — e é isso que a barra de fadiga mostra.

### 🦖 Jogo do Dino

O dinossauro do Chrome que pula quando você contrai o músculo.

1. **Relaxe o braço** e clique **CALIBRAR REPOUSO**. Durante ~2 segundos ele mede seu
   nível parado e define um **limiar** de disparo.
2. Quando aparecer **"Pronto! Contraia para pular"**, o jogo começa. **Contraia o
   músculo** para o dino pular sobre os obstáculos.
3. Os botões **- SENSIVEL / + SENSIVEL** ajustam o quão forte precisa ser a contração
   para disparar o pulo. A barra abaixo do jogo mostra o sinal atual e a **marca
   vermelha** é o limiar.
4. Se bater num obstáculo, é **GAME OVER** — contraia de novo para reiniciar. (A tecla
   **Espaço** também pula, útil para testar sem eletrodos.)

### 〰️ Osciloscópio

Mostra o **sinal EMG bruto** (valor do ADC, de 0 a 4095) rolando na tela ao vivo. Não
precisa calibrar: contraia e relaxe o músculo para ver a atividade aumentar e diminuir.
É o modo mais direto para *ver* o que o sensor está captando.

> Em qualquer modo, se um eletrodo perder contato, a página e o OLED avisam
> **"Eletrodo solto!"**.

---

## Como funciona por dentro

Um resumo rápido (a explicação completa está em [base_teorica.md](base_teorica.md)):

- **Amostragem a 1000 Hz.** Um timer de hardware lê o sinal a cada 1 ms. O EMG vive
  entre 20 e 500 Hz, então 1000 Hz atende ao critério de Nyquist com folga.
- **RMS como medida de intensidade.** O sinal bruto parece ruído; ler ponto a ponto não
  diz nada. O RMS mede a *intensidade* da oscilação — é o estimador padrão de amplitude
  em EMG. O modo Fadiga usa janelas de 256 amostras (~256 ms); o Jogo usa janelas
  curtas de 64 amostras (~64 ms) para reagir rápido.
- **Fadiga = quanto o RMS subiu.** A linha de base é a mediana das janelas coletadas na
  calibração (robusta a picos isolados), e só é aceita se for estável o bastante
  (coeficiente de variação abaixo de 0,35). Numa contração submáxima sustentada, o
  músculo recruta mais fibras para manter a força, e o RMS sobe. A fadiga em % mede esse
  aumento sobre a base.
- **O jogo usa detecção de contração com histerese.** Depois de medir o repouso, ele
  define dois limiares (um alto e um baixo). O pulo dispara quando o RMS cruza o limiar
  **alto**, e só "rearma" quando volta abaixo do **baixo** — assim uma única contração
  gera um único pulo, sem tremular.
- **Servidor web + polling.** O ESP32 serve uma página única; o navegador pergunta os
  valores em intervalos curtos (`/dados` para fadiga e jogo, `/scope` para o
  osciloscópio) e redesenha os gráficos com Canvas. O ESP só processa o modo ativo.

---

## Solução de problemas

| Sintoma | Causa provável | O que fazer |
|---|---|---|
| A tela OLED não acende | Placa/porta erradas, ou cabo USB só de carga | Confira a seleção da placa (`ESP32C3 Dev Module`) e use um cabo USB de dados |
| A tela mostra **"WiFi FALHOU"** | SSID/senha errados, ou rede 5 GHz | Corrija as linhas `SSID`/`SENHA` no código e reenvie; use uma rede **2.4 GHz** |
| A página web não abre | Celular/PC em outra rede, ou IP digitado errado | Confirme que está na **mesma rede WiFi** e digite o IP que aparece na tela OLED |
| O Monitor Serial não mostra nada | `USB CDC On Boot` desabilitado | Habilite em **Ferramentas → USB CDC On Boot** e recarregue |
| O valor fica **travado em 4095** | Eletrodo solto **ou** o módulo AD8232 sem alimentação de verdade (jumper 3.3V/GND folgado) | Confirme com multímetro que os 3.3V chegam ao módulo; refaça os jumpers de alimentação; verifique os eletrodos. **Esta foi a causa raiz no nosso caso.** |
| Aparece "Eletrodo solto!" o tempo todo | Um dos três eletrodos sem contato | Recole os eletrodos, limpe a pele, garanta que o **verde (referência)** está bem colado |
| "Baseline instavel, recalibre" na fadiga | A força variou demais durante a calibração | Recalibre segurando uma força **constante** |
| No jogo, o dino não pula (ou pula sozinho) | Limiar mal calibrado | Clique **CALIBRAR REPOUSO** com o braço relaxado e ajuste com **- / + SENSIVEL** |

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
  viável — e daria um quarto modo na página.
- **Sensor de EMG dedicado.** Trocar o AD8232 por um sensor com a banda correta (20–500
  Hz) capturaria o sinal completo.
- **Mais modos no menu.** A estrutura da página é extensível: para um novo modo, basta
  um card no HTML, um ramo na rota `/modo` e uma função `processarXxx()` no firmware.

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

- [`firmware/`](firmware) — o firmware que roda no ESP32 (fase 5d: menu web com os três
  modos — fadiga, jogo do dino e osciloscópio — mais a tela OLED).
- [`codigos/`](codigos) — sketches utilitários usados para testar e depurar o sensor.

---

## Referências

A lista completa de referências (datasheet do AD8232, artigos sobre EMG e fadiga,
bibliotecas usadas) está em **[referencia.md](referencia.md)**.
