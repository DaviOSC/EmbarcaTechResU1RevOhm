#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define ADC_PIN 28 // GPIO para o voltímetro
#define Botao_A 5  // GPIO para botão A

int R_conhecido = 4820;   // Resistor de 10k ohm
float R_x = 0.0;           // Resistor desconhecido
float ADC_VREF = 3.31;     // Tensão de referência do ADC
int ADC_RESOLUTION = 4095; // Resolução do ADC (12 bits)

// Valores comerciais padrão E24 (5%)
const int E24_VALUES[] = {510, 560, 620, 680, 750, 820, 910, 1000, 1100, 1200, 1300, 1500, 1600, 1800, 2000, 2200, 2400, 2700, 3000, 3300, 3600, 3900, 4300, 4700, 5100, 5600, 6200, 6800, 7500, 8200, 9100, 10000, 11000, 12000, 13000, 15000, 16000, 18000, 20000, 22000, 24000, 27000, 30000, 33000, 36000, 39000, 43000, 47000, 51000, 56000, 62000, 68000, 75000, 82000, 91000, 100000};

// Cores correspondentes às faixas do resistor
const char *COLOR_CODES[] = {"Preto", "Marrom", "Vermelho", "Laranja", "Amarelo", "Verde", "Azul", "Violeta", "Cinza", "Branco"};

// Função para encontrar o valor E24 mais próximo
int find_nearest_e24(float resistance) {
    int nearest = E24_VALUES[0];
    for (int i = 1; i < sizeof(E24_VALUES) / sizeof(E24_VALUES[0]); i++) {
        if (abs(E24_VALUES[i] - resistance) < abs(nearest - resistance)) {
            nearest = E24_VALUES[i];
        }
    }
    return nearest;
}

// Função para determinar as cores das faixas
#include <stdio.h>
#include <math.h>

void get_color_bands(int resistance, int *band1, int *band2, int *multiplier) {
    // Garante que a resistência está dentro de um intervalo válido
    if (resistance <= 0) {
        *band1 = *band2 = *multiplier = -1;
        return;
    }

    int digits = resistance;
    int pow10 = 0;

    // Reduz até que só restem dois dígitos significativos
    while (digits >= 100) {
        digits /= 10;
        pow10++;
    }

    *band1 = digits / 10;      // primeiro dígito
    *band2 = digits % 10;      // segundo dígito
    *multiplier = pow10;       // potência de 10 (ex: 10^3 → laranja)
}

// Trecho para modo BOOTSEL com botão B
#include "pico/bootrom.h"
#define botaoB 6
void gpio_irq_handler(uint gpio, uint32_t events)
{
  reset_usb_boot(0, 0);
}

int main()
{
  // Para ser utilizado o modo BOOTSEL com botão B
  stdio_init_all();
  gpio_init(botaoB);
  gpio_set_dir(botaoB, GPIO_IN);
  gpio_pull_up(botaoB);
  gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
  // Aqui termina o trecho para modo BOOTSEL com botão B

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

    while (true) {
        adc_select_input(2);

        float soma = 0.0f;
        for (int i = 0; i < 500; i++) {
            soma += adc_read();
            sleep_ms(1);
        }
        float media = soma / 500.0f;

        R_x = (R_conhecido * media) / (ADC_RESOLUTION - media);
        printf("R_x: %.2f\n", R_x); // Depuração: Verifique o valor de R_x
        int nearest_resistance = find_nearest_e24(R_x);
        printf("Resistência mais próxima: %d\n", nearest_resistance); // Depuração: Verifique o valor de resistência mais próxima

        int band1, band2, multiplier;
get_color_bands(nearest_resistance, &band1, &band2, &multiplier);

// Depuração: Verifique os valores calculados
printf("Debug - band1: %d, band2: %d, multiplier: %d\n", band1, band2, multiplier);

// Verifique se os índices estão dentro do intervalo válido
if (band1 >= 0 && band1 <= 9) {
    sprintf(str_band1, "%s", COLOR_CODES[band1]);
} else {
    sprintf(str_band1, "Erro"); // Valor inválido
}

if (band2 >= 0 && band2 <= 9) {
    sprintf(str_band2, "%s", COLOR_CODES[band2]);
} else {
    sprintf(str_band2, "Erro"); // Valor inválido
}

if (multiplier >= 0 && multiplier <= 9) {
    sprintf(str_multiplier, "%s", COLOR_CODES[multiplier]);
} else {
    sprintf(str_multiplier, "Erro"); // Valor inválido
}

// Depuração: Verifique as strings formatadas
printf("Debug - str_band1: %s, str_band2: %s, str_multiplier: %s\n", str_band1, str_band2, str_multiplier);
        printf("faixa1: %s\n", COLOR_CODES[band1]);
        printf("faixa2: %s\n", COLOR_CODES[band2]);
        sprintf(str_resistance, "%d", nearest_resistance);
        sprintf(str_band1, "%s", COLOR_CODES[band1]);
        sprintf(str_band2, "%s", COLOR_CODES[band2]);
        sprintf(str_multiplier, "%s", COLOR_CODES[multiplier]);

        int cor = 1;
        // cor = !cor;
        // Atualiza o conteúdo do display com animações
        ssd1306_fill(&ssd, !cor);                          // Limpa o display
        ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor);      // Desenha um retângulo
        ssd1306_line(&ssd, 3, 16, 123, 16, cor);           // Desenha uma linha
        ssd1306_line(&ssd, 3, 27, 123, 27, cor);  
        ssd1306_line(&ssd, 3, 38, 123, 38, cor);
        ssd1306_line(&ssd, 3, 49, 123, 49, cor);
        ssd1306_draw_string(&ssd, "Ohmimetro", 30, 6);   // Exibe "Ohmimetro"
        ssd1306_draw_string(&ssd, "Fx1:", 6, 18);       // Exibe "Faixa 1:"
        ssd1306_draw_string(&ssd, str_band1, 55, 18);       // Exibe o texto da primeira faixa de cor
        ssd1306_draw_string(&ssd, "Fx2:", 6, 29);      // Exibe "Faixa 2:"
        ssd1306_draw_string(&ssd, str_band2, 55, 29);      // Exibe o texto da segunda faixa de cor
        ssd1306_draw_string(&ssd, "Mult.:", 6, 40);// Exibe "Multiplicador:"
        ssd1306_draw_string(&ssd, str_multiplier, 55, 40); // Exibe o texto do multiplicador
        ssd1306_draw_string(&ssd, "Resi.:", 6, 52);  // Exibe "Resistencia:"
        ssd1306_draw_string(&ssd, str_resistance, 55, 52); // Exibe o valor da resistência
        ssd1306_send_data(&ssd);                           // Atualiza o display
        sleep_ms(700);                                     // Aguarda 700ms
    }
}