// lora_rx.c

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "lora.h" // Nosso cabeçalho com as definições

// Definições dos Pinos
#define SPI_PORT spi0
#define PIN_MISO 16
#define PIN_MOSI 19
#define PIN_SCK  18
#define PIN_CS   17
#define PIN_RST  20
#define PIN_DIO0 21

// Frequência do LoRa (DEVE SER A MESMA DO TRANSMISSOR!)
#define LORA_FREQUENCY 915E6

// --- Funções de baixo nível para SPI (idênticas ao TX) ---
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

// --- Funções de alto nível do LoRa ---
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

void lora_init_rx() {
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

    // Configura ponteiros da FIFO para RX
    lora_write_reg(REG_FIFO_RX_BASE_AD, 0);
    lora_write_reg(REG_FIFO_ADDR_PTR, 0);

    // Configura LNA para ganho máximo
    lora_write_reg(REG_LNA, LNA_MAX_GAIN);

    // Configura parâmetros do modem (DEVE SER IGUAL AO TX)
    lora_write_reg(REG_MODEM_CONFIG, EXPLICIT_MODE | ERROR_CODING_4_5 | BANDWIDTH_125K);
    lora_write_reg(REG_MODEM_CONFIG2, SPREADING_7 | CRC_ON);
    lora_write_reg(REG_MODEM_CONFIG3, 0x04); // LnaGain set by REG_LNA, LnaAgcOn=1

    // Mapear interrupção DIO0 para RxDone (00 -> RxDone)
    lora_write_reg(REG_DIO_MAPPING_1, 0x00);

    // Colocar em modo de recepção contínua
    lora_write_reg(REG_OPMODE, RF95_MODE_RX_CONTINUOUS);
    printf("Módulo LoRa (RX) inicializado e ouvindo...\n");
}

int lora_receive_packet(uint8_t *buffer, int max_len) {
    // Verifica se a flag de 'RxDone' foi acionada
    if ((lora_read_reg(REG_IRQ_FLAGS) & 0x40) == 0) {
        return 0; // Nenhum pacote recebido
    }

    // Limpa a flag de IRQ
    lora_write_reg(REG_IRQ_FLAGS, 0xFF);

    // Verifica se houve erro de CRC
    if ((lora_read_reg(REG_IRQ_FLAGS) & 0x20)) {
        printf("Erro de CRC!\n");
        return 0;
    }

    // Pega o tamanho do pacote recebido
    int len = lora_read_reg(REG_RX_NB_BYTES);
    if (len > max_len) {
        len = max_len;
    }

    // Aponta para o início do pacote na FIFO
    uint8_t current_addr = lora_read_reg(REG_FIFO_RX_CURRENT_ADDR);
    lora_write_reg(REG_FIFO_ADDR_PTR, current_addr);

    // Lê os dados da FIFO
    for (int i = 0; i < len; i++) {
        buffer[i] = lora_read_reg(REG_FIFO);
    }

    return len;
}

// --- Função Principal ---
int main() {
    stdio_init_all();
    sleep_ms(2000);
    printf("Inicializando Receptor LoRa...\n");

    // Inicializa hardware (SPI e GPIO)
    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);

    gpio_init(PIN_RST);
    gpio_set_dir(PIN_RST, GPIO_OUT);

    spi_init(SPI_PORT, 1000 * 1000);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    // Inicializa o rádio LoRa no modo RX
    lora_init_rx();

    uint8_t buffer[256];

    while (1) {
        int packet_len = lora_receive_packet(buffer, sizeof(buffer));
        if (packet_len > 0) {
            buffer[packet_len] = '\0'; // Adiciona terminador nulo para imprimir como string
            printf("Pacote recebido (%d bytes): '%s'\n", packet_len, buffer);
        }
    }

    return 0;
}