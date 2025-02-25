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

// Variáveis para o controle da interrupção
uint32_t last_interrupt_time = 0; // Controla o debounce da interrupção
int display_page = 0; // Controla que página será exibida
bool display_mode = true; // Controla o modo de visualização (cliente ou proprietário)


// Vetor indicativo do estado das vagas
bool spots_state[25] = {
    1, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
};



// Função de tratamento de interrupções
void gpio_irq_handler(uint gpio, uint32_t events){
    // Tratamento do debounce
    uint32_t current_time = to_us_since_boot(get_absolute_time()); // Obtendo o tempo atual
    if(current_time - last_interrupt_time > 200000){ // Debounce setado para 200ms
        last_interrupt_time = current_time; // Atualizando o tempo da ultima interrupção

        switch(gpio){
            case JOYSTICK_BUTTON:
                display_mode = !display_mode; // Troca o modo de visualização
                display_page = 0; // Vai para a primeira tela do display
                break;
                
            case BUTTON_A:
                display_page--; // Decrementa, navegando para a tela anterior
                if(display_page<0){
                    display_page=0; // Impede que vá para uma tela inválida
                }
                break;

            case BUTTON_B:
                if(busy_spot_popup){ // Condição para desativar o popup
                    busy_spot_popup = false;
                }
                else if(display_page==1 && display_mode && spots_state[customer_spot_select_spotview_value] && !busy_spot_popup){ // Condição para ativar popup
                    // Tem que estar na tela de seleção, no display dos clientes, com uma vaga selecionada no estado ocupado e o popup inicialmente tem que estar desativado
                    busy_spot_popup = true; // Para ativar o popup de tela ocupada
                }
                else{ // Se não for nenhum dos casos acima, ele mantém o fluxo e passa para a próxima tela
                    customer_selected_spot_time = 0; // Zera o tempo a ser exibido na seleção
                    display_page++; // Incrementa, navegando para a próxima tela
                    if (display_mode && display_page>3){ // Se tiver na tela do cliente, página acima de 3
                        display_page=3; // Força para que mantenha a tela 3
                    }
                    if (!display_mode && display_page>3){ // Se tiver na tela do proprietário, página acima de 3
                        display_page=3; // Força para que mantenha a tela 3
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
    printf("%d:%d\n", minutos, segundos);

    ssd1306_rect(&ssd, 0, 0, 127, 11, cor, cor); // Barra superior
    ssd1306_draw_string(&ssd, "Tempo de Vaga", 11, 2, true); // Texto superior
    // Imprimindo informações sobre a vaga
    ssd1306_draw_string(&ssd, "VAGA", 35, 14, false); 
    int_2_string(customer_spot_select_spotview_value); // Convertendo o valor da vaga atual para uma string
    ssd1306_draw_string(&ssd, converted_string, 35+40, 14, false); // Imprime o número da vaga

    // Imprimindo o tempo
    int_2_string(minutos); // Convertendo os minutos em string
    ssd1306_draw_string(&ssd, converted_string, 35, 26, false); // Imprimindo no display
    ssd1306_draw_char(&ssd, ':', 35+16, 26, false); // Imprime a divisória do tempo
    int_2_string(segundos); // Convertendo os segundos em string
    ssd1306_draw_string(&ssd, converted_string, 35+24, 26, false);

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

        if(display_mode){ // true = Tela do cliente
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
            }
        }


        ssd1306_send_data(&ssd); // Envia os dados para o display, atualizando o mesmo

        //printf("Joystick X: %d | Joystick Y: %d\n", vrx_value, vry_value); // Teste da USB
        sleep_ms(30);
    }
}
