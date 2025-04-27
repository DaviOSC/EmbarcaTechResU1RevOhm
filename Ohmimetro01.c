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
#define ADC_PIN 28
#define Botao_A 5
#define botaoB 6
#define OUT_PIN 7
#define NUM_PIXELS 25

#define DEBOUNCE_TIME_MS 300 // Tempo de debounce em ms
absolute_time_t last_interrupt_time = {0};

int R_conhecido = 10000;   // Resistor de 10k ohm
float R_x = 0.0;           // Resistor desconhecido
float ADC_VREF = 3.31;     // Tensão de referência do ADC
int ADC_RESOLUTION = 4095; // Resolução do ADC (12 bits)

// Valores comerciais padrão E24 (5%)
const int E24_VALUES[] = {
    510, 560, 620, 680, 750, 820, 910,
    1000, 1100, 1200, 1300, 1500, 1600, 1800, 2000, 2200, 2400, 2700, 3000, 3300, 3600, 3900, 4300, 4700,
    5100, 5600, 6200, 6800, 7500, 8200, 9100,
    10000, 11000, 12000, 13000, 15000, 16000, 18000, 20000, 22000, 24000, 27000, 30000, 33000, 36000, 39000, 43000, 47000,
    51000, 56000, 62000, 68000, 75000, 82000, 91000, 100000};

bool is_four_band_mode = true; // Variável para controlar o número de faixas
// Cores correspondentes às faixas do resistor
const char *COLOR_CODES[] = {"Preto", "Marrom", "Vermelho", "Laranja", "Amarelo", "Verde", "Azul", "Violeta", "Cinza", "Branco"};

uint32_t matrix_rgb(double r, double g, double b);
void pio_drawn(double desenho[][3], uint32_t valor_led, PIO pio, uint sm);
int find_nearest_e24(float resistance);
void get_color_bands(int resistance, int *band1, int *band2, int *band3, int *multiplier);
void draw_resistor_bands(int band1, int band2, int multiplier, PIO pio, uint sm);
void draw_resistor_bands_5(int band1, int band2, int band3, int multiplier, PIO pio, uint sm);
void gpio_irq_handler(uint gpio, uint32_t events);

