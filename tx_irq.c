// lora_tx_interrupt.c

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "pico/sync.h"
#include "lora.h"

// Definições dos Pinos
#define SPI_PORT spi0
#define PIN_MISO 16
#define PIN_MOSI 19
#define PIN_SCK  18
#define PIN_CS   17
#define PIN_RST  20
#define PIN_DIO0 21

// Frequência do LoRa
#define LORA_FREQUENCY 915E6

// Flag para sinalizar que a transmissão foi concluída
volatile bool tx_done_flag = false;

// --- Funções de baixo nível e inicialização (idênticas às anteriores) ---
void lora_write_reg(uint8_t reg, uint8_t val) { /* ...código idêntico... */ }
uint8_t lora_read_reg(uint8_t reg) { /* ...código idêntico... */ }
void lora_reset() { /* ...código idêntico... */ }
void lora_set_frequency(long frequency) { /* ...código idêntico... */ }
void lora_init() {
    lora_reset();

    lora_write_reg(REG_OPMODE, RF95_MODE_SLEEP);
    sleep_ms(10);
    lora_write_reg(REG_OPMODE, RF95_MODE_SLEEP);
    lora_write_reg(REG_OPMODE, lora_read_reg(REG_OPMODE) | 0x80);
    sleep_ms(10);

    if (lora_read_reg(REG_OPMODE) != (RF95_MODE_SLEEP | 0x80)) {
        printf("Falha ao iniciar o modo LoRa!\n");
        while(1);
    }
    
    lora_write_reg(REG_OPMODE, RF95_MODE_STANDBY);
    lora_set_frequency(LORA_FREQUENCY);
    lora_write_reg(REG_FIFO_TX_BASE_AD, 0);
    lora_write_reg(REG_FIFO_ADDR_PTR, 0);
    lora_write_reg(REG_LNA, LNA_MAX_GAIN);
    lora_write_reg(REG_MODEM_CONFIG, EXPLICIT_MODE | ERROR_CODING_4_5 | BANDWIDTH_125K);
    lora_write_reg(REG_MODEM_CONFIG2, SPREADING_7 | CRC_ON);
    lora_write_reg(REG_MODEM_CONFIG3, 0x04);
    lora_write_reg(REG_PA_CONFIG, PA_MAX_BOOST);
    lora_write_reg(REG_PA_DAC, PA_DAC_20);

    // *** Mapear interrupção DIO0 para TxDone (01 -> TxDone) ***
    lora_write_reg(REG_DIO_MAPPING_1, 0x40);

    lora_write_reg(REG_OPMODE, RF95_MODE_STANDBY);
    printf("Módulo LoRa (TX) inicializado com sucesso!\n");
}


// --- Rotina de Tratamento de Interrupção (ISR) para TX ---
void gpio_callback(uint gpio, uint32_t events) {
    if (gpio == PIN_DIO0 && (events & GPIO_IRQ_EDGE_RISE)) {
        tx_done_flag = true;
    }
}


// --- Função de Envio Modificada (Não bloqueante) ---
void lora_send_packet(const uint8_t *payload, uint8_t len) {
    // Colocar em modo Standby
    lora_write_reg(REG_OPMODE, RF95_MODE_STANDBY);

    // Apontar para o início da FIFO de TX e definir tamanho
    lora_write_reg(REG_FIFO_ADDR_PTR, 0);
    lora_write_reg(REG_PAYLOAD_LENGTH, len);

    // Escrever o payload na FIFO
    for (int i = 0; i < len; i++) {
        lora_write_reg(REG_FIFO, payload[i]);
    }

    // Limpar a flag de IRQ antes de transmitir
    lora_write_reg(REG_IRQ_FLAGS, 0xFF);

    // Iniciar a transmissão
    lora_write_reg(REG_OPMODE, RF95_MODE_TX);
    
    // A função retorna IMEDIATAMENTE. A interrupção nos avisará quando terminar.
}


// --- Função Principal Modificada ---
int main() {
    stdio_init_all();
    sleep_ms(2000);
    printf("Inicializando Transmissor LoRa (com Interrupção)...\n");

    // Inicializa hardware (SPI e GPIO)
    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);
    gpio_init(PIN_RST);
    gpio_set_dir(PIN_RST, GPIO_OUT);
    
    // Configura o pino DIO0 como entrada para a interrupção
    gpio_init(PIN_DIO0);
    gpio_set_dir(PIN_DIO0, GPIO_IN);

    spi_init(SPI_PORT, 1000 * 1000);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    
    // Inicializa o rádio
    lora_init();
    
    // *** Configura a interrupção no pino DIO0 para borda de subida ***
    gpio_set_irq_enabled_with_callback(PIN_DIO0, GPIO_IRQ_EDGE_RISE, true, &gpio_callback);
    
    int counter = 0;
    char message[50];
    tx_done_flag = true; // Inicia pronto para enviar o primeiro pacote

    while (1) {
        if (tx_done_flag) {
            tx_done_flag = false; // Reseta a flag, estamos ocupados
            
            sprintf(message, "TX IRQ! Pacote #%d", counter++);
            printf("Iniciando transmissão: '%s'\n", message);
            lora_send_packet((uint8_t*)message, strlen(message));
            
            // Aguarda 5 segundos antes de permitir o envio do próximo pacote.
            // Note que este sleep não bloqueia a CPU durante a transmissão.
            sleep_ms(5000);
        }

        // Enquanto o rádio está transmitindo, a CPU está livre!
        // Podemos fazer outras tarefas aqui.
        // Por exemplo, piscar um LED para mostrar que o programa está rodando.
        tight_loop_contents();
    }

    return 0;
}