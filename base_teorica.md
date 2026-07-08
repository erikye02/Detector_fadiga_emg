BASE TEÓRICA — EMG, FADIGA E O SISTEMA DE CÁLCULO
========================================

Antes de montar qualquer coisa, precisávamos entender o que é o sinal do músculo.
Estas anotações reúnem o que ficou claro ao longo do projeto.


O SINAL EMG
----------------------------------------

Quando o cérebro ordena uma contração, dispara impulsos elétricos pelos nervos até
as fibras musculares, e cada fibra que dispara gera uma pequena corrente. A soma
dessas correntes é captada por eletrodos na pele — é a eletromiografia de
superfície (EMG), o sinal que o AD8232 amplifica.

  - Faixa de frequência: 20 Hz a 500 Hz
  - Amplitude típica: de uma fração de mV a poucos mV (depois de amplificado)
  - O sinal bruto parece ruído, mas tem estrutura estatística que revela o estado
    do músculo, desde que não tentemos lê-lo ponto a ponto

Para capturá-lo corretamente, o critério de Nyquist pede amostrar a pelo menos
1000 Hz. É o que o firmware faz: um timer de hardware dispara a cada 1 ms (1000 Hz)
e lê uma amostra. Cada cálculo de RMS reúne 256 amostras, cerca de 256 ms de sinal.

Fontes: datasheet AD8232; documentação ESP32 (ver referencia.md).


O QUE ACONTECE NA FADIGA
----------------------------------------

Fadiga, no sentido fisiológico, é a perda da capacidade de gerar força mesmo com o
esforço mantido. No nível celular, três coisas acontecem quase ao mesmo tempo:

  - Metabólitos se acumulam (ácido lático, potássio)
  - A velocidade de condução do impulso na fibra diminui
  - O sistema nervoso recruta mais unidades motoras para compensar — por isso o
    músculo treme perto da exaustão

O ponto que nos confundiu no início foi supor que tudo isso tem uma resposta única
no sinal. Não tem:

  - A FREQUÊNCIA cai de forma confiável (a condução mais lenta desloca o espectro
    para baixo)
  - A AMPLITUDE depende do protocolo de contração:
      · Submáxima sustentada: o RMS costuma SUBIR no início (recrutamento
        compensatório) e só cai na exaustão
      · Próxima da máxima, mantida até o limite: o RMS tende a cair desde cedo,
        porque não há unidades motoras de reserva

Essa diferença é sutil, mas teve consequência no projeto: a lógica de detecção foi
escrita assumindo que a amplitude sempre cai.

Fontes: De Luca (1997); Healthcare Bulletin (2023); PMC5405568 (ver referencia.md).


POR QUE USAMOS O RMS
----------------------------------------

O RMS mede a intensidade de um sinal que oscila. A média simples de algo que sobe e
desce fica perto de zero e não serve; o RMS resolve elevando cada valor ao
quadrado, tirando a média e depois a raiz.

A escolha não foi só prática. Na literatura de EMG o RMS é o estimador padrão de
amplitude, e por dois motivos que se sustentam. Como ele corresponde à potência do
sinal, carrega um significado físico claro — foi assim que De Luca justificou, para
contrações voluntárias, que o valor RMS é o mais apropriado. E sob força constante,
sem fadiga, quando o EMG se comporta como ruído gaussiano, o RMS é a estimativa de
máxima verossimilhança da amplitude, não uma aproximação qualquer. Phinyomark e
colegas ainda o situam entre as características de domínio do tempo, de baixo custo
computacional e adequadas a processamento em tempo real, o que fecha bem com um
microcontrolador como o ESP32-C3.

  - Subtraímos antes o centro do sinal, usando a média dinâmica da própria janela,
    não um valor fixo — o centro varia com a pessoa e a posição dos eletrodos
  - Janela de 256 amostras: tempo suficiente para um número estável, curta o
    bastante para não atrasar a resposta
  - 256 é potência de dois, o que ajudaria caso acrescentássemos uma FFT no futuro

Fontes: De Luca (1997); Phinyomark et al. (2012); Delsys, RMS EMG envelope (ver referencia.md).


POR QUE NÃO USAMOS A FREQUÊNCIA MEDIANA (MDF)
----------------------------------------

A frequência mediana (MDF) é o ponto que divide a energia do espectro ao meio. Ela
cai com a fadiga e é mais robusta que a amplitude, porque não depende tanto do
protocolo. Mesmo assim, deixamos de lado:

  - Não foi por falta de amostragem — a 1000 Hz com 256 pontos daria para uma FFT
    razoável
  - Foi por simplicidade: implementar e depurar FFT num microcontrolador dá bem
    mais trabalho que um RMS
  - Para detecção qualitativa de fadiga, o RMS já era suficiente

A MDF ficou anotada como caminho possível para o futuro.

Fonte: Pflanzer et al. (2023) (ver referencia.md).


LIMITAÇÕES
----------------------------------------

  - Não é equipamento médico: a leitura é qualitativa, não dá número clínico
  - Não separa fadiga periférica (músculo) de central (sistema nervoso) — as duas
    se parecem no EMG de superfície
  - Sensível ao contato: eletrodo mal colado, pele oleosa ou cabo em movimento
    viram ruído que afeta o RMS
  - A amostragem (1000 Hz) é adequada; a limitação real é de processamento — só
    extraímos amplitude, não análise de frequência


RESUMO
----------------------------------------

  - O EMG mora entre 20 e 500 Hz; amostramos a 1000 Hz e calculamos RMS em janelas
    de 256 amostras
  - Na fadiga, a frequência cai sempre; a amplitude (RMS) depende do protocolo
  - Usamos RMS por simplicidade; a MDF seria viável e fica para o futuro
  - O sistema é qualitativo e foi pensado para aprendizado, não para uso clínico

Referências completas em referencia.md.
