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
bool cor = true; // Booleano que indica a cor branca do pixel

// Definições para o Joystick e ADC
#define JOYSTICK_X 27
#define JOYSTICK_Y 26
#define JOYSTICK_BUTTON 22

// Variáveis para controle visual do display dos clientes
int customer_standby_count = 0;
uint32_t customer_standby_time = 0;

void generate_border(){
    ssd1306_rect(&ssd, 0, 0, 127, 2, cor, cor); // Borda superior, altura=2
    ssd1306_rect(&ssd, 61, 0, 127, 2, cor, cor); // Borda inferior, altura=2
    ssd1306_rect(&ssd, 0, 0, 2, 63, cor, cor); // Borda esquerda, largura=2
    ssd1306_rect(&ssd, 0, 125, 2, 63, cor, cor); // Borda direita, largura=2
}

void customer_standby(){
    uint32_t current_time = to_us_since_boot(get_absolute_time());

    ssd1306_rect(&ssd, 0, 0, 127, 11, cor, cor); // Borda
    ssd1306_draw_string(&ssd, "SmartPark", 28, 2, true); // Logo em negativo
    ssd1306_draw_string(&ssd, "Bom dia!", 31, 18, false);  // Linha 1
    ssd1306_draw_string(&ssd, "Pressione B", 19, 32, false); // Linha 2
    ssd1306_draw_string(&ssd, "Aguardando", 7, 46, false); // Linha 3

    // Reticências dinâmicas da linha 3, se o contador não estiver zerado
    if (customer_standby_count>0){
        for(int i=1; i<=customer_standby_count; i++){
            ssd1306_draw_string(&ssd, ".", 87 + (8*i), 46, false); // Linha 3
        }
    }

    // Atualiza as reticências a cada 500ms através desse incremento periódico
    if(current_time - customer_standby_time > 500000){
        customer_standby_time = current_time;
        customer_standby_count++; // Atualiza a próxima interação do display
    }
    // Limita o contador a 3
    if(customer_standby_count>3){
        customer_standby_count = 0;
    }
}

int main(){
    stdio_init_all();

    // Configurações do I2C e Display 
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

    // Configurações do ADC
    adc_init(); // Inicializando o ADC
    adc_gpio_init(JOYSTICK_X); // Canal 1
    adc_gpio_init(JOYSTICK_Y); // Canal 0





    while (true) {
        // Leituras do ADC
        // Leitura do Eixo X (Canal 1)
        adc_select_input(1);
        uint16_t vrx_value = adc_read();
        // Leitura do Eixo Y (Canal 0)
        adc_select_input(0);
        uint16_t vry_value = adc_read();
        // Deadzone para os joysticks
        if (vrx_value>=1900 && vrx_value<=2194){
            vrx_value=2048;
        }
        if (vry_value>=1900 && vry_value<=2194){
            vry_value=2048;
        }


        ssd1306_fill(&ssd, false); // Limpa o display

        generate_border(); // Gera a borda com largura de 2 pixels
        
        //customer_standby(); // Tela de standby para o cliente

        ssd1306_send_data(&ssd); // Envia os dados para o display, atualizando o mesmo

        printf("Joystick X: %d | Joystick Y: %d\n", vrx_value, vry_value); // Teste da USB
        sleep_ms(30);
    }
}
