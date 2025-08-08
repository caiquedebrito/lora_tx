#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "aht20.h"
#include "bmp280.h"
#include "ssd1306.h"
#include "ws2812.h"
#include "buzzer.h"
#include <math.h>
#include "lora.h"

#define I2C_PORT i2c0               // i2c0 pinos 0 e 1, i2c1 pinos 2 e 3
#define I2C_SDA 0                   // 0 ou 2
#define I2C_SCL 1                   // 1 ou 3
#define SEA_LEVEL_PRESSURE 101325.0 // Pressão ao nível do mar em Pa
// Display na I2C
#define I2C_PORT_DISP i2c1
#define I2C_SDA_DISP 14
#define I2C_SCL_DISP 15
#define endereco 0x3C

// LEDs e buzzer
#define GREEN_LED 11
#define BLUE_LED 12
#define RED_LED 13
#define BUZZER_PIN 21

typedef struct {
    float temperatura;
    float umidade;
} AHT20; // Estrutura para armazenar os dados do AHT20

typedef struct {
    float temperatura;
    float pressao;
} BMP280; // Estrutura para armazenar os dados do BMP280

typedef struct {
    float minTemp;
    float maxTemp;
    float minHum;
    float maxHum;    
    float offsetTemp;
    float offsetHum;
    float offsetPress;
} ConfigData; // Estrutura para armazenar os dados de configuração

ConfigData config_data = {
    .minTemp = 20,
    .maxTemp = 30,
    .minHum = 60,
    .maxHum = 90,
    .offsetTemp = 0,
    .offsetHum = 0,
    .offsetPress = 0
};

AHT20 aht20_data = {
    .temperatura = 0,
    .umidade = 0,
}; // Estrutura para armazenar os dados do AHT20

BMP280 bmp280_data = {
    .temperatura = 0,
    .pressao = 0,
}; // Estrutura para armazenar os dados do BMP280

int payload_leng = 10;  // Tamanho do payload em bytes
int preamble_len = 8;   // Tamanho do preâmbulo em bytes
int sf = 7;             // Fator de espalhamento (spreading factor)
int crc = 0;            // Tamanho do CRC
int ih = 0;             // Tamanho do cabeçalho implícito
int bw = 125000;        // Largura de banda (bandwidth) em Hz
int cr = 1;             // Taxa de codificação (coding rate)
int de = 0;             // LowDataRateOptimize=1;

uint8_t xcenter_pos(char *text);

static void write_register(uint8_t reg, uint8_t data);
static uint8_t read_register(uint8_t addr);
static void cs_select();
static void cs_deselect();
void imprimir_binario(uint32_t valor);

// Função para calcular a altitude a partir da pressão atmosférica
double calculate_altitude(double pressure)
{
    return 44330.0 * (1.0 - pow(pressure / SEA_LEVEL_PRESSURE, 0.1903));
}

// Trecho para modo BOOTSEL com botão B
#include "pico/bootrom.h"
#define BOTAO_B 6
#define BOTAO_A 5
#define JOYSTICK_BUTTON 22
void gpio_irq_handler(uint gpio, uint32_t events);

