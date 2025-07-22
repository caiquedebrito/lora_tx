#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "aht20.h"
#include "bmp280.h"
#include "ssd1306.h"
#include "pico/cyw43_arch.h"
#include "ws2812.h"
#include "buzzer.h"
#include "lwipopts.h"
#include "lwip/tcp.h"
#include <math.h>
#include "index.html.h"
#include "temperatura.html.h"
#include "umidade.html.h"
#include "pressao.html.h"
#include "limites.html.h"
#include "offset.html.h"

#define I2C_PORT i2c0               // i2c0 pinos 0 e 1, i2c1 pinos 2 e 3
#define I2C_SDA 0                   // 0 ou 2
#define I2C_SCL 1                   // 1 ou 3
#define SEA_LEVEL_PRESSURE 101325.0 // Pressão ao nível do mar em Pa
// Display na I2C
#define I2C_PORT_DISP i2c1
#define I2C_SDA_DISP 14
#define I2C_SCL_DISP 15
#define endereco 0x3C

#define WIFI_SSID ""
#define WIFI_PASSWORD ""

#define GREEN_LED 11
#define BLUE_LED 12
#define RED_LED 13
#define BUZZER_PIN 21

typedef struct {
    float temperatura;
    float umidade;
} AHT20;

typedef struct {
    float temperatura;
    float pressao;
} BMP280;

typedef struct {
    float minTemp;
    float maxTemp;
    float minHum;
    float maxHum;    
    float offsetTemp;
    float offsetHum;
    float offsetPress;
} ConfigData;

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

typedef enum {
    INDEX,
    TEMPERATURA_LIMITES,
    UMIDADE_LIMITES,
    OFFSET
} PAGES;

typedef enum {
    NORMAL,
    ACIMA,
    ABAIXO
} STATUS;

PAGES current_page = INDEX; // Página atual

STATUS current_status = NORMAL; // Status atual

struct http_state
{
    char response[16384];
    size_t len;
    size_t sent;
};

uint8_t xcenter_pos(char *text);


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

static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    struct http_state *hs = (struct http_state *)arg;
    
    if (!hs) {
        tcp_close(tpcb);
        return ERR_ARG;
    }
    
    hs->sent += len;
    if (hs->sent >= hs->len)
    {
        tcp_close(tpcb);
        free(hs);
        return ERR_OK;
    }
    
    // Se ainda há dados para enviar, envia o próximo chunk
    size_t remaining = hs->len - hs->sent;
    size_t chunk = remaining > 1024 ? 1024 : remaining;
    err_t err = tcp_write(tpcb, hs->response + hs->sent, chunk, TCP_WRITE_FLAG_COPY);
    if (err == ERR_OK) {
        tcp_output(tpcb);
    } else {
        printf("Erro ao enviar chunk: %d\n", err);
        tcp_close(tpcb);
        free(hs);
        return err;
    }
    
    return ERR_OK;
}

