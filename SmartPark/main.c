#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/timer.h"
#include "libs/font.h"
#include "libs/ssd1306.h"
#include "libs/led_matrix.h"
#include "hardware/pwm.h"

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
// Valores do joystick declarados globalmente
uint16_t vrx_value, vry_value;
// Definições dos botões
#define BUTTON_A 5
#define BUTTON_B 6
// Definindo a máscara de entrada
#define INPUT_MASK ((1 << JOYSTICK_BUTTON) | (1 << BUTTON_A) | (1 << BUTTON_B))

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

bool busy_spot_popup = false; // Booleano para controlar o popup caso a vaga do cliente esteja ocupada na tela de seleção

uint32_t customer_selected_spot_time = 0; // Variável para armazenar o tempo desejado para a vaga (formato em us)
uint32_t customer_selected_spot_delay = 0; // Variável para controlar a velocidade das entradas


// Variáveis para controle visual do display dos proprietários
uint32_t owner_spot_select_info_time = 0; // Controla o tempo de alternar a mensagem superior na seleção de vagas
uint32_t owner_spot_select_spotview_time = 0; // Variável para controlar o tempo de alternagem entre as vagas no display
bool owner_spot_select_info_bool = false; // Variável para alterar visualização da vaga a ser selecionada
bool owner_spot_select_upper_info_bool = false;
int owner_spot_select_spotview_value = 0; // Variável que controla a navegação do proprietário entre as vagas
int owner_spot_select_spacer_value = 0; // Controla o espaçamento entre o numero das vagas no display

bool popup_expand_info = false; // Variável para controlar o popup das informações da vaga

// Variáveis para o controle da interrupção
uint32_t last_interrupt_time = 0; // Controla o debounce da interrupção
int display_page = 0; // Controla que página será exibida
bool display_mode = true; // Controla o modo de visualização (cliente ou proprietário)

// Vetor indicativo do estado das vagas
bool spots_state[25] = {
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
};

// Vetor para armazenar quando foi reservada cada vaga
uint32_t spots_time[25] = {
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
};

// Vetor para armazenar quanto tempo quer a reserva
uint32_t spots_input[25] = {
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
};

// Variáveis da PIO declaradas no escopo global
PIO pio;
uint sm;
// Constantes para a matriz de leds
#define IS_RGBW false
#define LED_MATRIX_PIN 7

// Variáveis para o PWM
uint wrap = 2048; // Valor do TOP
uint di = 2; // Divisor inteiro
// Equivale a 30,5 kHz
#define LED_RED 13
#define LED_GREEN 11
#define OUTPUT_MASK ((1 << LED_RED) | (1 << LED_GREEN))
uint red_pwm_slice, green_pwm_slice;


// Função para configurar o PWM
uint config_pwm(uint gpio){
    gpio_set_function(gpio, GPIO_FUNC_PWM); // Habilitando o pino como pwm
    uint slice = pwm_gpio_to_slice_num(gpio); // Obtendo o canal (slice) do PWM 
    pwm_set_clkdiv(slice, di); // Definindo o divisor de clock do PWM
    pwm_set_wrap(slice, wrap); // Definindo o valor de wrap (contador do PWM)
    pwm_set_gpio_level(gpio, 0); // Iniciando com 0% de duty cycle
    pwm_set_enabled(slice, true); // Habilitando o pwm no slice correspondente

    return slice;
}


// Função periódica para verificar a liberação das vagas e enviar dados via USB
bool repeating_timer_callback(struct repeating_timer *t){
    uint32_t current_time = to_us_since_boot(get_absolute_time()); // Obtendo o tempo atual
    for(int i=0; i<25; i++){ // Para varrer as 25 vagas
        if((int32_t)(current_time-spots_time[i]) > spots_input[i]){ // Se detectar que o tempo de reserva passou
            spots_state[i] = 0; // Marca como vaga livre
            spots_time[i] = 0; // Zera o tempo reservado para a mesma
            spots_input[i] = 0; // Zera o input
        }
    }

    // Enviando via USB
    // Normalizando valores
    float x_normal, y_normal;
    if(vrx_value>2048){
        x_normal = (float)(vrx_value-2048)/2048;
    }
    else{
        x_normal = (float)(2048-vrx_value)/2048;
    }
    if(vry_value>2048){
        y_normal = (float)(vry_value-2048)/2048;
    }
    else{
        y_normal = (float)(2048-vry_value)/2048;
    }

    for(int i=0; i<25; i++){
        printf("%d;", spots_state[i]);
    }
    printf("%.2f;%.2f\n", x_normal, y_normal);

    return true;
}

