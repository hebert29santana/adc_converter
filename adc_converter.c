/*
    Hebert Costa Vaz Santana
    TIC370101235
    Grupo 7
*/

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "display.h"

// Definições dos pinos
#define BOTAO_A 5
#define JOYSTICK_X 26
#define JOYSTICK_Y 27
#define JOYSTICK_BOTAO 22
#define LED_VERDE 11
#define LED_AZUL 12
#define LED_VERMELHO 13
#define DURACAO_MENSAGEM 2000 

// Variáveis globais
volatile bool green_led_state = false;
uint32_t debounce_time = 0;
volatile uint8_t border_style = 0;
volatile bool pwm_leds_enabled = true;
uint32_t button_a_debounce_time = 0;
volatile bool show_led_message = false;
volatile uint32_t message_start_time = 0;
volatile bool button_a_prev_state = true;

// Função de debouncing para botões
bool debounce(uint32_t *last_time)
{
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    if (current_time - *last_time > 500)
    { 
        *last_time = current_time;
        return true;
    }
    return false;
}

// Função de callback para interrupções de GPIO
void gpio_callback(uint gpio, uint32_t events) {
    if (gpio == JOYSTICK_BOTAO) {
        if (debounce(&debounce_time)) {
            green_led_state = !green_led_state;
            gpio_put(LED_VERDE, green_led_state);
            border_style = (border_style + 1) % 3;
        }
    }
    else if (gpio == BOTAO_A) {
        bool current_state = gpio_get(BOTAO_A);
        
        // Fica testando se o botão foi pressionado para conseguir exibir também a mensagem no SERIAL MONITOR
        while(!current_state && debounce(&button_a_debounce_time))
        {
            pwm_leds_enabled = !pwm_leds_enabled;
            
            if (!pwm_leds_enabled) {
                printf("LEDs Estão Desligados \n");
                pwm_set_chan_level(pwm_gpio_to_slice_num(LED_AZUL), PWM_CHAN_A, 0);
                pwm_set_chan_level(pwm_gpio_to_slice_num(LED_VERMELHO), PWM_CHAN_B, 0);
            }
            else {
                printf("LEDs Estão Ligados \n");
            }    
            show_led_message = true;
            message_start_time = to_ms_since_boot(get_absolute_time());
        }
    }
}

// Função para inicializar o ADC
void init_adc()
{
    adc_init();
    adc_gpio_init(JOYSTICK_X);
    adc_gpio_init(JOYSTICK_Y);
}

// Função para inicializar os LEDs
void init_leds()
{
    gpio_init(LED_VERDE);
    gpio_set_dir(LED_VERDE, GPIO_OUT);
    gpio_put(LED_VERDE, false);

    // Inicialização do LED azul
    gpio_set_function(LED_AZUL, GPIO_FUNC_PWM);
    uint slice_num_blue = pwm_gpio_to_slice_num(LED_AZUL);
    pwm_set_wrap(slice_num_blue, 4095);
    pwm_set_chan_level(slice_num_blue, PWM_CHAN_A, 0);
    pwm_set_enabled(slice_num_blue, true);

    // Inicialização do LED vermelho
    gpio_set_function(LED_VERMELHO, GPIO_FUNC_PWM);
    uint slice_num_red = pwm_gpio_to_slice_num(LED_VERMELHO);
    pwm_set_wrap(slice_num_red, 4095);                
    pwm_set_chan_level(slice_num_red, PWM_CHAN_B, 0); 
    pwm_set_enabled(slice_num_red, true);             
}

// Função para inicializar o botão do joystick
void init_joystick_button()
{
    gpio_init(JOYSTICK_BOTAO);
    gpio_set_dir(JOYSTICK_BOTAO, GPIO_IN);
    gpio_pull_up(JOYSTICK_BOTAO);
    gpio_set_irq_enabled_with_callback(JOYSTICK_BOTAO, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
}

// Função para inicializar o Botão A
void init_button_a()
{
    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);
    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &gpio_callback); 
}

// Função para ajustar o brilho do LED azul
void adjust_blue_led(uint16_t y_value)
{
    if (!pwm_leds_enabled)
        return; // Sai da função se os LEDs estiverem desabilitados

    uint16_t brightness = 0;
    if (y_value < 1980 || y_value > 2100)
    {
        brightness = (y_value > 2048) ? (y_value - 2048) * 2 : (2048 - y_value) * 2;
    }
    pwm_set_chan_level(pwm_gpio_to_slice_num(LED_AZUL), PWM_CHAN_A, brightness);
}

// Função para ajustar o brilho do LED vermelho
void adjust_red_led(uint16_t x_value)
{
    if (!pwm_leds_enabled)
        return; // Sai da função se os LEDs estiverem desabilitados

    uint16_t brightness = 0;
    if (x_value < 1980 || x_value > 2100)
    {
        brightness = (x_value > 2048) ? (x_value - 2048) * 2 : (2048 - x_value) * 2;
        if (brightness > 4095)
        {
            brightness = 4095;
        }
    }
    pwm_set_chan_level(pwm_gpio_to_slice_num(LED_VERMELHO), PWM_CHAN_B, brightness);
}

