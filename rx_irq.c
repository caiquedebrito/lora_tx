// lora_rx.c (versão com interrupção)

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "pico/sync.h" // Necessário para a seção crítica
#include "lora.h"

// ... (Definições de pinos e frequência permanecem as mesmas) ...
#define SPI_PORT spi0
#define PIN_MISO 16
#define PIN_MOSI 19
#define PIN_SCK  18
#define PIN_CS   17
#define PIN_RST  20
#define PIN_DIO0 21
#define LORA_FREQUENCY 915E6

// Flag para sinalizar que um pacote foi recebido
// 'volatile' é crucial para garantir que o compilador não otimize o acesso a esta variável,
// já que ela é modificada por uma interrupção.
volatile bool packet_received_flag = false;

// ... (Funções lora_write_reg, lora_read_reg, lora_reset, lora_set_frequency são idênticas) ...
// (O corpo da função lora_init_rx também é idêntico ao anterior)

void lora_write_reg(uint8_t reg, uint8_t val) { /* ...código idêntico... */ }
uint8_t lora_read_reg(uint8_t reg) { /* ...código idêntico... */ }
void lora_reset() { /* ...código idêntico... */ }
void lora_set_frequency(long frequency) { /* ...código idêntico... */ }
void lora_init_rx() { /* ...código idêntico ao da resposta anterior... */ }


// --- Rotina de Tratamento de Interrupção (ISR) ---
void gpio_callback(uint gpio, uint32_t events) {
    if (gpio == PIN_DIO0) {
        if ((events & GPIO_IRQ_EDGE_RISE) != 0) {
            packet_received_flag = true;
        }
    }
}

// --- Função Principal Modificada ---
int main() {
    stdio_init_all();
    sleep_ms(2000);
    printf("Inicializando Receptor LoRa (com Interrupção)...\n");

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

    // Inicializa o rádio LoRa no modo RX
    lora_init_rx();
    
    // *** Configura a interrupção no pino DIO0 ***
    gpio_set_irq_enabled_with_callback(PIN_DIO0, GPIO_IRQ_EDGE_RISE, true, &gpio_callback);

    uint8_t buffer[256];

    while (1) {
        if (packet_received_flag) {
            // Entra em uma seção crítica para ler o pacote de forma segura
            uint32_t status = save_and_disable_interrupts();
            packet_received_flag = false; // Reseta a flag
            
            // Limpa a flag de IRQ do rádio
            lora_write_reg(REG_IRQ_FLAGS, 0xFF);
            
            // Verifica se houve erro de CRC
            if ((lora_read_reg(REG_IRQ_FLAGS) & 0x20)) {
                printf("Erro de CRC!\n");
            } else {
                 // Pega o tamanho e lê os dados
                int len = lora_read_reg(REG_RX_NB_BYTES);
                uint8_t current_addr = lora_read_reg(REG_FIFO_RX_CURRENT_ADDR);
                lora_write_reg(REG_FIFO_ADDR_PTR, current_addr);
                for (int i = 0; i < len; i++) {
                    buffer[i] = lora_read_reg(REG_FIFO);
                }
                buffer[len] = '\0';
                printf("Pacote recebido (%d bytes): '%s'\n", len, buffer);
            }
            
            restore_interrupts(status);
        }

        // O processador pode fazer outras coisas aqui ou dormir
        // tight_loop_contents(); // ou __wfi(); para economizar energia
    }

    return 0;
}


// void lora_processar_pacote() {
//     uint8_t buffer[256];

//     // Entra em uma seção crítica para ler o pacote de forma segura
//     uint32_t status = save_and_disable_interrupts();
    
//     // Limpa a flag de IRQ do rádio
//     lora_write_reg(REG_IRQ_FLAGS, 0xFF);
    
//     // ... (resto da lógica para ler da FIFO, checar CRC, etc.) ...
    
//     int len = lora_read_reg(REG_RX_NB_BYTES);
//     // ...
//     buffer[len] = '\0';
//     printf("Pacote recebido (%d bytes): '%s'\n", len, buffer);
    
//     restore_interrupts(status);
// }

// int main() {
//     // ... (inicialização) ...

//     while (1) {
//         if (packet_received_flag) {
//             packet_received_flag = false; // Reseta a flag
//             lora_processar_pacote();      // Chama a função de processamento
//         }
        
//         // A CPU está livre aqui
//     }
// }