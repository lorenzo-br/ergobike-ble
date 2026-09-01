# Especificação Técnica de Design: Adaptador BLE para Ergométrica com ESP32-C3

**Data:** 31 de Agosto de 2026  
**Status:** Aprovado para Implementação  
**Autor:** Antigravity / OpenCode Pair Programming  
**Hardware Alvo:** ESP32-C3 + Célula 18650 + Conector Jack P1/P2 + Enclosure V9

---

## 1. Visão Geral e Objetivos

O objetivo deste projeto é transformar uma bicicleta ergométrica convencional sem eletrônica inteligente em um *Smart Indoor Trainer* via **Bluetooth Low Energy (BLE)** de alta precisão e baixo consumo de energia.

O sistema intercepta os pulsos gerados pelo sensor magnético de rotação (reed switch / hall) através do conector P1/P2 (3.5mm/2.5mm mono), processa os pulsos via interrupções de hardware com debounce digital e filtro de média móvel no microcontrolador ESP32-C3, e transmite dados em tempo real para aplicativos de treino indoor (como **CycleGo**, Zwift, Kinomap, Rouvy, Wahoo, Strava).

### Principais Diferenciais:
1. **Dual BLE Stack (CSCS + FTMS):** Suporte nativo ao serviço clássico *Cycling Speed and Cadence* (UUID `0x1816`) e ao padrão moderno *Fitness Machine Service* (UUID `0x1826` - Indoor Bike Data), além do *Battery Service* (UUID `0x180F`).
2. **Portal Web Wi-Fi de Calibração e Teste:** Interface responsiva hospedada no próprio ESP32 para calibrar a relação de transmissão (assistente de 10 voltas de pedal), diâmetro virtual da roda, tempos de debounce e atualização de firmware via OTA sem necessidade de cabos.
3. **Gestão Inteligente de Energia com Deep Sleep:** Consumo ultra-baixo (< 15 µA) em repouso com desligamento automático após 3 minutos de inatividade e despertar instantâneo ao primeiro giro da roda/pedal.
4. **Proteção Eletrônica e Monitoramento da Bateria 18650:** Divisor resistivo com curva de calibração para leitura precisa de tensão e proteção contra descarga profunda da célula 18650.

---

## 2. Arquitetura de Hardware e Conexões

### 2.1 Pinout do ESP32-C3

| Função | Pino ESP32-C3 | Tipo | Descrição / Circuito |
| :--- | :--- | :--- | :--- |
| **Entrada Sensor P2** | `GPIO 3` | Digital Input (Interrupt + Deep Sleep Wakeup) | Conectado à ponta do conector P2 com resistor em série de 1kΩ e Pull-Up interno ativado. Nível LOW quando ímã aproxima. |
| **GND Sensor P2** | `GND` | Terra comum | Conectado à malha do conector P2. |
| **Leitura Bateria 18650** | `GPIO 0` | ADC1_CH0 | Divisor de tensão resistivo $100\text{k}\Omega / 100\text{k}\Omega$ ligado diretamente ao polo positivo da célula 18650. |
| **Botão de Setup / Wi-Fi** | `GPIO 9` | Digital Input (BOOT) | Botão nativo BOOT. Pressionado por 3s no boot inicia o Portal Wi-Fi de Configuração. |
| **LED de Status** | `GPIO 8` | Digital Output | LED onboard / externo indicando BLE anunciando, conectado, pulso de pedal ou modo Wi-Fi. |

### 2.2 Esquema Elétrico de Proteção e Interface

```
                       ESP32-C3
                   +---------------+
[ JACK P2 ]        |               |
Ponta (Sinal) -----[ 1kΩ ]---------| GPIO 3 (PULL-UP)
                   |               |
Malha (GND)   -----+---------------| GND
                   |               |
[ BATERIA 18650 ]  |               |
Polo (+) ----------[ 100kΩ ]---+---| GPIO 0 (ADC)
                               |   |
                           [ 100kΩ ]
                               |   |
Polo (-) ----------------------+---| GND
                                   |
[ BOTÃO BOOT ]---------------------| GPIO 9 (PULL-UP)
[ LED STATUS ]---------------------| GPIO 8
                   +---------------+
```

---

## 3. Arquitetura do Firmware

O firmware é desenvolvido em C++ modular sob o framework Arduino / ESP-IDF (PlatformIO).

### 3.1 Estrutura de Diretórios

```
ergobike_ble/
├── include/
│   ├── config.h              # Definições de pinos, padrões de fábrica e constantes de tempo
│   ├── sensor_reader.h       # ISR do sensor, debounce por software, cálculo de RPM e velocidade
│   ├── ble_manager.h         # Servidor BLE, serviços CSCS (0x1816), FTMS (0x1826), Battery (0x180F)
│   ├── storage_manager.h     # Persistência em NVS Flash (Preferences)
│   ├── battery_monitor.h     # Leitura do ADC, conversão para mV e curva percentual da 18650
│   ├── power_manager.h       # Controle de inatividade, light sleep e deep sleep com wakeup
│   └── web_portal.h          # Ponto de acesso Wi-Fi, Servidor Web assíncrono, WebSockets e OTA
├── src/
│   ├── main.cpp              # Inicialização, orquestração e loop principal
│   ├── sensor_reader.cpp
│   ├── ble_manager.cpp
│   ├── storage_manager.cpp
│   ├── battery_monitor.cpp
│   ├── power_manager.cpp
│   └── web_portal.cpp
├── platformio.ini             # Configuração de build, flags de compilação e bibliotecas
└── README.md
```

