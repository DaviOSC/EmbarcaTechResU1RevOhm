#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"

#include "lib/ssd1306.h"
#include "lib/font.h"
#include "pico/bootrom.h"
#include "pio_matrix.pio.h"

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define ADC_PIN 28 // GPIO para o voltímetro
#define Botao_A 5  // GPIO para botão A
#define botaoB 6
#define OUT_PIN 7
#define NUM_PIXELS 25

int R_conhecido = 10000;    // Resistor de 10k ohm
float R_x = 0.0;           // Resistor desconhecido
float ADC_VREF = 3.31;     // Tensão de referência do ADC
int ADC_RESOLUTION = 4095; // Resolução do ADC (12 bits)

// Valores comerciais padrão E24 (5%)
const int E24_VALUES[] = {510, 560, 620, 680, 750, 820, 910, 1000, 1100, 1200, 1300, 1500, 1600, 1800, 2000, 2200, 2400, 2700, 3000, 3300, 3600, 3900, 4300, 4700, 5100, 5600, 6200, 6800, 7500, 8200, 9100, 10000, 11000, 12000, 13000, 15000, 16000, 18000, 20000, 22000, 24000, 27000, 30000, 33000, 36000, 39000, 43000, 47000, 51000, 56000, 62000, 68000, 75000, 82000, 91000, 100000};

// Cores correspondentes às faixas do resistor
const char *COLOR_CODES[] = {"Preto", "Marrom", "Vermelho", "Laranja", "Amarelo", "Verde", "Azul", "Violeta", "Cinza", "Branco"};

uint32_t matrix_rgb(double r, double g, double b);
void pio_drawn(double *desenho, uint32_t valor_led, PIO pio, uint sm, double r1, double g1, double b1, double r2, double g2, double b2, double r3, double g3, double b3);
int find_nearest_e24(float resistance);
void get_color_bands(int resistance, int *band1, int *band2, int *multiplier);
void get_color_bands(int resistance, int *band1, int *band2, int *multiplier);
void draw_resistor_bands(int band1, int band2, int multiplier, PIO pio, uint sm);
void gpio_irq_handler(uint gpio, uint32_t events);

int main()
{
    // Para ser utilizado o modo BOOTSEL com botão B
    stdio_init_all();
    gpio_init(botaoB);
    gpio_set_dir(botaoB, GPIO_IN);
    gpio_pull_up(botaoB);
    gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    // Aqui termina o trecho para modo BOOTSEL com botão B
    PIO pio = pio0;
    uint32_t valor_led;

    // Configurações da PIO
    uint offset = pio_add_program(pio, &pio_matrix_program);
    uint sm = pio_claim_unused_sm(pio, true);
    pio_matrix_program_init(pio, sm, offset, OUT_PIN);

    gpio_init(Botao_A);
    gpio_set_dir(Botao_A, GPIO_IN);
    gpio_pull_up(Botao_A);

    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    ssd1306_t ssd;
    ssd1306_init(&ssd, 128, 64, false, endereco, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    adc_init();
    adc_gpio_init(ADC_PIN);

    char str_resistance[10];
    char str_band1[10], str_band2[10], str_multiplier[10];
    //int index = 0;
    while (true)
    {
        //R_conhecido = E24_VALUES[index];
        adc_select_input(2);

        float soma = 0.0f;
        for (int i = 0; i < 500; i++)
        {
            soma += adc_read();
            sleep_ms(1);
        }
        float media = soma / 500.0f;
        R_x = (R_conhecido * media) / (ADC_RESOLUTION - media);
        int nearest_resistance = find_nearest_e24(R_x);

        int band1, band2, multiplier;
        get_color_bands(nearest_resistance, &band1, &band2, &multiplier);

        sprintf(str_band1, "%s", COLOR_CODES[band1]);
        sprintf(str_band2, "%s", COLOR_CODES[band2]);
        sprintf(str_multiplier, "%s", COLOR_CODES[multiplier]);
        sprintf(str_resistance, "%d", nearest_resistance);
        sprintf(str_band1, "%s", COLOR_CODES[band1]);
        sprintf(str_band2, "%s", COLOR_CODES[band2]);
        sprintf(str_multiplier, "%s", COLOR_CODES[multiplier]);

        int cor = 1;
        // cor = !cor;
        // Atualiza o conteúdo do display com animações
        ssd1306_fill(&ssd, !cor);                     // Limpa o display
        ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor); // Desenha um retângulo
        ssd1306_line(&ssd, 3, 16, 123, 16, cor);      // Desenha uma linha
        ssd1306_line(&ssd, 3, 27, 123, 27, cor);
        ssd1306_line(&ssd, 3, 38, 123, 38, cor);
        ssd1306_line(&ssd, 3, 49, 123, 49, cor);
        ssd1306_draw_string(&ssd, "Ohmimetro", 30, 6);     // Exibe "Ohmimetro"
        ssd1306_draw_string(&ssd, "Fx1:", 6, 18);          // Exibe "Faixa 1:"
        ssd1306_draw_string(&ssd, str_band1, 55, 18);      // Exibe o texto da primeira faixa de cor
        ssd1306_draw_string(&ssd, "Fx2:", 6, 29);          // Exibe "Faixa 2:"
        ssd1306_draw_string(&ssd, str_band2, 55, 29);      // Exibe o texto da segunda faixa de cor
        ssd1306_draw_string(&ssd, "Mult.:", 6, 40);        // Exibe "Multiplicador:"
        ssd1306_draw_string(&ssd, str_multiplier, 55, 40); // Exibe o texto do multiplicador
        ssd1306_draw_string(&ssd, "Resi.:", 6, 52);        // Exibe "Resistencia:"
        ssd1306_draw_string(&ssd, str_resistance, 55, 52); // Exibe o valor da resistência
        ssd1306_send_data(&ssd);                           // Atualiza o display

        draw_resistor_bands(band1, band2, multiplier, pio, sm);
        sleep_ms(3000);
        //index++;
    }
}