// Função de tratamento de interrupções da GPIO
void gpio_irq_handler(uint gpio, uint32_t events){
    // Tratamento do debounce
    uint32_t current_time = to_us_since_boot(get_absolute_time()); // Obtendo o tempo atual
    if(current_time - last_interrupt_time > 200000){ // Debounce setado para 200ms
        last_interrupt_time = current_time; // Atualizando o tempo da ultima interrupção

        switch(gpio){
            case JOYSTICK_BUTTON:
                display_mode = !display_mode; // Troca o modo de visualização
                display_page = 0; // Vai para a primeira tela do display
                popup_expand_info = false; // Desligando popups
                busy_spot_popup = false; // Desligando popups
                break;

            case BUTTON_A:
                if(display_mode){ // true = Tela do cliente
                    if(display_page<1){
                        display_page=0; // Impede que vá para uma tela inválida
                    }
                    else if(display_page==3){ // Para não ter como voltar pra tela anterior depois de confirmar
                        display_page = 0;
                    }
                    else{
                        display_page--; // Decrementa, navegando para a tela anterior 
                    }
                }
                
                else{ // False = Tela do proprietário
                    popup_expand_info = !popup_expand_info; // Alterna a visualização do popup
                }
                break;

            case BUTTON_B:
                if(display_mode){ // Tratamento das telas de clientes
                    if(busy_spot_popup){ // Condição para desativar o popup
                        busy_spot_popup = false;
                    }
                    else if(display_page==1 && spots_state[customer_spot_select_spotview_value] && !busy_spot_popup){ // Condição para ativar popup
                        // Tem que estar na tela de seleção, no display dos clientes, com uma vaga selecionada no estado ocupado e o popup inicialmente tem que estar desativado
                        busy_spot_popup = true; // Para ativar o popup de tela ocupada
                    }
                    else if(display_page==2 && !spots_state[customer_spot_select_spotview_value]){ // Caso esteja na página 2, selecionado uma vaga vazia (bool = false)
                        display_page++; // Vai para a última tela
                        spots_state[customer_spot_select_spotview_value] = 1; // Marca a vaga como ocupada
                        spots_time[customer_spot_select_spotview_value] = current_time; // Armazena quando foi reservada a vaga
                        spots_input[customer_spot_select_spotview_value] = customer_selected_spot_time; // Salva o tempo de reserva em us
                    }
                    else{ // Se não for nenhum dos casos acima, ele mantém o fluxo e passa para a próxima tela
                        customer_selected_spot_time = 0; // Zera o tempo a ser exibido na seleção
                        display_page++; // Incrementa, navegando para a próxima tela
                        if (display_page>3){ // Se tiver na tela do cliente, página acima de 3
                            display_page=0; // Força para que mantenha a tela 3
                        }
                    }
                }
                else{ // Tratamento das telas de funcionários
                    display_page++;

                    if(display_page>1){ // Forçando a volta para tela inicial se for além das disponíveis
                        display_page=0;
                    }
                }
                break;
        }
        
    }
}


void generate_border(){
    ssd1306_rect(&ssd, 0, 0, 127, 2, cor, cor); // Borda superior, altura=2
    ssd1306_rect(&ssd, 61, 0, 127, 2, cor, cor); // Borda inferior, altura=2
    ssd1306_rect(&ssd, 0, 0, 2, 63, cor, cor); // Borda esquerda, largura=2
    ssd1306_rect(&ssd, 0, 125, 2, 63, cor, cor); // Borda direita, largura=2
}

