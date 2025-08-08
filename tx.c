// lora_tx.c

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "lora.h" // <-- Incluímos o seu novo arquivo de cabeçalho!

// Definições dos Pinos (mantemos aqui para a configuração do hardware)
#define SPI_PORT spi0
#define PIN_MISO 16
#define PIN_MOSI 19
#define PIN_SCK  18
#define PIN_CS   17
#define PIN_RST  20
#define PIN_DIO0 21

// Frequência do LoRa (em Hz)
#define LORA_FREQUENCY 915E6

// --- Funções de baixo nível para SPI (sem alterações) ---
void lora_write_reg(uint8_t reg, uint8_t val) {
    uint8_t buf[2];
    buf[0] = reg | 0x80;
    buf[1] = val;
    gpio_put(PIN_CS, 0);
    spi_write_blocking(SPI_PORT, buf, 2);
    gpio_put(PIN_CS, 1);
}

uint8_t lora_read_reg(uint8_t reg) {
    uint8_t val;
    gpio_put(PIN_CS, 0);
    spi_write_blocking(SPI_PORT, &reg, 1);
    spi_read_blocking(SPI_PORT, 0, &val, 1);
    gpio_put(PIN_CS, 1);
    return val;
}

// --- Funções de alto nível do LoRa (atualizadas) ---
void lora_reset() {
    gpio_put(PIN_RST, 0);
    sleep_ms(1);
    gpio_put(PIN_RST, 1);
    sleep_ms(5);
}

void lora_set_frequency(long frequency) {
    uint64_t frf = ((uint64_t)frequency << 19) / 32000000;
    lora_write_reg(REG_FRF_MSB, (uint8_t)(frf >> 16));
    lora_write_reg(REG_FRF_MID, (uint8_t)(frf >> 8));
    lora_write_reg(REG_FRF_LSB, (uint8_t)(frf >> 0));
}

// Função de inicialização aprimorada!
void lora_init() {
    lora_reset();

    // Entra em modo Sleep para configurar o modo LoRa
    lora_write_reg(REG_OPMODE, RF95_MODE_SLEEP);
    sleep_ms(10);
    
    // Ativa o modo LoRa
    lora_write_reg(REG_OPMODE, RF95_MODE_SLEEP);
    lora_write_reg(REG_OPMODE, lora_read_reg(REG_OPMODE) | 0x80);
    sleep_ms(10);

    // Verifica se o modo LoRa foi ativado
    if (lora_read_reg(REG_OPMODE) != (RF95_MODE_SLEEP | 0x80)) {
        printf("Falha ao iniciar o modo LoRa!\n");
        while(1);
    }
    
    lora_write_reg(REG_OPMODE, RF95_MODE_STANDBY); // Volta para Standby
    
    // Configura a frequência
    lora_set_frequency(LORA_FREQUENCY);

    // Configura ponteiros da FIFO
    lora_write_reg(REG_FIFO_TX_BASE_AD, 0);
    lora_write_reg(REG_FIFO_ADDR_PTR, 0);

    // Configura LNA para ganho máximo
    lora_write_reg(REG_LNA, LNA_MAX_GAIN);

    // *** Usando as macros do seu lora.h ***
    // Configura parâmetros do modem: Header explícito, CR 4/5, BW 125kHz
    lora_write_reg(REG_MODEM_CONFIG, EXPLICIT_MODE | ERROR_CODING_4_5 | BANDWIDTH_125K);
    // Configura SF 7 e CRC ativado
    lora_write_reg(REG_MODEM_CONFIG2, SPREADING_7 | CRC_ON);
    // Habilita LNA AGC
    lora_write_reg(REG_MODEM_CONFIG3, 0x04); // LnaGain set by REG_LNA, LnaAgcOn=1

    // Configura potência de saída para 20dBm (máximo com PA_BOOST)
    lora_write_reg(REG_PA_CONFIG, PA_MAX_BOOST);
    lora_write_reg(REG_PA_DAC, PA_DAC_20);

    // Mapear interrupção DIO0 para TxDone
    lora_write_reg(REG_DIO_MAPPING_1, 0x40);

    // Colocar em modo Standby, pronto para transmitir
    lora_write_reg(REG_OPMODE, RF95_MODE_STANDBY);
    printf("Módulo LoRa (TX) inicializado com sucesso!\n");
}

void lora_send_packet(const uint8_t *payload, uint8_t len) {
    // 1. Colocar em modo Standby
    lora_write_reg(REG_OPMODE, RF95_MODE_STANDBY);

    // 2. Apontar para o início da FIFO de TX e definir tamanho do payload
    lora_write_reg(REG_FIFO_ADDR_PTR, 0);
    lora_write_reg(REG_PAYLOAD_LENGTH, len);

    // 3. Escrever o payload na FIFO
    for (int i = 0; i < len; i++) {
        lora_write_reg(REG_FIFO, payload[i]);
    }

    // 4. Iniciar a transmissão
    lora_write_reg(REG_OPMODE, RF95_MODE_TX);

    // 5. Aguardar o término da transmissão
    printf("Transmitindo pacote...\n");
    while ((lora_read_reg(REG_IRQ_FLAGS) & 0x08) == 0); // Espera pela flag TxDone
    printf("Pacote transmitido!\n");

    // 6. Limpar a flag de IRQ
    lora_write_reg(REG_IRQ_FLAGS, 0xFF); // Limpa todas as flags
}

// --- Função Principal (sem alterações) ---
int main() {
    stdio_init_all();
    sleep_ms(2000);
    printf("Inicializando Transmissor LoRa...\n");

    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);

    gpio_init(PIN_RST);
    gpio_set_dir(PIN_RST, GPIO_OUT);

    spi_init(SPI_PORT, 1000 * 1000);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    lora_init();

    int counter = 0;
    char message[50];

    while (1) {
        sprintf(message, "Ola RX! Pacote #%d", counter++);
        lora_send_packet((uint8_t*)message, strlen(message));
        sleep_ms(5000);
    }

    return 0;
}