int main()
{
    // Para ser utilizado o modo BOOTSEL com botão B
    stdio_init_all();
    gpio_init(botaoB);
    gpio_set_dir(botaoB, GPIO_IN);
    gpio_pull_up(botaoB);
    gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_init(Botao_A);
    gpio_set_dir(Botao_A, GPIO_IN);
    gpio_pull_up(Botao_A);
    gpio_set_irq_enabled_with_callback(Botao_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    // Aqui termina o trecho para modo BOOTSEL com botão B
    PIO pio = pio0;
    uint32_t valor_led;

    // Configurações da PIO
    uint offset = pio_add_program(pio, &pio_matrix_program);
    uint sm = pio_claim_unused_sm(pio, true);
    pio_matrix_program_init(pio, sm, offset, OUT_PIN);

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
    char str_band1[10], str_band2[10], str_band3[10], str_multiplier[10];
    char title[20];
    while (true)
    {
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
        printf("R_x: %.2f\n", R_x);

        int band1, band2, band3, multiplier;
        get_color_bands(nearest_resistance, &band1, &band2, &band3, &multiplier);

        // Exibe os textos no display
        sprintf(title, "Ohmimetro %s", is_four_band_mode ? "4F" : "5F");
        sprintf(str_band1, "1:%s", COLOR_CODES[band1]);
        sprintf(str_multiplier, "M:%s", COLOR_CODES[multiplier]);
        sprintf(str_resistance, "Res:%d", nearest_resistance);

        int cor = 1;
        ssd1306_fill(&ssd, !cor); // Limpa o display
        ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor);
        ssd1306_line(&ssd, 3, 16, 123, 16, cor);
        ssd1306_line(&ssd, 3, 27, 123, 27, cor);
        ssd1306_line(&ssd, 3, 38, 123, 38, cor);
        ssd1306_line(&ssd, 3, 49, 123, 49, cor);

        ssd1306_draw_string(&ssd, title, 17, 6);     // Exibe o título
        ssd1306_draw_string(&ssd, str_band1, 6, 18); // Exibe o texto da primeira faixa

        if (is_four_band_mode)
        {
            sprintf(str_band2, "2:%s", COLOR_CODES[band2]); // Formata e o texto da segunda faixa
            ssd1306_draw_string(&ssd, str_band2, 6, 29);
            draw_resistor_bands(band1, band2, multiplier, pio, sm);
        }
        else
        {
            sprintf(str_band2, "2:%.4s", COLOR_CODES[band2]); // Formata e exibe o texto da segunda faixa
            ssd1306_draw_string(&ssd, str_band2, 6, 29);
            sprintf(str_band3, "3:%.4s", COLOR_CODES[band3]); // Formata e exibe o texto da terceira faixa
            ssd1306_draw_string(&ssd, str_band3, 65, 29);
            draw_resistor_bands_5(band1, band2, band3, multiplier, pio, sm);
        }

        ssd1306_draw_string(&ssd, str_multiplier, 6, 40); // Exibe o texto do multiplicador
        ssd1306_draw_string(&ssd, str_resistance, 6, 52); // Exibe o valor da resistência
        ssd1306_send_data(&ssd);                          // Atualiza o display

        sleep_ms(500);
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
void pio_drawn(double desenho[][3], uint32_t valor_led, PIO pio, uint sm)
{
    for (int16_t i = 0; i < NUM_PIXELS; i++)
    {
        double r = desenho[i][0] * 0.1; // Reduz a intensidade da cor
        double g = desenho[i][1] * 0.1;
        double b = desenho[i][2] * 0.1;

        valor_led = matrix_rgb(r, g, b);
        pio_sm_put_blocking(pio, sm, valor_led);
    }
}

// Função para encontrar o valor E24 mais próximo
int find_nearest_e24(float resistance)
{
    int nearest = E24_VALUES[0];
    for (int i = 1; i < sizeof(E24_VALUES) / sizeof(E24_VALUES[0]); i++)
    {
        // Verifica se o valor atual é mais próximo do que o valor mais próximo encontrado até agora
        if (abs(E24_VALUES[i] - resistance) < abs(nearest - resistance))
        {
            nearest = E24_VALUES[i];
        }
    }
    return nearest;
}

// Função para determinar as cores das faixas
void get_color_bands(int resistance, int *band1, int *band2, int *band3, int *multiplier)
{
    // Garante que a resistência está dentro de um intervalo válido
    if (resistance <= 0)
    {
        *band1 = *band2 = *multiplier = -1;
        return;
    }

    int digits = resistance;
    int pow10 = 0;

    if (is_four_band_mode)
    {
        while (digits >= 100)
        {
            digits /= 10;
            pow10++;
        }
        *band1 = digits / 10; // Primeiro dígito
        *band2 = digits % 10; // Segundo dígito
        *multiplier = pow10;  // Potência de 10
    }
    else
    {
        while (digits >= 1000)
        {
            digits /= 10;
            pow10++;
        }
        *band1 = digits / 100;       // Primeiro dígito
        *band2 = (digits / 10) % 10; // Segundo dígito
        *band3 = digits % 10;        // Terceiro dígito
        *multiplier = pow10;         // Potência de 10 (multiplicador)
    }
}

// Trecho para modo BOOTSEL com botão B
void gpio_irq_handler(uint gpio, uint32_t events)
{
    absolute_time_t current_time = get_absolute_time();

    if (absolute_time_diff_us(last_interrupt_time, current_time) < DEBOUNCE_TIME_MS * 1000)
    {
        return; // Ignora a interrupção se estiver dentro do tempo de debounce
    }
    else
    {
        last_interrupt_time = current_time;
    }

    if (gpio == Botao_A)
    {
        is_four_band_mode = !is_four_band_mode; // Alterna entre os modos de 4 e 5 faixas
    }
    if (gpio == botaoB)
    {
        reset_usb_boot(0, 0);
    }
}

void draw_resistor_bands(int band1, int band2, int multiplier, PIO pio, uint sm)
{
    // Cores em valores RGB
    double color_map[10][3] = {
        {0, 0, 0},        // Preto
        {1, 0.125, 0.0},  // Marrom
        {1.0, 0.0, 0.0},  // Vermelho
        {1.0, 0.25, 0.0}, // Laranja
        {1.0, 1.0, 0.0},  // Amarelo
        {0.0, 1.0, 0.0},  // Verde
        {0.0, 0.0, 1.0},  // Azul
        {0.5, 0.0, 0.5},  // Violeta
        {0.5, 0.5, 0.5},  // Cinza
        {1.0, 1.0, 1.0}   // Branco
    };

    // Matriz 5x5 representada como um array de 25 pixels e 3 cores (RGB) 
    double desenho[NUM_PIXELS][3] = {0};

    // Define as colunas centrais para as faixas e o multiplicador
    for (int i = 0; i < 5; i++)
    {
        // Se a linha for par, desenha as faixas e o multiplicador da direita para a esquerda
        if (i % 2 == 0)
        {
            // Faixa 1 (coluna 4 invertida)
            desenho[i * 5 + 3][0] = color_map[band1][0]; // R
            desenho[i * 5 + 3][1] = color_map[band1][1]; // G
            desenho[i * 5 + 3][2] = color_map[band1][2]; // B

            // Faixa 2 (coluna 3)
            desenho[i * 5 + 2][0] = color_map[band2][0]; // R
            desenho[i * 5 + 2][1] = color_map[band2][1]; // G
            desenho[i * 5 + 2][2] = color_map[band2][2]; // B

            // Multiplicador (coluna 2 invertida)
            desenho[i * 5 + 1][0] = color_map[multiplier][0]; // R
            desenho[i * 5 + 1][1] = color_map[multiplier][1]; // G
            desenho[i * 5 + 1][2] = color_map[multiplier][2]; // B
        }
        // Se a linha for ímpar, desenha as faixas e o multiplicador da esquerda para a direita
        else
        {
            // Faixa 1 (coluna 2)
            desenho[i * 5 + 1][0] = color_map[band1][0]; // R
            desenho[i * 5 + 1][1] = color_map[band1][1]; // G
            desenho[i * 5 + 1][2] = color_map[band1][2]; // B

            // Faixa 2 (coluna 3)
            desenho[i * 5 + 2][0] = color_map[band2][0]; // R
            desenho[i * 5 + 2][1] = color_map[band2][1]; // G
            desenho[i * 5 + 2][2] = color_map[band2][2]; // B

            // Multiplicador (coluna 4)
            desenho[i * 5 + 3][0] = color_map[multiplier][0]; // R
            desenho[i * 5 + 3][1] = color_map[multiplier][1]; // G
            desenho[i * 5 + 3][2] = color_map[multiplier][2]; // B
        }
    }
    
    // Chama a função pio_drawn para enviar os dados para a pio
    pio_drawn(desenho, 0, pio, sm);
}

void draw_resistor_bands_5(int band1, int band2, int band3, int multiplier, PIO pio, uint sm)
{
    // Cores em valores RGB
    double color_map[10][3] = {
        {0, 0, 0},        // Preto
        {1, 0.125, 0.0},  // Marrom
        {1.0, 0.0, 0.0},  // Vermelho
        {1.0, 0.25, 0.0}, // Laranja
        {1.0, 1.0, 0.0},  // Amarelo
        {0.0, 1.0, 0.0},  // Verde
        {0.0, 0.0, 1.0},  // Azul
        {0.5, 0.0, 0.5},  // Violeta
        {0.5, 0.5, 0.5},  // Cinza
        {1.0, 1.0, 1.0}   // Branco
    };

    // Matriz 5x5 representada como um array de 25 pixels e 3 cores (RGB) 
    double desenho[NUM_PIXELS][3] = {0};

    // Define as colunas centrais para as faixas e o multiplicador
    for (int i = 0; i < 5; i++)
    {
        // Se a linha for par, desenha as faixas e o multiplicador da direita para a esquerda
        if (i % 2 == 0)
        {
            // Faixa 1 (coluna 4 invertida)
            desenho[i * 5 + 3][0] = color_map[band1][0]; // R
            desenho[i * 5 + 3][1] = color_map[band1][1]; // G
            desenho[i * 5 + 3][2] = color_map[band1][2]; // B

            // Faixa 2 (coluna 3 invertida)
            desenho[i * 5 + 2][0] = color_map[band2][0]; // R
            desenho[i * 5 + 2][1] = color_map[band2][1]; // G
            desenho[i * 5 + 2][2] = color_map[band2][2]; // B
            
            // Faixa 3 (coluna 2 invertida)
            desenho[i * 5 + 1][0] = color_map[band3][0]; // R
            desenho[i * 5 + 1][1] = color_map[band3][1]; // G
            desenho[i * 5 + 1][2] = color_map[band3][2]; // B
            
            // Multiplicador (coluna 1 invertida)
            desenho[i * 5 + 0][0] = color_map[multiplier][0]; // R
            desenho[i * 5 + 0][1] = color_map[multiplier][1]; // G
            desenho[i * 5 + 0][2] = color_map[multiplier][2]; // B
        }
        else
        {
            // Faixa 1 (coluna 2)
            desenho[i * 5 + 1][0] = color_map[band1][0]; // R
            desenho[i * 5 + 1][1] = color_map[band1][1]; // G
            desenho[i * 5 + 1][2] = color_map[band1][2]; // B

            // Faixa 2 (coluna 3)
            desenho[i * 5 + 2][0] = color_map[band2][0]; // R
            desenho[i * 5 + 2][1] = color_map[band2][1]; // G
            desenho[i * 5 + 2][2] = color_map[band2][2]; // B

            // Faixa 3 (coluna 4)
            desenho[i * 5 + 3][0] = color_map[band3][0]; // R
            desenho[i * 5 + 3][1] = color_map[band3][1]; // G
            desenho[i * 5 + 3][2] = color_map[band3][2]; // B

            // Multiplicador (coluna 5)
            desenho[i * 5 + 4][0] = color_map[multiplier][0]; // R
            desenho[i * 5 + 4][1] = color_map[multiplier][1]; // G
            desenho[i * 5 + 4][2] = color_map[multiplier][2]; // B
        }
    }

    // Chama a função pio_drawn para enviar os dados para a pio
    pio_drawn(desenho, 0, pio, sm);
}