/////////////////////////////////////////////////////// FUNÇÕES DO CLIENTE ///////////////////////////////////////////////////////
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

void int_2_string(int num){
    if(num<9){ // Gera string para as menores que 10
        int_2_char(num, &converted_num); // Converte o dígito à direita do número para char
        converted_string[0] = '0'; // Char para melhorar o visual
        converted_string[1] = converted_num; // Int convertido para char
        converted_string[2] = '\0'; // Terminador nulo da String 
    }
    else{ // Gera a string para as maiores/iguais que 10
        int divider = num/10; // Obtém as dezenas
        int_2_char(divider, &converted_num);
        converted_string[0] = converted_num;

        int_2_char(num%10, &converted_num); // Obtém a parte das unidades
        converted_string[1] = converted_num; // Int convertido para char
        converted_string[2] = '\0'; // Terminador nulo da String
    }
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

        int_2_string(i+1); // Converte o número da vaga em uma string, que fica na variável global "converted_string"

        // Escreve o número da vaga atual
        ssd1306_draw_string(&ssd, converted_string, 8+(16*customer_spot_select_spacer_value)+(8*customer_spot_select_spacer_value), 5+(8*line_select)+(1*line_select), customer_spot_select_info_bool); 

        customer_spot_select_spacer_value++; // Incrementa o spacer para separar as vagas no display
    }

}


// Função para o popup  caso selecione vaga ocupada na função acima
void customer_busy_spot_func(){
    ssd1306_rect(&ssd, 16, 10, 108, 39, cor, !cor);
    ssd1306_rect(&ssd, 18, 12, 104, 35, cor, cor); // Cria o retangulo do popup
    ssd1306_draw_string(&ssd, "VAGA OCUPADA", 16, 22, true);
    ssd1306_draw_string(&ssd, "PARA VOLTAR", 20, 32, true);
    ssd1306_draw_string(&ssd, "PRESSIONE B", 20, 42, true);
}


// Função que seleciona o tempo para a vaga desejada
void customer_select_spot_time(uint16_t y_value){
    uint32_t current_time = to_us_since_boot(get_absolute_time()); // Pegando o tempo de execução atual do código

    if(current_time-customer_selected_spot_delay > 100000){ // Só vai incrementar tempo a cada 100ms
        customer_selected_spot_delay = current_time; // Atualiza quando foi a ultima ação
        if(y_value>3000){ // Joystick para cima
            customer_selected_spot_time+=5000000; // Incrementa 5 segundos (mas no formato de us)
        } else if(y_value<1000){ // Joystick para baixo
            if(customer_selected_spot_time<5000001){ // Condição para não ocorrer um bug com valores negativos
                customer_selected_spot_time=0; // Nesse caso, só zera o valor do tempo
            }
            else{
                customer_selected_spot_time-=5000000; // Decrementa 5 segundos (mas no formato de us)
            }
        }
    }
    
    // Calculando os valores no formato minuto:segundo
    int minutos = customer_selected_spot_time/60000000; // Calcula a quantidade de minutos 
    int segundos = (customer_selected_spot_time%60000000) / 1000000; // Calcula a quantidade de segundos

    ssd1306_rect(&ssd, 0, 0, 127, 11, cor, cor); // Barra superior
    ssd1306_draw_string(&ssd, "Tempo de Vaga", 11, 2, true); // Texto superior
    // Imprimindo informações sobre a vaga
    ssd1306_draw_string(&ssd, "VAGA", 35, 14, false); 
    int_2_string(customer_spot_select_spotview_value+1); // Convertendo o valor da vaga atual para uma string
    ssd1306_draw_string(&ssd, converted_string, 35+40, 14, false); // Imprime o número da vaga

    // Imprimindo o tempo
    ssd1306_draw_char(&ssd, '>', 59, 28, false); // Simbolo para cima
    int_2_string(minutos); // Convertendo os minutos em string
    ssd1306_draw_string(&ssd, converted_string, 43, 37, false); // Imprimindo no display
    ssd1306_draw_char(&ssd, ':', 43+16, 37, false); // Imprime a divisória do tempo
    int_2_string(segundos); // Convertendo os segundos em string
    ssd1306_draw_string(&ssd, converted_string, 43+24, 37, false);
    ssd1306_draw_char(&ssd, '<', 59, 46, false); // Símbolo para baixo
}