// A função recebe os valores de vermelho, verde e azul do LED
uint32_t matrix_rgb(double r, double g, double b)
{
    unsigned char R, G, B;
    R = r * 255;
    G = g * 255;
    B = b * 255;
    return (G << 24) | (R << 16) | (B << 8);
}

// rotina para acionar a matrix de leds
void pio_drawn(double *desenho, uint32_t valor_led, PIO pio, uint sm,
               double r1, double g1, double b1, // Cor da coluna 1
               double r2, double g2, double b2, // Cor da coluna 2
               double r3, double g3, double b3) // Cor da coluna 3
{
    for (int16_t i = 0; i < NUM_PIXELS; i++)
    {
        // Determina a linha e coluna
        int linha = i / 5;
        int coluna = i % 5;

        // Ajusta as colunas para linhas invertidas (1 e 4)
        if (linha == 1 || linha == 3) // Linhas 1 e 4
        {
            if (coluna == 1)
                coluna = 3; // Coluna 2 vira 4
            else if (coluna == 2)
                coluna = 2; // Coluna 3 permanece
            else if (coluna == 3)
                coluna = 1; // Coluna 4 vira 2
        }

        // Determina a cor com base na coluna
        if (coluna == 3) // Coluna 2
        {
            valor_led = matrix_rgb(r1 * desenho[i], g1 * desenho[i], b1 * desenho[i]);
        }
        else if (coluna == 2) // Coluna 3
        {
            valor_led = matrix_rgb(r2 * desenho[i], g2 * desenho[i], b2 * desenho[i]);
        }
        else if (coluna == 1) // Coluna 4
        {
            valor_led = matrix_rgb(r3 * desenho[i], g3 * desenho[i], b3 * desenho[i]);
        }
        else
        {
            valor_led = 0; // Pixels fora das colunas centrais ficam apagados
        }

        // Envia o valor para a PIO
        pio_sm_put_blocking(pio, sm, valor_led);
    }
}

// Função para encontrar o valor E24 mais próximo
int find_nearest_e24(float resistance)
{
    int nearest = E24_VALUES[0];
    for (int i = 1; i < sizeof(E24_VALUES) / sizeof(E24_VALUES[0]); i++)
    {
        if (abs(E24_VALUES[i] - resistance) < abs(nearest - resistance))
        {
            nearest = E24_VALUES[i];
        }
    }
    return nearest;
}

// Função para determinar as cores das faixas
void get_color_bands(int resistance, int *band1, int *band2, int *multiplier)
{
    // Garante que a resistência está dentro de um intervalo válido
    if (resistance <= 0)
    {
        *band1 = *band2 = *multiplier = -1;
        return;
    }

    int digits = resistance;
    int pow10 = 0;

    // Reduz até que só restem dois dígitos significativos
    while (digits >= 100)
    {
        digits /= 10;
        pow10++;
    }

    *band1 = digits / 10; // primeiro dígito
    *band2 = digits % 10; // segundo dígito
    *multiplier = pow10;  // potência de 10 (ex: 10^3 → laranja)
}

// Trecho para modo BOOTSEL com botão B
void gpio_irq_handler(uint gpio, uint32_t events)
{
    reset_usb_boot(0, 0);
}

void draw_resistor_bands(int band1, int band2, int multiplier, PIO pio, uint sm)
{
    // Mapeia as cores para valores RGB
    double color_map[10][3] = {
        {0.001, 0.001, 0.001},  // Preto
        {1, 0.125, 0.0}, // Marrom
        {1.0, 0.0, 0.0},  // Vermelho
        {1.0, 0.25, 0.0},  // Laranja
        {1.0, 1.0, 0.0},  // Amarelo
        {0.0, 1.0, 0.0},  // Verde
        {0.0, 0.0, 1.0},  // Azul
        {0.5, 0.0, 0.5},  // Violeta
        {0.5, 0.5, 0.5},  // Cinza
        {1.0, 1.0, 1.0}   // Branco
    };

    // Matriz 5x5 representada como um array de 25 pixels
    double desenho[NUM_PIXELS] = {0};

    // Define as colunas centrais para as faixas e o multiplicador
    for (int i = 0; i < 5; i++)
    {
        desenho[i * 5 + 1] = 0.1; // Coluna 2 para a primeira faixa
        desenho[i * 5 + 2] = 0.1; // Coluna 3 para a segunda faixa
        desenho[i * 5 + 3] = 0.1; // Coluna 4 para o multiplicador
    }

    pio_drawn(desenho, 0, pio, sm,
              color_map[band1][0], color_map[band1][1], color_map[band1][2],                 // Cor da coluna 2
              color_map[band2][0], color_map[band2][1], color_map[band2][2],                 // Cor da coluna 3
              color_map[multiplier][0], color_map[multiplier][1], color_map[multiplier][2]); // Cor da coluna 4
}