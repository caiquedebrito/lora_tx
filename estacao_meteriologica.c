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

int main()
{
    stdio_init_all();

    while (true) {
        printf("Hello, world!\n");
        sleep_ms(1000);
    }
}