// Função para a tela de confirmação da vaga
void customer_confirmation_view(){
    ssd1306_rect(&ssd, 0, 0, 127, 11, cor, cor); // Borda
    ssd1306_draw_string(&ssd, "SmartPark", 28, 2, true); // Logo em negativo
    ssd1306_draw_string(&ssd, "Confirmado!", 19, 18, false);  // Linha 1
    ssd1306_draw_string(&ssd, "Pressione B", 19, 32, false); // Linha 2
    ssd1306_draw_string(&ssd, "Para sair", 27, 46, false); // Linha 3
}


/////////////////////////////////////////////////////// FUNÇÕES DO PROPRIETÁRIO ///////////////////////////////////////////////////////
// Função para a tela das vagas para o proprietário
void owner_select_spot(uint16_t x_value, uint16_t y_value){
    ssd1306_rect(&ssd, 0, 0, 127, 11, cor, cor); // Borda superior

    uint32_t current_time = to_us_since_boot(get_absolute_time()); // Pega o tempo de execução atual

    // Controla a informação superior a ser exibida
    if(current_time - owner_spot_select_info_time > 4000000){
        owner_spot_select_info_time = current_time; 
        owner_spot_select_upper_info_bool = !owner_spot_select_upper_info_bool;
    }

    if(owner_spot_select_upper_info_bool){
        ssd1306_draw_string(&ssd, "Confirme com A", 7, 2, true); // Texto superior em negativo
    }
    else{
        ssd1306_draw_string(&ssd, "Alterne com B", 11, 2, true); // Texto superior em negativo
    }
    
    // Controle de tempo para a navegação do display
    if(current_time - owner_spot_select_spotview_time > 200000){ // 200ms de intervalo
        owner_spot_select_spotview_time = current_time; // Atualiza o tempo

        // Navegação esquerda-direita
        if(x_value>3000){ // Joystick para a direita
            owner_spot_select_spotview_value++; // Altera para a vaga da direita (+1)
        } else if(x_value<1000){ // Joystick para a esquerda
            owner_spot_select_spotview_value--; // Altera para a vaga da esquerda (-1)
        }

        if(y_value>3000){ // Joystick para cima
            owner_spot_select_spotview_value-=5; // Altera para a vaga de cima (-5)
        } else if(y_value<1000){ // Joystick para baixo
            owner_spot_select_spotview_value+=5; // Altera para a vaga de baixo (+5)
        }

        // Condicionais para não ultrapassar o limite de vags
        if(owner_spot_select_spotview_value<0){ // Se tentar menor que 0, joga para a última
            owner_spot_select_spotview_value=24;
        }
        else if(owner_spot_select_spotview_value>24){ // Se tentar maior que 24, joga para a primeira
            owner_spot_select_spotview_value=0;
        }
    }


    int line_select = 0; // Variável que aponta para a linha de vagas a ser escrita (zerada antes de cada loop for a seguir)
    // Visualização de todas as vagas possíveis no display
    for(int i=0; i<25; i++){
        if(i%5==0){ // Troca a linha quando ultrapassar 5 iterações e reseta o spacer das vagas
            line_select++; // Alterna as linhas, por meio de incremento dessa variável
            owner_spot_select_spacer_value = 0; // Para alternar o espaçamento necessário entre as vagas da linha
        }

        // Tratamento do visual da vaga selecionada
        if (i == owner_spot_select_spotview_value){ // Se a iteração atual for a vaga selecionada
            owner_spot_select_info_bool = true; // Vai realizar a inversão da fonte do numero da vaga
            ssd1306_rect(&ssd, 4+(8*line_select)+(1*line_select), 5+(16*owner_spot_select_spacer_value)+(8*owner_spot_select_spacer_value), 19, 9, cor, cor); // Gera o branco da vaga selecionada
        }
        else{
            owner_spot_select_info_bool = false; // Se não, mantém o estado padrão
        }

        int_2_string(i+1); // Converte o número da vaga em uma string, que fica na variável global "converted_string"

        // Escreve o número da vaga atual
        ssd1306_draw_string(&ssd, converted_string, 8+(16*owner_spot_select_spacer_value)+(8*owner_spot_select_spacer_value), 5+(8*line_select)+(1*line_select), owner_spot_select_info_bool); 

        owner_spot_select_spacer_value++; // Incrementa o spacer para separar as vagas no display
    }
}