---

## 4. Algoritmos e Detalhes dos Módulos

### 4.1 SensorReader (Detecção e Métricas)
- **Interrupção:** `attachInterrupt(digitalPinToInterrupt(PIN_SENSOR), isr_sensor_trigger, FALLING)`.
- **Filtro de Debounce Dinâmico:** Intervalos inferiores ao tempo de debounce configurado (padrão 15 ms) são descartados na ISR.
- **Base de Tempo:** Medição do delta de tempo ($\Delta t = t_{\text{atual}} - t_{\text{anterior}}$) em microssegundos com `esp_timer_get_time()`.
- **Filtro de Suavização:** *Moving Average* com buffer circular dos últimos 4 pulsos para compensar irregularidades na inércia mecânica.
- **Relação de Transmissão e Conversões:**
  - $\text{RPM}_{\text{pedal}} = \frac{60 \times 10^6}{\Delta t_{\mu s} \times \text{gear\_ratio}}$
  - $\text{Velocidade}_{\text{km/h}} = \left(\frac{\text{wheel\_circ}_{\text{mm}} \times 3600}{\Delta t_{\mu s} \times \text{gear\_ratio}}\right)$
- **Timeout de Parada Imediata:** Se passar mais de 2,0 segundos sem pulso, a velocidade e o RPM são zerados instantaneamente.

### 4.2 BLEManager (Protocolos de Transmissão)
- **Serviço CSCS (0x1816):**
  - Característica `0x2A5B` (*CSC Measurement* - Notify):
    - Byte 0: Flags (`0x03` = Wheel Revolutions Present + Crank Revolutions Present).
    - Bytes 1..4: `Cumulative Wheel Revolutions` (uint32).
    - Bytes 5..6: `Last Wheel Event Time` (uint16, resolução $1/1024\text{ s}$).
    - Bytes 7..8: `Cumulative Crank Revolutions` (uint16).
    - Bytes 9..10: `Last Crank Event Time` (uint16, resolução $1/1024\text{ s}$).
  - Característica `0x2A5C` (*CSC Feature* - Read): `0x03`.
  - Característica `0x2A5D` (*Sensor Location* - Read): `0x0C` (Rear Dropout / Trainer).
- **Serviço FTMS (0x1826):**
  - Característica `0x2AD2` (*Indoor Bike Data* - Notify):
    - Flags: Instantaneous Speed present, Instantaneous Cadence present.
    - Velocidade instantânea em unidades de $0.01\text{ km/h}$.
    - Cadência instantânea em unidades de $0.5\text{ RPM}$.
- **Serviço de Bateria (0x180F):**
  - Característica `0x2A19` (*Battery Level* - Read/Notify): Percentual de 0 a 100%.

### 4.3 WebPortal e Auto-Calibração
- **Rede Wi-Fi:** AP com SSID `ErgoBike-BLE-Setup` e IP `192.168.4.1`.
- **Assistente de Calibração Guiada:**
  - O usuário aciona "Iniciar Calibração" na página.
  - O usuário pedala exatamente 10 voltas completas.
  - O ESP32 conta os pulsos $N_{\text{pulsos}}$ recebidos da roda de inércia.
  - A relação é calculada automaticamente: $\text{gear\_ratio} = \frac{N_{\text{pulsos}}}{10.0}$.
- **Live WebSocket Dashboard:** Exibe RPM, Velocidade em km/h e pulsos detectados em tempo real na tela do smartphone.
- **Painel de Configuração NVS:** Ajuste de debounce, circunferência, timeout de sleep e calibração de voltagem.
- **Atualização OTA:** Endpoint `/update` para upload de novo binário de firmware pelo navegador.

### 4.4 PowerManager (Deep Sleep)
- **Critério de Inatividade:** Sem pulso no sensor e sem cliente BLE conectado por 3 minutos.
- **Procedimento de Suspensão:**
  1. Salva estado acumulado em Flash NVS.
  2. Desativa Wi-Fi e rádio BLE.
  3. Configura `gpio_wakeup_enable(GPIO_NUM_3, GPIO_INTR_LOW_LEVEL)`.
  4. Ativa `esp_deep_sleep_enable_gpio_wakeup((1ULL << GPIO_NUM_3), ESP_GPIO_WAKEUP_GPIO_LOW)`.
  5. Entra em `esp_deep_sleep_start()`.
- **Despertar:** Ao primeiro giro do volante, o ímã puxa GPIO 3 para LOW, reiniciando o ESP32 imediatamente.

---

## 5. Estratégia de Teste e Validação

1. **Testes Unitários e de Algoritmo:**
   - Validação da máquina de estados do debounce e ring buffer de média móvel com simulação de jitter e repique mecânico.
   - Validação da formatação dos bytes CSCS ($1/1024\text{ s}$ wrap-around em 64 segundos).
2. **Testes de Integração BLE:**
   - Conexão e leitura de dados no app **CycleGo** (iOS / Android).
   - Teste de compatibilidade em **Zwift** e **nRF Connect** para validação dos descritores BLE e taxas de notificação.
3. **Teste de Calibração e Persistência NVS:**
   - Verificação do cálculo do assistente de 10 voltas e persistência após reinicialização.
4. **Teste de Consumo e Transição de Sono:**
   - Medição do consumo em repouso e validação do acionamento no primeiro pulso.

---

## 6. Aprovação

Este documento estabelece todos os requisitos, arquiteturas e especificações necessárias para a implementação completa do firmware e ferramentas do projeto.
