# Sofle sem fio (PandaKB) com ZMK + DYA Studio

🇬🇧 [English](README.md) · 🇧🇷 Português

Firmware do meu **Sofle RGB MX sem fio** (placa PandaKB, controladores nRF52840), feito com o
[ZMK](https://zmk.dev) e configurável em tempo real pelo
[DYA Studio](https://studio.dya.cormoran.works/).

O que este repositório tem de diferente de um `zmk-config` comum:

- roda no **fork do ZMK do cormoran (`main+dya`, Zephyr 4.1)**, o que liga o DYA Studio
  (keymap, macros, combos e dial editáveis pelo navegador, sem recompilar);
- uma **tela de status própria para o OLED em pé** (Bluetooth, raio de carga, bateria com nível,
  camada e modificadores), escrita em LVGL 9 porque o `zmk-nice-oled` não compila no Zephyr 4.1;
- o registro, num só lugar, de **tudo que deu errado na montagem** e como foi resolvido
  (seção [Armadilhas de hardware](#armadilhas-de-hardware)).

> Projeto pessoal, testado só no meu teclado. O fork `main+dya` do ZMK é experimental e o
> `west.yml` aponta para branches (não para commits fixos): uma compilação nova pode puxar
> versões diferentes. **Guarde sempre os `.uf2` que funcionam.**

## Sumário

- [Início rápido](#início-rápido)
- [Como funciona](#como-funciona)
- [Personalizar](#personalizar)
- [Solução de problemas](#solução-de-problemas)
- [Hardware](#hardware)
- [Branches](#branches)
- [Camadas do keymap](#camadas-do-keymap)
- [Gravar o firmware](#gravar-o-firmware)
- [DYA Studio](#dya-studio)
- [Tela do OLED](#tela-do-oled)
- [Compilar](#compilar)
- [Armadilhas de hardware](#armadilhas-de-hardware)
- [Notas sobre o Zephyr 4.1](#notas-sobre-o-zephyr-41)
- [Limitações conhecidas](#limitações-conhecidas)
- [Créditos](#créditos)

## Início rápido

Mesmo teclado que o meu (PandaKB Sofle RGB MX com controladores nRF52840)?

1. **Pegue o firmware.** No GitHub abra *Actions*, escolha a última execução verde do branch
   `dya-tela` e baixe o artefato `firmware`. Ele tem três arquivos: `settings_reset_dya.uf2`,
   `Sofle_left_oled_dya.uf2` e `Sofle_right_oled_dya.uf2`.
2. **Grave as duas metades** seguindo [Gravar o firmware](#gravar-o-firmware) (sempre
   `settings_reset` primeiro).
3. **Pareie** o teclado com o computador por Bluetooth, ou ligue o esquerdo por USB.
4. **Personalize** ao vivo em <https://studio.dya.cormoran.works/> (veja [DYA Studio](#dya-studio)).

Outro hardware? Leia antes [Como funciona](#como-funciona) e [Personalizar](#personalizar): o
mapa de pinos fica nos arquivos do shield e precisará de ajuste.

## Como funciona

```
 computador ⇄ USB ou Bluetooth ⇄ metade ESQUERDA (central) ⇄ Bluetooth ⇄ metade DIREITA (periférico)
                                  │  keymap, camadas, macros, combos, DYA Studio
```

- Cada metade lê as próprias teclas. A **metade direita só informa as posições das teclas** por
  Bluetooth.
- A **metade esquerda decide o que cada tecla significa** (camadas, mod-taps, macros) e envia o
  resultado ao computador como um teclado comum.
- Consequências: só a esquerda conecta no computador e no DYA Studio; a direita não funciona como
  teclado sozinha; o OLED direito não sabe qual é a camada ativa.
- Edições do keymap feitas no DYA Studio **ficam gravadas no teclado**, não neste repositório. O
  arquivo `.keymap` é só o ponto de partida depois de um `settings_reset`.

### Mapa do repositório

| Caminho | O que é |
|---|---|
| `config/Sofle.keymap` | camadas, teclas e comportamento dos dials (o que você mais edita) |
| `config/Sofle.conf` | ajustes das duas metades: Bluetooth, sono, RGB, memória do display |
| `config/west.yml` | quais ZMK, Zephyr e módulos DYA são baixados |
| `build.yaml` | quais firmwares o CI compila (esquerdo, direito, `settings_reset`) |
| `boards/shields/Sofle/` | o shield: pinos (`*.overlay`), OLED e LEDs (`Sofle.dtsi`), opções por metade (`Sofle_L.conf`, `Sofle_R.conf`) |
| `src/status_screen.c`, `Kconfig`, `CMakeLists.txt`, `zephyr/module.yml` | a tela própria do OLED, empacotada como módulo do Zephyr |
| `.github/workflows/build.yml` | o build do CI |

As outras pastas de `boards/shields/` (`Nice_view_config`, `Sofle_dongle`,
`eyeslash_dongle_config`) sobraram do modelo de onde este repositório partiu e não são usadas.

### Glossário

| Termo | Significado |
|---|---|
| **ZMK** | firmware livre para teclados sem fio, feito sobre o Zephyr |
| **Zephyr** | o sistema operacional de tempo real em que o ZMK roda |
| **central / periférico** | a metade que fala com o computador / a que só informa teclas |
| **shield** | a descrição de uma placa de teclado no ZMK (pinos, matriz, display, LEDs) |
| **`west`** | ferramenta do Zephyr que baixa as fontes listadas no `west.yml` |
| **UF2** | arquivo de firmware que você copia no disco USB do controlador |
| **`settings_reset`** | um firmware mínimo que apaga pareamentos e ajustes salvos |
| **DYA Studio** | aplicativo no navegador para editar keymap, macros, combos e dials ao vivo |
| **LVGL** | a biblioteca gráfica usada na tela do OLED |

## Personalizar

| Quero… | Faça isto |
|---|---|
| trocar uma tecla por enquanto | DYA Studio → Keymap (fica gravado no teclado) |
| trocar uma tecla de vez | edite `config/Sofle.keymap`, suba, grave o novo build (depois `settings_reset`) |
| mudar o que o dial faz | DYA Studio → *Rotary Encoder Configuration*, ou edite `rsr_vol` / `rsr_pg` no keymap |
| mudar o tempo de ociosidade e sono | `CONFIG_ZMK_IDLE_TIMEOUT` (120000 ms, 2 minutos) e `CONFIG_ZMK_IDLE_SLEEP_TIMEOUT` (900000 ms) em `config/Sofle.conf` |
| mudar o padrão do RGB | `CONFIG_ZMK_RGB_UNDERGLOW_*` em `config/Sofle.conf` |
| mudar o nome do Bluetooth | `CONFIG_ZMK_KEYBOARD_NAME` em `config/Sofle.conf` |
| mudar a tela do OLED | `src/status_screen.c`; inverta `CONFIG_SOFLE_STATUS_SCREEN_ROTATE_CW` se estiver de cabeça para baixo |
| usar outra ligação do controlador | edite os pinos `pro_micro` em `boards/shields/Sofle/Sofle_L.overlay` e `Sofle_R.overlay` |

## Solução de problemas

| Sintoma | Causa provável e correção |
|---|---|
| A metade direita não digita | Ela é periférico: veja se o OLED direito mostra `OK` ao lado do ícone de Bluetooth. Rode `settings_reset` nas **duas** metades, regrave, pareie o computador de novo e espere alguns minutos |
| O computador diz "Conectado" mas nada digita | chaves velhas depois de um reset: remova o teclado do Bluetooth e pareie de novo |
| LEDs e OLED apagados juntos | o `ext_power` foi desligado (camada LOWER, primeira tecla da fileira da Tab, ou ADJUST). Aperte a tecla de novo, ou regrave |
| Só o OLED apagado | os 4 jumpers do OLED estão na face errada, ou um fio do display soltou (veja [Armadilhas de hardware](#armadilhas-de-hardware)) |
| A tela pisca e fica com chuvisco | o LVGL ficou sem memória e o teclado reinicia: mantenha os dois ajustes de memória de [Tela do OLED](#tela-do-oled) |
| Caps Lock travado | segure RAISE e aperte G |
| O DYA Studio não conecta | use um navegador Chromium por USB, com cabo de dados direto no computador (sem hub) |
| Uma coluna ou linha morta | solda fria no pino da base da PCB (onde o controlador encaixa); resolde primeiro e só depois suspeite do controlador |

## Hardware

| Item | Detalhe |
|---|---|
| Placa | [PandaKB Sofle RGB MX](https://pandakb.com/guides/sofle-rgb-mx-build-guide) (hot-swap, PCB **reversível**) |
| Controladores | clones **SuperMini nRF52840** (bootloader UF2 do nice!nano). No ZMK usa-se a placa `nice_nano` |
| Display | OLED 128×32 SSD1306 (I²C), **montado em pé** na placa |
| LEDs | 72 × SK6812MINI-E (36 por metade: 1 indicador, 6 underglow, 29 por tecla) |
| Encoders | 2 × EC11 (dial esquerdo e direito) |
| Energia | LiPo por metade, ligada ao B+/B- (o conector CN1 da placa não funcionou na minha; ver abaixo) |
| Conexão | Bluetooth; as metades falam entre si por BLE (o cabo TRRS não é usado no ZMK) |

O **esquerdo é o central** (fala com o computador e com o DYA Studio) e o **direito é
periférico**. O periférico não aparece como teclado no USB.

## Branches

| Branch | Para quê |
|---|---|
| `main` | estado original do repositório |
| `pandakb-shield` | firmware **ZMK v0.3** (Zephyr 3.5) com o shield PandaKB e a tela `nice_oled`. É a volta segura |
| `dya-teste` | primeira migração para o fork `main+dya` (usa a tela embutida do ZMK) |
| **`dya-tela`** | **o branch em uso**: tudo do `dya-teste` + dial configurável + tela própria |

## Camadas do keymap

O keymap fica em [`config/Sofle.keymap`](config/Sofle.keymap).

```
Polegar esquerdo (fora → centro):  GUI  ALT  CTRL  LOWER  ENTER
Polegar direito  (centro → fora):  SPACE  RAISE  CTRL  ALT  GUI
```

| Camada | Como chegar | Para quê |
|---|---|---|
| 0 `base` | — | letras, números, modificadores |
| 1 `lower` | segurar **LOWER** | `F1`–`F12`, números, símbolos (`! @ # $ % ^ & * ( )`), `[ ] { } = - + \ ;` |
| 2 `raise` | segurar **RAISE** | setas, `Del`, `PgUp`/`PgDn`, copiar/colar/desfazer, **`Caps Lock` (RAISE + G)**, perfis Bluetooth |
| 3 `adjust` | segurar **LOWER + RAISE** | perfis BT, `BT_CLR`, energia e efeitos do RGB |

Os dials começam como **volume** (esquerdo) e **PgUp/PgDn** (direito), em todas as camadas, mas
podem ser trocados por camada no DYA Studio (aba Keymap → *Rotary Encoder Configuration*).

> Cuidado com o `BT_CLR` (canto superior esquerdo da camada RAISE): apaga os pareamentos.
> E com `&ext_power EP_TOG`: ele fica na **camada LOWER, primeira tecla da fileira da Tab**, e na
> ADJUST. Corta a energia dos LEDs e do OLED, e **o
> estado fica gravado no teclado** (tirar o USB não religa; aperte a tecla de novo).

### Cores dos LEDs

As cores mudam na camada **AJUSTE** (segure **LOWER + RAISE**):

```
Fileira da Esc:  [Esc]       Q        W        E         R         T
                 EXT_POWER   Matiz-   Matiz+   Satur.-   Satur.+   Efeito
Fileira da Tab:  [Tab]       A        S        D
                 (nada)      Brilho-  Brilho+  Liga/desliga RGB
```

O firmware começa no efeito 3 (arco-íris girando), em que as teclas de matiz não têm o que mudar.
**Aperte `T` uma vez para ir ao efeito 0 (cor fixa)** e use `Q`/`W` para escolher a cor.
Efeitos: 0 cor fixa, 1 respirando, 2 espectro, 3 arco-íris. Os ajustes ficam salvos em cada
metade, então faça nas duas.

## Gravar o firmware

Os `.uf2` saem do GitHub Actions (artefato `firmware`) ou da [compilação local](#compilar).

Para cada metade:

1. USB ligado, **dois toques rápidos no botão de reset**. Aparece um disco (tipo `NICENANO`).
2. Copie o `settings_reset_dya.uf2`. A placa reinicia sozinha.
3. Dois toques no reset de novo.
4. Copie o `Sofle_left_oled_dya.uf2` (esquerdo) ou o `Sofle_right_oled_dya.uf2` (direito).

Regras que economizam tempo:

- **`settings_reset` nas duas metades** quando trocar de firmware ou de controlador. Sem isso o
  esquerdo guarda o pareamento do controlador antigo do direito e não fala com o novo.
- No computador, remova o teclado do Bluetooth e pareie de novo depois de um `settings_reset`:
  o BlueZ continua mostrando "Connected" com chaves velhas, sem entrada de teclado.
- A conexão inicial entre as metades pode **demorar alguns minutos**.
- O `settings_reset` também apaga o que foi editado no DYA Studio (isso fica no teclado,
  não volta para o `.keymap`).
- Montar o disco no Linux sem sudo: `udisksctl mount -b /dev/sdX` (confira com `lsblk`).

## DYA Studio

Abra <https://studio.dya.cormoran.works/> em um navegador **Chromium** e conecte o esquerdo.

- **USB** é o caminho confiável. No Linux a porta serial precisa estar liberada para o seu
  usuário (uma regra `udev` com `uaccess` para `1d50:615e`).
- No meu computador funcionou no **Helium**. No Brave a conexão foi cancelada e o site avisou
  *"BLE Not Supported"*. Firefox não tem Web Bluetooth.
- **Bluetooth** no Linux exige `chrome://flags/#enable-experimental-web-platform-features`, e o
  teclado já pareado como teclado pode não aparecer na lista de dispositivos.
- Se aparecer *"Runtime sensor rotation subsystem is not available"*, falta o módulo
  `zmk-behavior-runtime-sensor-rotate` (já está neste `west.yml`).
- O `OS Layout` do site é só visual; não muda o que o teclado envia.

O central usa `CONFIG_ZMK_STUDIO_LOCKING=n`, então não é preciso apertar uma tecla de
desbloqueio.

## Tela do OLED

[`src/status_screen.c`](src/status_screen.c) desenha uma tela de 32×128 em um *canvas* do LVGL 9 e
gira 90° para o OLED montado em pé.

| | Esquerdo (central) | Direito (periférico) |
|---|---|---|
| Linha 1 | `USB`, ou ícone Bluetooth + perfil (`1*` conectado, `1-` desconectado) | ícone Bluetooth + `OK`/`--` (link com o esquerdo) |
| Raio | USB ligado | USB ligado |
| Bateria | ícone com nível + `%` | ícone com nível + `%` |
| Camada | nome (4 letras: `BASE`, `LOWE`, `RAIS`, `ADJU`) | o mesmo nome, **enviado pela metade esquerda** (`----` até a primeira atualização) |
| Modificadores | `CTL SFT ALT GUI`, caixa cheia = apertado | idem: as **posições das teclas** do próprio lado mais os modificadores **enviados pela metade esquerda** |

- **Camada e modificadores no lado direito** viajam do central pelos eventos de relay do ZMK
  (`CONFIG_ZMK_SPLIT_RELAY_EVENT`, em `Sofle_L.conf` e `Sofle_R.conf`, com o mesmo `DATA_LEN`).
  O [`src/status_events.c`](src/status_events.c) define o evento `sofle_status_changed` e o repassa;
  para poupar a bateria do direito, o central só reenvia o estado quando a camada ou os
  modificadores mudam e quando uma tecla do lado direito é apertada.
- Se a tela sair de cabeça para baixo, troque `CONFIG_SOFLE_STATUS_SCREEN_ROTATE_CW`
  (em [`Kconfig`](Kconfig)).
- No direito, as posições são `49` (RShift), `57` (RCtrl), `58` (RAlt) e `59` (RGui), válidas
  para o keymap de 60 posições. Mudou o layout, mude os números em `periph_track()`.
- **Memória:** o pool padrão do LVGL (2 KB) e a pilha da thread do display (3 KB) não bastam
  para desenhar com canvas. Em `config/Sofle.conf` estão `CONFIG_LV_Z_MEM_POOL_SIZE=16384` e
  `CONFIG_ZMK_DISPLAY_DEDICATED_THREAD_STACK_SIZE=6144`. Sem isso o teclado **reinicia em loop**
  (a tela pisca e fica "chuviscada"), porque os asserts do LVGL vêm desligados e a falta de
  memória vira ponteiro nulo.

## Compilar

### GitHub Actions

Um *push* dispara o workflow ([`.github/workflows/build.yml`](.github/workflows/build.yml)), que
usa o workflow oficial do ZMK. Os firmwares saem no artefato `firmware`.

### Local, com Docker (sem subir nada)

Usa a mesma imagem do Actions (`zmkfirmware/zmk-build-arm:stable`). O workspace do `west` tem uns
3 GB e fica fora deste repositório.

```bash
mkdir -p ~/zmk-build && cd ~/zmk-build
cp -r /caminho/do/repo/config ./config

# 1) baixar ZMK, Zephyr e módulos (uma vez)
docker run --rm -u $(id -u):$(id -g) -e HOME=/tmp \
  -v ~/zmk-build:/ws zmkfirmware/zmk-build-arm:stable bash -c \
  'cd /ws && west init -l config && west update --fetch-opt=--filter=tree:0 -o=--depth=1'

# 2) compilar (L = esquerdo com Studio, R = direito)
docker run --rm -u $(id -u):$(id -g) -e HOME=/tmp \
  -v ~/zmk-build:/ws -v /caminho/do/repo:/repo zmkfirmware/zmk-build-arm:stable bash -c \
  'cd /ws && west zephyr-export && west build -s zmk/app -d build/L -b "nice_nano//zmk" \
     -S studio-rpc-usb-uart -- -DSHIELD=Sofle_L -DZMK_CONFIG=/ws/config -DZMK_EXTRA_MODULES=/repo'
# resultado: build/L/zephyr/zmk.uf2
```

Para o direito, troque `L` por `R` e tire o `-S studio-rpc-usb-uart`. O `west zephyr-export`
precisa rodar em cada `docker run`, porque o `HOME=/tmp` do contêiner some.

Este repositório é um **módulo do Zephyr** ([`zephyr/module.yml`](zephyr/module.yml)): o
`CMakeLists.txt` e o `Kconfig` da raiz compilam a tela própria, e `boards/shields/Sofle` define o
shield.

## Armadilhas de hardware

Tudo isso foi encontrado na prática; foi o que mais custou tempo.

**A PCB é reversível: confira sempre a face certa.**
- Os **4 jumpers do OLED** são feitos na **face de cima** (a mesma dos sockets das chaves), logo
  abaixo da pegada do controlador. Feitos do lado errado, o OLED não acende.
- Cada LED tem pegada nas duas faces. Um LED soldado na face errada e retirado pode arrancar a
  trilha. O canto marcado do LED deve coincidir com a marca da pegada.

**Cadeia de LEDs.** A ordem é a numeração do guia da PandaKB (1 = indicador perto do TRRS, 2–7
underglow, 8–36 teclas). Se a cadeia para no LED *N*, suspeite **primeiro do último aceso (N)**: o
`DOUT` morto ou mal soldado, e só depois do primeiro apagado (N+1). Reparos que funcionaram: trocar
o LED *N*; fio fino do `DOUT` do *N* ao `DIN` do *N+1*.

**Linha ou coluna morta, ou controlador "morto": confira primeiro a solda na PCB, depois o controlador.**
Por dias culpei os controladores e depois achei **pinos da base mal soldados na própria PCB**, onde
o controlador encaixa. Os dois podem estar com defeito (os controladores são clones baratos) e eu
não consegui separar uma causa da outra, então trate como uma soma de causas.
- pino com **solda fria / sem escorrer** (ou perna torta) deixa uma **linha ou coluna morta**:
  fileira dos polegares no `pro_micro 9` (P1.06) e coluna `0 P ; /` no `pro_micro 18` (P1.15,
  etiqueta `115`). Resoldar resolveu;
- trocar o controlador pode parecer a solução porque reassenta os contatos, então isso não prova
  que o controlador está com defeito (nem que está bom); refaça a solda dos pinos da base primeiro,
  que é o teste mais barato;
- um controlador que parece não alimentar o OLED nem os LEDs pode ser solda fraca nos pinos de
  energia da base, ou um controlador ruim (não medido);
- cuidado: a etiqueta do pino (`009`) **não** é o número do `pro_micro` (`9`).

**Bateria.** O soquete **CN1** (embaixo do controlador) não passou energia na minha placa.
Ligar a bateria direto nos pads **B+/B- da borda da PCB** resolveu. **Não** faça jumper no `RAW`: é
a entrada de 5 V do USB e pula o carregador. Confira a polaridade com multímetro e isole com Kapton.

**Teste de matriz do lado direito.** O periférico não vira teclado no USB. Para testar a matriz
dele, grave um build temporário como central (`-DCONFIG_ZMK_SPLIT_ROLE_CENTRAL=y`) e depois
regrave o firmware normal.

**Sintomas de enumeração.** Um `lsusb` que lista o `1d50:615e` **sem nenhuma interface** (e nenhum
`/dev/input` ou `ttyACM0`) quer dizer que a enumeração falhou; costuma ser hub USB. Use uma porta
direta da placa-mãe e um cabo de dados.

## Notas sobre o Zephyr 4.1

Ao migrar do ZMK v0.3 para o fork `main+dya`, estas coisas quebraram e foram corrigidas:

| Sintoma | Causa | Solução |
|---|---|---|
| `Aborting due to Kconfig warnings` | `CONFIG_WS2812_STRIP` não existe mais | apagar as linhas (o driver vem pelo devicetree) |
| mesmo erro, em `BATTERY_LEVEL_FETCHING`/`PROXY` | opções só do central, atribuídas ao periférico | tirar do `Sofle_R.conf` |
| `LV_IMG_CF_*` / `lv_draw_img_dsc_t` não declarados | `zmk-nice-oled` usa LVGL 8; o Zephyr 4.1 usa LVGL 9 | remover o módulo e usar a tela embutida ou a própria |
| OLED só com blocos | `CONFIG_LV_FONT_DEFAULT_MONTSERRAT_26` herdada do nice_view (26 px num display de 32 px) | tirar as duas linhas de fonte |
| nome da placa | `nice_nano_v2` virou `nice_nano//zmk` | ajustar o `build.yaml` |
| `zmk/display.h: No such file` ao compilar o módulo | faltam os includes do ZMK | `zephyr_library_include_directories(${CMAKE_SOURCE_DIR}/include)` |
| `lv_draw_triangle_dsc_t` desconhecido | header fora do `lvgl.h` e campos `bg_color`/`bg_opa` | `#include "draw/lv_draw_triangle.h"` |

## Limitações conhecidas

- Sem as animações do `nice_oled` (gato, vim). A tela própria mostra só o essencial.
- Nome da camada com 4 letras na tela.
- O lado direito acende na hora as próprias teclas modificadoras, pela posição física, e depois
  segue o estado repassado pelo central; um modificador feito por `&mt` ou similar nessa metade só
  aparece depois que o central responde.
- O fork `main+dya` é experimental e sem versão fixa.
- RGB, OLED e o shield PandaKB só foram testados no meu hardware.

## Créditos

- [ZMK Firmware](https://github.com/zmkfirmware/zmk) (MIT) e o fork
  [`cormoran/zmk`](https://github.com/cormoran/zmk) com os módulos do
  [DYA Studio](https://github.com/cormoran/dya-studio).
- Shield do Sofle: Ryan Cross e colaboradores do ZMK; ajustes para a placa e guia de montagem da
  [PandaKB](https://pandakb.com/guides/sofle-rgb-mx-build-guide).
- Este repositório usa a [Licença MIT](LICENSE). Arquivos vindos de projetos de origem (como o
  shield do Sofle em `boards/shields/Sofle`) mantêm os próprios cabeçalhos; confira a licença de
  cada projeto antes de reutilizar o código.