// Função que ativa o popup das informações da vaga
void owner_expand_info(){
    uint32_t current_time = to_us_since_boot(get_absolute_time()); // Pegando o tempo atual
    
    int32_t remaining_time = -(current_time - (spots_time[0] + spots_input[0])); // Variável que armazena o tempo restante da vaga
    int minutos = (remaining_time)/60000000;
    int segundos = (remaining_time%60000000) / 1000000; // Calcula a quantidade de segundos

    ssd1306_rect(&ssd, 16, 10, 108, 39, cor, !cor);
    ssd1306_rect(&ssd, 18, 12, 104, 35, cor, cor); // Cria o retangulo do popup
    ssd1306_draw_string(&ssd, "VAGA", 35, 22, true); // Linha 1
    int_2_string(owner_spot_select_spotview_value+1); // Convertendo o numero da vaga em uma string
    ssd1306_draw_string(&ssd, converted_string, 35+(8*4)+8, 22, true); // Linha 1 

    if(spots_input[owner_spot_select_spotview_value] == 0){ // Quando a vaga estiver livre
        ssd1306_draw_string(&ssd, "DESOCUPADA", 23, 32, true); // Linha 2
    }
    else{
        int_2_string(minutos);
        ssd1306_draw_string(&ssd, converted_string, 43, 32, true); // Linha 2
        ssd1306_draw_char(&ssd, ':', 43+16, 32, true); // Linha 2
        int_2_string(segundos);
        ssd1306_draw_string(&ssd, converted_string, 43+24, 32, true); // Linha 2
    }

    ssd1306_draw_string(&ssd, "PRESSIONE A", 20, 42, true); // Linha 3
}

// Função da tela de luminosidade
void owner_luminosity_level(uint x_value, uint y_value){
    float x_normal, y_normal;

    // Normalizando valores
    if(x_value>2048){
        x_normal = (float)(x_value-2048)/2048;
    }
    else{
        x_normal = (float)(2048-x_value)/2048;
    }
    if(y_value>2048){
        y_normal = (float)(y_value-2048)/2048;
    }
    else{
        y_normal = (float)(2048-y_value)/2048;
    }

    ssd1306_rect(&ssd, 0, 0, 127, 11, cor, cor); // Borda
    ssd1306_draw_string(&ssd, "Luminosidade", 15, 2, true); // Logo em negativo

    // Andar 0
    ssd1306_rect(&ssd, 12, 13, 60, 12, cor, cor); // Fundo branco
    ssd1306_draw_string(&ssd, "Andar 0", 16, 16, true); // Nome andar
    ssd1306_rect(&ssd, 12+12, 13, 60+40, 8, cor, !cor); // Frame vazio (a ser preenchido)
    ssd1306_rect(&ssd, 12+12, 13, 100*x_normal, 8, cor, cor); // Preenchimento dinamico

    // Andar 1
    ssd1306_rect(&ssd, 52-8-3, 56-3, 60, 12, cor, cor); // Fundo branco
    ssd1306_draw_string(&ssd, "Andar 1", 56, 52-8, true); // Nome andar
    ssd1306_rect(&ssd, 52, 13, 60+40, 8, cor, !cor); // Frame vazio (a ser preenchido)
    ssd1306_rect(&ssd, 52, 13, 100*y_normal, 8, cor, cor); // Preenchimento dinamico
}

