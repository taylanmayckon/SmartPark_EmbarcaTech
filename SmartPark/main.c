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
int customer_standby_count = 0; // Contagem de reticências do display de standby
uint32_t customer_standby_time = 0; // Controle de tempo do display de standby

int customer_spot_select_info_count = 0; // Controla a mensagem exibida em cima na seleção de vagas
uint32_t customer_spot_select_info_time = 0; // Controla o tempo de alternar a mensagem superior na seleção de vagas
int customer_spot_select_spotview_value = 0; // Variável que controla a navegação do usuário entre as vagas
uint32_t customer_spot_select_spotview_time = 0; // Variável para controlar o tempo de alternagem entre as vagas no display
bool customer_spot_select_info_bool = false; // Altera o visual da vaga a ser selecionada
int customer_spot_select_spacer_value = 0; // Controla o espaçamento entre o numero das vagas no display
char converted_num; // Variável que armazena o número convertido em char
char converted_string[3]; // String que armazena o número convertido, no formato a ser exibido no display

// Vetor indicativo do estado das vagas
bool spots_state[25] = {
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
};




void generate_border(){
    ssd1306_rect(&ssd, 0, 0, 127, 2, cor, cor); // Borda superior, altura=2
    ssd1306_rect(&ssd, 61, 0, 127, 2, cor, cor); // Borda inferior, altura=2
    ssd1306_rect(&ssd, 0, 0, 2, 63, cor, cor); // Borda esquerda, largura=2
    ssd1306_rect(&ssd, 0, 125, 2, 63, cor, cor); // Borda direita, largura=2
}

// Função para o standby do usuário
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


// Função que converte int para char
void int_2_char(int num, char *out){
    *out = '0' + num;
}

// Função para seleção de vaga por parte do usuário
void customer_select_spot(uint16_t x_value, uint16_t y_value){
    ssd1306_rect(&ssd, 0, 0, 127, 11, cor, cor); // Borda superior

    uint32_t current_time = to_us_since_boot(get_absolute_time()); // Pega o tempo de execução atual

    // Condicional para controlar a informação superior de forma periódica
    if(current_time - customer_spot_select_info_time > 4000000){ // 4 segundos de intervalo
        customer_spot_select_info_time = current_time; // Atualiza o tempo
        customer_spot_select_info_count++; // Atualiza a variável do switch que controla o texto exposto em cima
    }

    // Limita o contador para o texto superior
    if(customer_spot_select_info_count>2){
        customer_spot_select_info_count = 0;
    }

    // Switch/case para selecionar a informação na parte superior
    switch(customer_spot_select_info_count){
        case 0:
            ssd1306_draw_string(&ssd, "Selecione", 28, 2, true); // Texto superior em negativo
            break;
        case 1:
            ssd1306_draw_string(&ssd, "Confirme com B", 7, 2, true); // Texto superior em negativo
            break;
        case 2:
            ssd1306_draw_string(&ssd, "Cancele com A", 11, 2, true); // Texto superior em negativo
            break;
    }

    // Controle de tempo para a navegação do display
    if(current_time - customer_spot_select_spotview_time > 200000){ // 200ms de intervalo
        customer_spot_select_spotview_time = current_time; // Atualiza o tempo

        // Navegação esquerda-direita
        if(x_value>3000){ // Joystick para a direita
            customer_spot_select_spotview_value++; // Altera para a vaga da direita (+1)
        } else if(x_value<1000){ // Joystick para a esquerda
            customer_spot_select_spotview_value--; // Altera para a vaga da esquerda (-1)
        }

        if(y_value>3000){ // Joystick para cima
            customer_spot_select_spotview_value-=5; // Altera para a vaga de cima (-5)
        } else if(y_value<1000){ // Joystick para baixo
            customer_spot_select_spotview_value+=5; // Altera para a vaga de baixo (+5)
        }

        // Condicionais para não ultrapassar o limite de vags
        if(customer_spot_select_spotview_value<0){ // Se tentar menor que 0, joga para a última
            customer_spot_select_spotview_value=24;
        }
        else if(customer_spot_select_spotview_value>24){ // Se tentar maior que 24, joga para a primeira
            customer_spot_select_spotview_value=0;
        }
        
    }


    int line_select = 0; // Variável que aponta para a linha de vagas a ser escrita (zerada antes de cada loop for a seguir)
    // Visualização de todas as vagas possíveis no display
    for(int i=0; i<25; i++){
        if(i%5==0){ // Troca a linha quando ultrapassar 5 iterações e reseta o spacer das vagas
            line_select++; // Alterna as linhas, por meio de incremento dessa variável
            customer_spot_select_spacer_value = 0; // Para alternar o espaçamento necessário entre as vagas da linha
        }

        // Tratamento do visual da vaga selecionada
        if (i == customer_spot_select_spotview_value){ // Se a iteração atual for a vaga selecionada
            customer_spot_select_info_bool = true; // Vai realizar a inversão da fonte do numero da vaga
            ssd1306_rect(&ssd, 4+(8*line_select)+(1*line_select), 5+(16*customer_spot_select_spacer_value)+(8*customer_spot_select_spacer_value), 19, 9, cor, cor); // Gera o branco da vaga selecionada
        }
        else{
            customer_spot_select_info_bool = false; // Se não, mantém o estado padrão
        }

        if(i<9){ // Gera string para as menores que 10
            int_2_char(i+1, &converted_num); // Converte o dígito à direita do número para char
            converted_string[0] = '0'; // Char para melhorar o visual
            converted_string[1] = converted_num; // Int convertido para char
            converted_string[2] = '\0'; // Terminador nulo da String
            ssd1306_draw_string(&ssd, converted_string, 8+(16*customer_spot_select_spacer_value)+(8*customer_spot_select_spacer_value), 5+(8*line_select)+(1*line_select), customer_spot_select_info_bool);
        }
        else if(i<19){ // Gera string para as menores que 20 e maiores que 10
            int_2_char(i-9, &converted_num); // Converte o dígito à direita do número para char
            converted_string[0] = '1'; // Char para melhorar o visual
            converted_string[1] = converted_num; // Int convertido para char
            converted_string[2] = '\0'; // Terminador nulo da String
            ssd1306_draw_string(&ssd, converted_string, 8+(16*customer_spot_select_spacer_value)+(8*customer_spot_select_spacer_value), 5+(8*line_select)+(1*line_select), customer_spot_select_info_bool);
        }
        else{ // Gera a string para as maiores que 20
            int_2_char(i-19, &converted_num); // Converte o dígito à direita do número para char
            converted_string[0] = '2'; // Char para melhorar o visual
            converted_string[1] = converted_num; // Int convertido para char
            converted_string[2] = '\0'; // Terminador nulo da String
            ssd1306_draw_string(&ssd, converted_string, 8+(16*customer_spot_select_spacer_value)+(8*customer_spot_select_spacer_value), 5+(8*line_select)+(1*line_select), customer_spot_select_info_bool);
        }

        customer_spot_select_spacer_value++; // Incrementa o spacer para separar as vagas no display
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

        customer_select_spot(vrx_value, vry_value); // Tela de seleção de vaga para o cliente
        



        ssd1306_send_data(&ssd); // Envia os dados para o display, atualizando o mesmo

        printf("Joystick X: %d | Joystick Y: %d\n", vrx_value, vry_value); // Teste da USB
        sleep_ms(30);
    }
}
