#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "libs/font.h"
#include "libs/ssd1306.h"

// Definições para o I2C e display 
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define DISPLAY_ADDRESS 0x3C
ssd1306_t ssd; // Inicializa a estrutura do display no escopo global


int main(){
    stdio_init_all();

    // Inicializando o I2C
    i2c_init(I2C_PORT, 400*1000);
    // Setando as funções dos pinos do I@C
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    // Garantindo o Pull up do I2C
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    // Configuração do display
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, DISPLAY_ADDRESS, I2C_PORT); // Inicializa o display
    ssd1306_config(&ssd); // Configura o display
    ssd1306_send_data(&ssd); // Envia os dados para o display
    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);
    bool cor = true; // Booleano que indica a cor branca do pixel

    while (true) {

        // Limpa o display
        ssd1306_fill(&ssd, false);
        ssd1306_draw_string(&ssd, "Teste", 59, 27, true); // Quadrado preenchido
        ssd1306_send_data(&ssd); // Envia os dados para o display, atualizando o mesmo

        printf("Hello, world!\n"); // Teste da USB
        sleep_ms(30);
    }
}