// Função de ajuste da luminosidade
void change_led_luminosity(uint gpio, uint joy_value){
    if(joy_value>2048){ // Analógico do meio para cima
        pwm_set_gpio_level(gpio, joy_value-2048); 
    }
    else{ // Analógico do meio para baixo
        pwm_set_gpio_level(gpio, 2048-joy_value); 
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

    // Iniciando os pinos dos botões
    gpio_init_mask(INPUT_MASK);
    // Habilitando os pull ups internos
    gpio_pull_up(BUTTON_A);
    gpio_pull_up(BUTTON_B);
    gpio_pull_up(JOYSTICK_BUTTON);

    // Configurando a interrupção dos botões
    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BUTTON_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(JOYSTICK_BUTTON, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    // Configurando um temporizador de repetição, para liberar as vagas e enviar dados via USB
    struct repeating_timer timer;
    // Configurnado o repeating timer
    add_repeating_timer_ms(500, repeating_timer_callback, NULL, &timer); // Configurada para repetir a cada 500ms

    // Inicializando a PIO
    pio = pio0;
    sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, LED_MATRIX_PIN, 800000, IS_RGBW);

    // Configurando PWM
    gpio_init_mask(OUTPUT_MASK);
    gpio_set_dir_out_masked(OUTPUT_MASK);
    gpio_clr_mask(OUTPUT_MASK);
    red_pwm_slice = config_pwm(LED_RED);
    green_pwm_slice = config_pwm(LED_GREEN);

    

    while (true) {
        // Leituras do ADC
        // Leitura do Eixo X (Canal 1)
        adc_select_input(1);
        vrx_value = adc_read();
        // Leitura do Eixo Y (Canal 0)
        adc_select_input(0);
        vry_value = adc_read();
        // Deadzone para os joysticks
        if (vrx_value>=1900 && vrx_value<=2194){
            vrx_value=2048;
        }
        if (vry_value>=1900 && vry_value<=2194){
            vry_value=2048;
        }

        // Atualiza a matriz de leds endereçáveis
        atualiza_vagas(spots_state);

        // Gerenciamento do display
        ssd1306_fill(&ssd, false); // Limpa o display

        generate_border(); // Gera a borda com largura de 2 pixels

        if(display_mode){ // true = Tela do cliente
            // Zerando o PWM dos leds
            pwm_set_gpio_level(LED_GREEN, 0); 
            pwm_set_gpio_level(LED_RED, 0);
            switch(display_page){
                case 0: // Tela de standby, aguardando interação
                    customer_standby();
                    break;
                case 1: // Tela de seleção de vaga
                    customer_select_spot(vrx_value, vry_value);
                    if(busy_spot_popup){ // Ativa o popup informativo caso tente selecionar uma vaga ocupada
                        customer_busy_spot_func();
                    }
                    break;
                case 2:
                    customer_select_spot_time(vry_value); // Função para selecionar o tempo de reserva da vaga
                    break;
                case 3:
                    customer_confirmation_view();
                    break;
            }
        }
        else{ // false = Tela do proprietário
            switch(display_page){
                case 0: // Tela de seleção de vagas
                    // Zerando o PWM dos leds
                    pwm_set_gpio_level(LED_GREEN, 0); 
                    pwm_set_gpio_level(LED_RED, 0);
                    owner_select_spot(vrx_value, vry_value);
                    if(popup_expand_info){ // Ativa o popup caso seja selecionada a vaga
                        owner_expand_info();
                    }
                    break;
                case 1:
                    // (Eixo Y - Verde) Led equivalente ao andar 0
                    change_led_luminosity(LED_GREEN, vrx_value);
                    // (Eixo X - Vermelho) Led equivalente ao andar 1
                    change_led_luminosity(LED_RED, vry_value);

                    owner_luminosity_level(vrx_value, vry_value);
                    break;
            }
        }

        ssd1306_send_data(&ssd); // Envia os dados para o display, atualizando o mesmo

        sleep_ms(30);
    }
}