int main()
{
    stdio_init_all();

    gpio_init(GREEN_LED);
    gpio_set_dir(GREEN_LED, GPIO_OUT);
    gpio_init(BLUE_LED);
    gpio_set_dir(BLUE_LED, GPIO_OUT);
    gpio_init(RED_LED);
    gpio_set_dir(RED_LED, GPIO_OUT);

    // Configura o buzzer com PWM
    buzzer_setup_pwm(BUZZER_PIN, 4000);

    // Para ser utilizado o modo BOOTSEL com botão B
    gpio_init(BOTAO_B);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_pull_up(BOTAO_B);
    gpio_set_irq_enabled_with_callback(BOTAO_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);
    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
   
    gpio_init(JOYSTICK_BUTTON);
    gpio_set_dir(JOYSTICK_BUTTON, GPIO_IN);
    gpio_pull_up(JOYSTICK_BUTTON);
    gpio_set_irq_enabled_with_callback(JOYSTICK_BUTTON, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    PIO pio = pio0;
    uint sm = 0;
    ws2812_init(pio, sm);

    // I2C do Display funcionando em 400Khz.
    i2c_init(I2C_PORT_DISP, 400 * 1000);

    gpio_set_function(I2C_SDA_DISP, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_set_function(I2C_SCL_DISP, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_pull_up(I2C_SDA_DISP);                                        // Pull up the data line
    gpio_pull_up(I2C_SCL_DISP);                                        // Pull up the clock line

    // Inicializa o I2C
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    ssd1306_t ssd;                                                     // Inicializa a estrutura do display
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT_DISP); // Inicializa o display
    ssd1306_config(&ssd);                                              // Configura o display
    ssd1306_fill(&ssd, false);                                   // Limpa o display
    ssd1306_send_data(&ssd);                                           // Envia os dados para o display

    // Inicializa o BMP280
    bmp280_init(I2C_PORT);
    struct bmp280_calib_param params;
    bmp280_get_calib_params(I2C_PORT, &params);

    // Inicializa o AHT20
    aht20_reset(I2C_PORT);
    aht20_init(I2C_PORT);

    // Estrutura para armazenar os dados do sensor
    AHT20_Data data;
    int32_t raw_temp_bmp;
    int32_t raw_pressure;

    char str_aht20_temp[5];  // Buffer para armazenar a string
    char str_aht20_hum[5];  // Buffer para armazenar a string  
    char str_bmp280_temp[5];  // Buffer para armazenar a string
    char str_bpm_280_press[5];  // Buffer para armazenar a string      

    bool cor = true;
    while (true)
    {
        // Leitura do BMP280
        bmp280_read_raw(I2C_PORT, &raw_temp_bmp, &raw_pressure);
        int32_t temperature = bmp280_convert_temp(raw_temp_bmp, &params);
        int32_t pressure = bmp280_convert_pressure(raw_pressure, raw_temp_bmp, &params);

        // Cálculo da altitude
        double altitude = calculate_altitude(pressure);

        printf("Pressao = %.3f kPa\n", pressure / 1000.0);
        printf("Temperatura BMP: = %.2f C\n", temperature / 100.0);
        printf("Altitude estimada: %.2f m\n", altitude);

        // Leitura do AHT20
        if (aht20_read(I2C_PORT, &data))
        {
            printf("Temperatura AHT: %.2f C\n", data.temperature);
            printf("Umidade: %.2f %%\n\n\n", data.humidity);
        }
        else
        {
            printf("Erro na leitura do AHT10!\n\n\n");
        }

        aht20_data.temperatura = data.temperature + config_data.offsetTemp; // Aplica o offset de temperatura
        aht20_data.umidade = data.humidity + config_data.offsetHum;

        bmp280_data.temperatura = temperature / 100.0 + config_data.offsetTemp; // Aplica o offset de temperatura
        bmp280_data.pressao = pressure / 1000.0 + config_data.offsetPress; // Aplica o offset de pressão

        sprintf(str_aht20_temp, "%.1fC", aht20_data.temperatura);  // Temperatura do AHT20
        sprintf(str_aht20_hum, "%.1f%%", aht20_data.umidade);  // Umidade do AHT20
        sprintf(str_bmp280_temp, "%.1fC", bmp280_data.temperatura);  // Temperatura do BMP280
        sprintf(str_bpm_280_press, "%.1fhPa", bmp280_data.pressao);  // Pressão do BMP280

        //  Atualiza o conteúdo do display com animações
        ssd1306_fill(&ssd, !cor);                           // Limpa o display
        ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor);       // Desenha um retângulo

        ssd1306_draw_string(&ssd, "AHT20 & BMP280", xcenter_pos("AHT20 & BMP280"), 10); // Desenha uma string

        ssd1306_line(&ssd, 63, 41, 63, 60, cor);            // Desenha uma linha vertical
        ssd1306_draw_string(&ssd, str_aht20_temp, 14, 41);  // Temperatura AHT20
        ssd1306_draw_string(&ssd, str_aht20_hum, 14, 52);   // Umidade AHT20
        ssd1306_draw_string(&ssd, str_bmp280_temp, 73, 41); // Temperatura BMP280
        ssd1306_draw_string(&ssd, str_bpm_280_press, 73, 52); // Pressão BMP280
        
        ssd1306_send_data(&ssd);                            // Atualiza o display

        sleep_ms(500);
    }
}

/**
 * @brief Calcula a posição centralizada do texto no display.
 * 
 * @param text Texto a ser centralizado.
 * @return Posição X centralizada.
 */
uint8_t xcenter_pos(char* text) {
    return (WIDTH - 8 * strlen(text)) / 2; // Calcula a posição centralizada
}


void gpio_irq_handler(uint gpio, uint32_t events) {
    uint64_t current_time = to_ms_since_boot(get_absolute_time());
    static uint64_t last_time = 0;

    if (gpio == BOTAO_B) {
        // Se o botão B for pressionado, reinicia o boot
        reset_usb_boot(0, 0);
    }

    if ((current_time - last_time < 300)) {
        // Ignora o botão A se pressionado rapidamente
        return;
    }

    last_time = current_time;

    // if (gpio == BOTAO_A) {
    //     // Avança para a próxima página
    //     current_page = (current_page + 1) % 4; // Alterna entre as páginas
    // }

    // if (gpio == JOYSTICK_BUTTON) {
    //     // Volta para a página anterior
    //     current_page = (current_page - 1 + 4) % 4; // Alterna entre as páginas
    // }
    
}


// Definir frequência de operação
void set_frequency(double frequency) {
    unsigned long frequency_value;

    printf("Frequência escolhida %.3f Mhz\n", frequency);

    frequency = frequency * 7110656 / 434;

    frequency_value = (unsigned long)frequency;
    printf("Frequência calculada para ajuste dos registradores: %lu\n", frequency_value);

    write_register(REG_FRF_MSB, (frequency_value >> 16) & 0xFF); // Configura o registrador MSB
    write_register(REG_FRF_MID, (frequency_value >> 8) & 0xFF);  // Configura o registrador MID
    write_register(REG_FRF_LSB, frequency_value & 0xFF);       // Configura o registrador LSB

    printf("Impressão de valor binário da frequência:\n");
    imprimir_binario(frequency_value); // Imprimir o valor binário de 32 bits
    printf("\n");

    printf("Frequ. Regs. config:\n");
    read_register(REG_FRF_MSB);
    read_register(REG_FRF_MID);
    read_register(REG_FRF_LSB);
}

#define SPI_PORT spi0 // Definindo o SPI a ser utilizado

static void write_register(uint8_t reg, uint8_t data) {
    uint8_t buffer[2];
    buffer[0] = reg | 0x80; // Set the write bit
    buffer[1] = data;

    cs_select(); // Iniciar comunicação SPI
    spi_write_blocking(SPI_PORT, buffer, 2); // Envia o registr
    cs_deselect(); // Finalizar comunicação SPI

    sleep_ms(1); // Atraso para garantir que o registrador foi escrito corretamente
}

static uint8_t read_register(uint8_t addr) {
    uint8_t buf[1];

    addr &= 0x7F;

    cs_select(); // Iniciar comunicação SPI
    spi_write_blocking(SPI_PORT, &addr, 1); // Envia o endereço do registrador
    sleep_ms(1); // Atraso para garantir que o registrador foi lido corretamente
    spi_read_blocking(SPI_PORT, 0, buf, 1);
    cs_deselect(); // Finalizar comunicação SPI

    sleep_ms(1); // Atraso para garantir que o registrador foi lido corretamente

    printf("READ %02X: ", buf[0]);
    return buf[0];
}

static void cs_select() {
    gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 0); // Ativa o chip select
}

static void cs_deselect() {
    gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 1); // Desativa o chip select
}

void imprimir_binario(uint32_t valor) {
    for (int i = 31; i >= 0; i--) {
        printf("%d", (valor >> i) & 1);
    }
}