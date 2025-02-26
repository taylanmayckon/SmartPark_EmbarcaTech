#include "led_matrix.h"

#define MATRIX_PIN 7

// Variável que define a intensidade do brilho dos LEDS
static const uint8_t intensidade_max = 10;

// Declaração das cores dos frames que serão usados na matriz de leds
uint8_t led_r = intensidade_max;
uint8_t led_g = 0;
uint8_t led_b = 0;

// Quantidade de pixels
#define NUM_PIXELS 25

// Buffer que armazena o frame atual
bool led_buffer[NUM_PIXELS] = {
    0, 0, 0, 0, 0, // Linha 0 (Certo)
    0, 0, 0, 0, 0, // Linha 1 (Espelhado)
    0, 0, 0, 0, 0, // Linha 2 (Certo)
    0, 0, 0, 0, 0, // Linha 3 (Espelhado)
    0, 0, 0, 0, 0, // Linha 4 (Certo)
};

static inline void put_pixel(uint32_t pixel_grb){
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

// Função que vai transformar valores correspondentes ao padrão RGB em dados binários
uint32_t urgb_u32(double r, double g, double b){
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

// Função que vai imprimir cada pixel na matriz
void set_leds(uint8_t r, uint8_t g, uint8_t b){
    // Define a cor com base nos parâmetros fornecidos
    uint32_t color = urgb_u32(r, g, b);

    // Define todos os LEDs com a cor especificada
    // Faz o processo de virar de cabeça para baixo o arranjo
    for (int i = NUM_PIXELS-1; i >= 0; i--){
        if (led_buffer[i]){
            put_pixel(color); // Liga o LED com um no buffer
        }
        else{
            put_pixel(0);  // Desliga os LEDs com zero no buffer
        }
    }
}

// Função que faz as operações necessárias para atualizar o display
void atualiza_vagas(bool *spots_info){
    // Ordenando corretamente o vetor recebido no buffer
    int j = 0; // Variável para controle do index espelhado
    for(int i=0; i<25; i++){
        if(i>4 && i<10){
            led_buffer[i] = spots_info[9-j];
            j++;
        }
        else if(i>14 && i<20){
            led_buffer[i] = spots_info[19-j];
            j++;
        }
        else{
            j=0;
            led_buffer[i] = spots_info[i];
        }
    }

    // Chamando a função que vai imprimir as vagas
    set_leds(led_r, led_g, led_b);
}