// Função para mostrar mensagem no display
void show_led_status(ssd1306_t *ssd) {
    ssd1306_fill(ssd, false);  // Limpa o display
    
    // Posiciona o texto no centro do display
    if (pwm_leds_enabled) {
        ssd1306_draw_string(ssd, "LED LIGADO", 20, 24);
    } else {
        ssd1306_draw_string(ssd, "LED DESLIGADO", 8, 24);
    }
    
    ssd1306_send_data(ssd);
}

// Função para mover o quadrado no display e alterar a borda
void move_square(ssd1306_t *ssd, uint16_t x_value, uint16_t y_value) {
    // Verifica se deve mostrar a mensagem de status dos LEDs
    if (show_led_message) {
        uint32_t current_time = to_ms_since_boot(get_absolute_time());
        if (current_time - message_start_time < DURACAO_MENSAGEM) {
            show_led_status(ssd);
            return;  // Sai da função sem desenhar o quadrado
        } else {
            show_led_message = false;  // Desativa a mensagem após o tempo
        }
    }

    static uint8_t square_x = SSD1306_WIDTH / 2 - 4;
    static uint8_t square_y = SSD1306_HEIGHT / 2 - 4;
    
    square_x = (y_value * SSD1306_WIDTH) / 4095;
    square_y = SSD1306_HEIGHT - ((x_value * SSD1306_HEIGHT) / 4095);
    
    // Condições para o Quadrado não passar dos limites do LCD
    if (square_x < 0) square_x = 0;
    if (square_x > SSD1306_WIDTH - 8) square_x = SSD1306_WIDTH - 8;
    if (square_y < 0) square_y = 0;
    if (square_y > SSD1306_HEIGHT - 8) square_y = SSD1306_HEIGHT - 8;
    
    ssd1306_fill(ssd, false);
    ssd1306_rect(ssd, square_y, square_x, 8, 8, true, true);

    switch (border_style) {
    case 0:
        // Desenha bordas horizontais
        ssd1306_line(ssd, 0, 0, SSD1306_WIDTH - 1, 0, true);
        ssd1306_line(ssd, 0, SSD1306_HEIGHT - 1, SSD1306_WIDTH - 1, SSD1306_HEIGHT - 1, true);
        break;
    case 1:
        // Desenha bordas verticais
        ssd1306_line(ssd, 0, 0, 0, SSD1306_HEIGHT - 1, true);
        ssd1306_line(ssd, SSD1306_WIDTH - 1, 0, SSD1306_WIDTH - 1, SSD1306_HEIGHT - 1, true);
        break;
    case 2:
        // Desenha um retângulo interno como borda
        ssd1306_rect(ssd, 2, 2, SSD1306_WIDTH - 4, SSD1306_HEIGHT - 4, true, false);
        break;
    default:
        break;
    }
    ssd1306_send_data(ssd);
}

// Função Principal
int main()
{
    stdio_init_all();

    // Inicializa o display
    ssd1306_t ssd;
    init_display(&ssd);

    // Inicializa o ADC
    init_adc();

    // Inicializa os LEDs
    init_leds();

    // Inicializa o botão do joystick
    init_joystick_button();

    // Inicializa o Botão A
    init_button_a();

    // Exibe a mensagem "MOVA O JOYSTICK" no display
    ssd1306_fill(&ssd, false);  
    ssd1306_draw_string(&ssd, "MEXE O", 35, 16);
    ssd1306_draw_string(&ssd, "JOYSTICK", 20, 32);
    printf("Mover o Joystick");
    ssd1306_send_data(&ssd);

    // Aguarda até que o usuário mova o joystick
    uint16_t x_center = 2048, y_center = 2048; // Valor típico do ADC em repouso
    bool joystick_moved = false;
    
    while (!joystick_moved) {
        adc_select_input(0);
        uint16_t x_value = adc_read();
        adc_select_input(1);
        uint16_t y_value = adc_read();

        // Verifica se o joystick se moveu para além de um limite (~150 de variação)
        if (abs(x_value - x_center) > 150 || abs(y_value - y_center) > 150) {
            joystick_moved = true;
        }
    }

    // Quando o joystick for movido, inicia a interface normal
    while (true)
    {
        adc_select_input(0);
        uint16_t x_value = adc_read();
        adc_select_input(1);
        uint16_t y_value = adc_read();

        // Ajusta o brilho dos LEDs
        adjust_blue_led(y_value);
        adjust_red_led(x_value);

        // Move o quadrado no display
        move_square(&ssd, x_value, y_value);

        sleep_ms(10);
    }

    return 0;
}