static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if (!p)
    {
        tcp_close(tpcb);
        return ERR_OK;
    }

    char *req = (char *)p->payload;
    struct http_state *hs = malloc(sizeof(struct http_state));
    if (!hs)
    {
        pbuf_free(p);
        tcp_close(tpcb);
        return ERR_MEM;
    }
    hs->sent = 0;

    if (strstr(req, "GET /data"))
    {
        char json_payload[256];
        int json_len = snprintf(json_payload, sizeof(json_payload),
            "{\"temperature_aht20\":%.1f,\"temperature_bmp\":%.1f,\"humidity\":%.1f,\"pressure\":%.1f}",
            aht20_data.temperatura,
            bmp280_data.temperatura,
            aht20_data.umidade,
            bmp280_data.pressao
        );

        hs->len = snprintf(hs->response, sizeof(hs->response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "\r\n"
            "%s",
            json_len, json_payload);
    }
    else if (strstr(req, "GET /settings/data")) {
        char json_payload[256];
        int json_len = snprintf(json_payload, sizeof(json_payload),
            "{\"offsetHum\":%.1f, \"offsetPres\":%.1f, \"offsetTemp\":%.1f, \"tempMin\":%.1f, \"tempMax\":%.1f, \"humMin\":%.1f, \"humMax\":%.1f}",
            config_data.offsetHum,
            config_data.offsetPress,
            config_data.offsetTemp,
            config_data.minTemp,
            config_data.maxTemp,
            config_data.minHum,
            config_data.maxHum
        );

        hs->len = snprintf(hs->response, sizeof(hs->response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            strlen(json_payload),
            json_payload
        );
    }
    else if (strstr(req, "POST /settings/limites"))
    {
        // Encontrar o início do JSON no corpo da requisição
        char *json_start = strstr(req, "\r\n\r\n");
        if (json_start) {
            json_start += 4; // Pular o "\r\n\r\n"
            
            float tempMin, tempMax, humMin, humMax;
            
            // Parse mais robusto do JSON
            int parsed = sscanf(json_start, "{\"tempMin\":%f,\"tempMax\":%f,\"humMin\":%f,\"humMax\":%f}",
                               &tempMin, &tempMax, &humMin, &humMax);
            
            if (parsed == 4) {
                config_data.minTemp = tempMin;
                config_data.maxTemp = tempMax;
                config_data.minHum = humMin;
                config_data.maxHum = humMax;
                
                printf("Limites atualizados: Temp Min: %.1f, Temp Max: %.1f, Hum Min: %.1f, Hum Max: %.1f\n",
                       config_data.minTemp, config_data.maxTemp, config_data.minHum, config_data.maxHum);
            } else {
                printf("Erro no parsing: apenas %d valores foram parseados\n", parsed);
            }
        } else {
            printf("Erro: não foi possível encontrar o corpo da requisição\n");
        }

        char response_msg[] = "Configurações de limites atualizadas com sucesso.";
        hs->len = snprintf(hs->response, sizeof(hs->response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            strlen(response_msg), response_msg);
    }
    else if (strstr(req, "POST /settings/offset"))
    {
        // Encontrar o início do JSON no corpo da requisição
        char *json_start = strstr(req, "\r\n\r\n");
        if (json_start) {
            json_start += 4; // Pular o "\r\n\r\n"
            
            float offsetTemp, offsetHum, offsetPress;
            
            // Parse mais robusto do JSON
            int parsed = sscanf(json_start, "{\"offsetTemp\":%f,\"offsetHum\":%f,\"offsetPres\":%f}",
                               &offsetTemp, &offsetHum, &offsetPress);
        
            if (parsed == 3) {
                config_data.offsetTemp = offsetTemp;
                config_data.offsetHum = offsetHum;
                config_data.offsetPress = offsetPress;
                
                printf("Offsets atualizados: Temp: %.1f, Hum: %.1f, Press: %.1f\n",
                       config_data.offsetTemp, config_data.offsetHum, config_data.offsetPress);
            } else {
                printf("Erro no parsing: apenas %d valores foram parseados\n", parsed);
            }
        } else {
            printf("Erro: não foi possível encontrar o corpo da requisição\n");
        }
        
        char response_msg[] = "Configurações de offsets atualizadas com sucesso.";
        hs->len = snprintf(hs->response, sizeof(hs->response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            strlen(response_msg), response_msg);
    }
    else if (strstr(req, "GET /umidade"))
    {
        hs->len = snprintf(hs->response, sizeof(hs->response),
                           "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/html\r\n"
                           "Content-Length: %zu\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "%s",
                           strlen(UMIDADE_HTML), UMIDADE_HTML);
    }
    else if (strstr(req, "GET /pressao"))
    {
        hs->len = snprintf(hs->response, sizeof(hs->response),
                           "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/html\r\n"
                           "Content-Length: %zu\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "%s",
                           strlen(PRESSAO_HTML), PRESSAO_HTML);
    }
    else if (strstr(req, "GET /temperatura"))        
    {
        size_t html_len = strlen(TEMPERATURA_HTML);
        hs->len = snprintf(hs->response, sizeof(hs->response),
                           "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/html\r\n"
                           "Content-Length: %zu\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "%s",
                           html_len, TEMPERATURA_HTML);
    } 
    else if (strstr(req, "GET /limites")) {
        hs->len = snprintf(hs->response, sizeof(hs->response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            strlen(LIMITES_HTML), LIMITES_HTML);
    }
    else if (strstr(req, "GET /offset")) {
        hs->len = snprintf(hs->response, sizeof(hs->response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            strlen(OFFSET_HTML), OFFSET_HTML);
    }
    else {
        hs->len = snprintf(hs->response, sizeof(hs->response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            strlen(INDEX_HTML),
            INDEX_HTML
        );
    }

    tcp_arg(tpcb, hs);
    tcp_sent(tpcb, http_sent);

    tcp_write(tpcb, hs->response, hs->len, TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);

    pbuf_free(p);
    return ERR_OK;
}

static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    tcp_recv(newpcb, http_recv);
    return ERR_OK;
}

static void start_http_server(void)
{
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb)
    {
        printf("Erro ao criar PCB TCP\n");
        return;
    }
    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK)
    {
        printf("Erro ao ligar o servidor na porta 80\n");
        return;
    }
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, connection_callback);
    printf("Servidor HTTP rodando na porta 80...\n");
}

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
    ssd1306_draw_string(&ssd, "Iniciando Wi-Fi", 0, 0);
    char aguarde[] = "Aguarde...";
    ssd1306_draw_string(&ssd, aguarde, xcenter_pos(aguarde), 30);
    ssd1306_send_data(&ssd);                                           // Envia os dados para o display

    while (cyw43_arch_init())
    {
        ssd1306_fill(&ssd, false);
        ssd1306_draw_string(&ssd, "FALHA NO WIFI", xcenter_pos("FALHA NO WIFI"), 8);
        ssd1306_draw_string(&ssd, ":(", xcenter_pos(":("), 24);
        ssd1306_send_data(&ssd);
    }

    cyw43_arch_enable_sta_mode();
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 10000))
    {
        ssd1306_fill(&ssd, false);
        ssd1306_draw_string(&ssd, "WiFi => ERRO", xcenter_pos("WiFi => ERRO"), 24);
        ssd1306_send_data(&ssd);
    }

    uint8_t *ip = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);
    char ip_str[24];
    snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

    ssd1306_fill(&ssd, false);
    ssd1306_draw_string(&ssd, "WiFi => OK", 0, 0);
    ssd1306_draw_string(&ssd, ip_str, 0, 10);
    ssd1306_send_data(&ssd);

    start_http_server();

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
        cyw43_arch_poll(); // Polling do Wi-Fi
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

        if (
            aht20_data.temperatura < config_data.minTemp ||
            bmp280_data.temperatura < config_data.minTemp ||
            aht20_data.umidade < config_data.minHum
        ) {
            gpio_put(GREEN_LED, true);
            gpio_put(RED_LED, true);
            buzzer_play(BUZZER_PIN, 2, 725, 100); // Toca o buzzer se os valores estiverem abaixo dos mínimos
            set_pattern(pio, sm, 0, "amarelo"); // Liga a matriz de LEDs em vermelho
            current_status = ABAIXO;
        } else if (
            aht20_data.temperatura > config_data.maxTemp ||
            bmp280_data.temperatura > config_data.maxTemp ||
            aht20_data.umidade > config_data.maxHum
        ) {
            gpio_put(GREEN_LED, false);
            gpio_put(RED_LED, true);
            buzzer_play(BUZZER_PIN, 3, 2000, 100); // Toca o buzzer se os valores estiverem acima dos máximos
            set_pattern(pio, sm, 1, "vermelho"); // Liga a matriz de LEDs em vermelho
            current_status = ACIMA;
        } else {
            gpio_put(GREEN_LED, true);
            gpio_put(RED_LED, false);
            set_pattern(pio, sm, 2, "verde"); // Limpa a matriz de LEDs
            current_status = NORMAL; // Valores normais
        }

        sprintf(str_aht20_temp, "%.1fC", aht20_data.temperatura);  // Temperatura do AHT20
        sprintf(str_aht20_hum, "%.1f%%", aht20_data.umidade);  // Umidade do AHT20
        sprintf(str_bmp280_temp, "%.1fC", bmp280_data.temperatura);  // Temperatura do BMP280
        sprintf(str_bpm_280_press, "%.1fhPa", bmp280_data.pressao);  // Pressão do BMP280

        // Strings para os limites de temperatura e umidade
        char str_aht20_min_temp[16];
        char str_aht20_max_temp[16]; 
        char str_aht20_min_hum[16];
        char str_aht20_max_hum[16];
        
        sprintf(str_aht20_min_temp, "Minimo: %.1fC", config_data.minTemp);
        sprintf(str_aht20_max_temp, "Maximo: %.1fC", config_data.maxTemp);
        sprintf(str_aht20_min_hum, "Minimo: %.1f%%", config_data.minHum);
        sprintf(str_aht20_max_hum, "Maximo: %.1f%%", config_data.maxHum);

        //  Atualiza o conteúdo do display com animações
        ssd1306_fill(&ssd, !cor);                           // Limpa o display
        ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor);       // Desenha um retângulo

        if (current_page == INDEX) {
            ssd1306_draw_string(&ssd, ip_str, xcenter_pos(ip_str), 1);   // Desenha uma string
            ssd1306_draw_string(&ssd, "AHT20 & BMP280", xcenter_pos("AHT20 & BMP280"), 10); // Desenha uma string

            if (current_status == ABAIXO) {
                ssd1306_draw_string(&ssd, "ABAIXO", xcenter_pos("ABAIXO"), 20);
            } else if (current_status == ACIMA) {
                ssd1306_draw_string(&ssd, "ACIMA", xcenter_pos("ACIMA"), 20);
            } else {
                ssd1306_draw_string(&ssd, "NORMAL", xcenter_pos("NORMAL"), 20);
            }

            ssd1306_line(&ssd, 63, 41, 63, 60, cor);            // Desenha uma linha vertical
            ssd1306_draw_string(&ssd, str_aht20_temp, 14, 41);  // Temperatura AHT20
            ssd1306_draw_string(&ssd, str_aht20_hum, 14, 52);   // Umidade AHT20
            ssd1306_draw_string(&ssd, str_bmp280_temp, 73, 41); // Temperatura BMP280
            ssd1306_draw_string(&ssd, str_bpm_280_press, 73, 52); // Pressão BMP280
        } else if (current_page == TEMPERATURA_LIMITES) {
            ssd1306_draw_string(&ssd, "Temperatura", xcenter_pos("Temperatura"), 1); // Desenha uma string
            ssd1306_draw_string(&ssd, "Limites", xcenter_pos("Limites"), 12); // Desenha uma string
            ssd1306_draw_string(&ssd, str_aht20_min_temp, 10, 34); // Temperatura mínima
            ssd1306_draw_string(&ssd, str_aht20_max_temp, 10, 45); // Temperatura máxima
        } else if (current_page == UMIDADE_LIMITES) {
            ssd1306_draw_string(&ssd, "Umidade", xcenter_pos("Umidade"), 1); // Desenha uma string
            ssd1306_draw_string(&ssd, "Limites", xcenter_pos("Limites"), 12); // Desenha uma string
            ssd1306_draw_string(&ssd, str_aht20_min_hum, 10, 34); // Umidade mínima
            ssd1306_draw_string(&ssd, str_aht20_max_hum, 10, 45); // Umidade máxima
        } else if (current_page == OFFSET) {
            char str_offset_temp[16];
            char str_offset_hum[16];
            char str_offset_press[16];

            sprintf(str_offset_temp, "Temp: %.1fC", config_data.offsetTemp);
            sprintf(str_offset_hum, "Hum: %.1f%%", config_data.offsetHum);
            sprintf(str_offset_press, "Press: %.1fkPa", config_data.offsetPress);

            ssd1306_draw_string(&ssd, "OFFSET", xcenter_pos("OFFSET"), 1);   // Desenha uma string

            ssd1306_draw_string(&ssd, str_offset_temp, 10, 12); // Offset de temperatura
            ssd1306_draw_string(&ssd, str_offset_hum, 10, 23); // Offset de umidade
            ssd1306_draw_string(&ssd, str_offset_press, 10, 34); // Offset de pressão
        }
        
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

    if (gpio == BOTAO_A) {
        // Avança para a próxima página
        current_page = (current_page + 1) % 4; // Alterna entre as páginas
    }

    if (gpio == JOYSTICK_BUTTON) {
        // Volta para a página anterior
        current_page = (current_page - 1 + 4) % 4; // Alterna entre as páginas
    }
    
}