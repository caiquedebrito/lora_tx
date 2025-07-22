# Estação Meteorológica Inteligente

## 📋 Visão Geral

Este projeto oferece uma solução completa e prática para monitorar em tempo real condições ambientais como temperatura, umidade e pressão atmosférica. É ideal para ambientes que necessitam monitoramento constante e ações rápidas em caso de desvios nos parâmetros estabelecidos.

## 🎯 Objetivo Geral

O projeto visa desenvolver uma plataforma compacta e eficiente para monitoramento remoto e contínuo de condições ambientais. O sistema permite que usuários acompanhem dados em tempo real, configurem limites e ajustes (offsets) para os sensores e recebam alertas visuais e sonoros em caso de desvios dos parâmetros definidos, garantindo monitoramento ambiental preciso e proativo.

## ⚙️ Funcionalidades

- ✅ Monitoramento remoto e em tempo real de temperatura, umidade e pressão
- ⚙️ Configuração remota de limites e offsets para medições mais precisas
- 🚨 Alertas visuais e sonoros ao ultrapassar limites definidos
- 🌐 Interface web interativa com gráficos dinâmicos e configuração amigável
- 📱 Dashboard responsivo com Bootstrap para diferentes dispositivos
- 📊 Gráficos em tempo real para visualização histórica dos dados

## 🔧 Componentes

### Hardware
- **Placa BitDogLab** (Raspberry Pi Pico W)
- **Sensores:**
  - AHT20 (temperatura e umidade)
  - BMP280 (temperatura e pressão atmosférica)
- **Display OLED** para visualização local
- **LED RGB** e **matriz WS2812** para alertas visuais
- **Buzzer** para alertas sonoros
- **Joystick** e **botões** para interação física

### Software
- **Pico SDK** para desenvolvimento C/C++
- **lwIP** para stack TCP/IP
- **FreeRTOS** para multitarefa
- **Interface Web** com HTML5, CSS3 e JavaScript

## 🎮 Uso dos Periféricos da BitDogLab

### Joystick
- Navegação entre diferentes telas de exibição no display OLED (dashboard, limites e offsets)

### Botões
- **Botão A:** avança entre as diferentes páginas exibidas no display OLED
- **Botão B:** reinicia o sistema no modo BOOTSEL para atualizações de firmware

### Display OLED
- Exibe informações sobre temperatura, umidade, pressão
- Mostra status atual (normal, acima ou abaixo dos limites)
- Exibe limites configurados e offsets aplicados

### Matriz de LEDs (WS2812)
Indica visualmente o status do sistema:
- 🟢 **Verde:** condições normais
- 🟡 **Amarelo:** parâmetros abaixo dos limites
- 🔴 **Vermelho:** parâmetros acima dos limites superiores

### LED RGB
- 🟢 **LED verde:** valores normais
- 🔴 **LED vermelho:** valores fora dos limites definidos

### Buzzer
Emite alertas sonoros com diferentes padrões:
- **Toque intermitente:** valores abaixo dos limites mínimos
- **Toque contínuo:** valores acima dos limites máximos

## 🔄 Como Clonar e Executar o Projeto

### Pré-requisitos

1. **Ambiente de Desenvolvimento:**
   ```bash
   # Instalar Pico SDK
   git clone https://github.com/raspberrypi/pico-sdk.git
   cd pico-sdk
   git submodule update --init
   export PICO_SDK_PATH=$(pwd)
   ```

2. **Ferramentas Necessárias:**
   - CMake (versão 3.13 ou superior)
   - GCC ARM Embedded Toolchain
   - Visual Studio Code com extensão Raspberry Pi Pico

### Clonando o Repositório

```bash
# Clone o repositório
git clone https://github.com/caiquedebrito/estacao_meteriologica
cd estacao_meteriologica

# Prepare o ambiente de build
mkdir build
cd build
```

### Configuração

1. **Configure as credenciais Wi-Fi** no arquivo principal:
   ```c
   #define WIFI_SSID "SUA_REDE_WIFI"
   #define WIFI_PASSWORD "SUA_SENHA_WIFI"
   ```

2. **Configure os pinos dos sensores** se necessário no arquivo de configuração.

### Compilação

```bash
# No diretório build
cmake ..
make -j4
```

### Upload para o Dispositivo

1. **Modo BOOTSEL:**
   - Mantenha o botão BOOTSEL pressionado
   - Conecte o Raspberry Pi Pico W ao computador
   - Solte o botão BOOTSEL

2. **Upload do firmware:**
   ```bash
   # Copie o arquivo .uf2 para o dispositivo
   cp estacao_meteriologica.uf2 /path/to/RPI-RP2/
   ```

   Ou use o picotool:
   ```bash
   picotool load estacao_meteriologica.uf2 -fx
   ```

### Execução

1. **Inicialização:**
   - O device reiniciará automaticamente após o upload
   - Aguarde a conexão Wi-Fi (indicada no display OLED)
   - Anote o endereço IP exibido no display

2. **Acesso à Interface Web:**
   ```
   http://[IP_DO_DISPOSITIVO]
   ```

### Estrutura de URLs da Interface Web

- `/` - Dashboard principal com dados em tempo real
- `/temperatura` - Página específica de temperatura
- `/umidade` - Página específica de umidade  
- `/pressao` - Página específica de pressão
- `/limites` - Configuração de limites mínimos e máximos
- `/offset` - Configuração de offsets dos sensores

### APIs Disponíveis

- `GET /data` - Dados dos sensores em JSON
- `GET /settings/data` - Configurações atuais em JSON
- `POST /settings/limites` - Configurar limites
- `POST /settings/offset` - Configurar offsets

## 🛠️ Desenvolvimento

### Estrutura do Projeto

```
estacao_meteriologica/
├── estacao_meteriologica.c    # Arquivo principal
├── CMakeLists.txt             # Configuração do build
├── lib/                       # Bibliotecas dos sensores
│   ├── aht20.c/.h            # Driver do sensor AHT20
│   ├── bmp280.c/.h           # Driver do sensor BMP280
│   ├── ssd1306.c/.h          # Driver do display OLED
│   ├── ws2812.c/.h           # Driver da matriz de LEDs
│   └── buzzer.c/.h           # Driver do buzzer
├── *.html.h                   # Páginas web minificadas
└── README.md                  # Este arquivo
```

### Compilação Manual

Se você preferir usar as tasks do VS Code:
- `Ctrl+Shift+P` → "Tasks: Run Task" → "Compile Project"
- Para upload: "Tasks: Run Task" → "Run Project"

## 🚀 Funcionalidades Avançadas

### Monitoramento em Tempo Real
- Atualização automática a cada 2 segundos
- Gráficos dinâmicos com histórico de 30 pontos
- Interface responsiva para diferentes dispositivos

### Sistema de Alertas
- Comparação contínua com limites configuráveis
- Alertas visuais progressivos (verde → amarelo → vermelho)
- Alertas sonoros diferenciados por tipo de desvio

### Configuração Flexível
- Ajuste de limites via interface web
- Calibração com offsets individuais por sensor
- Validação de dados em tempo real

## 📞 Suporte

Para dúvidas ou problemas:
1. Verifique as conexões dos sensores
2. Confirme a configuração Wi-Fi
3. Monitore o display OLED para mensagens de status
4. Verifique a saída serial para debug detalhado