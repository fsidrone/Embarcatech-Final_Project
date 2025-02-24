/* ------- Projeto Final - Embarcatech--------
  ----- Autor: Felipe Sidrone da Silva Dionisio
*/

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "neopixel.c"
#include "hardware/pwm.h" // Biblioteca para controle de PWM no RP2040
#include "time.h"
#include "pico/binary_info.h"
#include "inc/ssd1306.h"
#include "hardware/i2c.h"

//Pinos I2C do Display
const uint I2C_SDA = 14;
const uint I2C_SCL = 15;

// GPIO conectado ao Botão A e B
#define BUTTON_A 5    
#define BUTTON_B 6

// Pino e número de LEDs da matriz de LEDs.
#define LED_PIN 7
#define LED_COUNT 25

// Configuração do pino do buzzer
#define BUZZER_PIN 21

// Parametro para escolher notas aleatórias
#define NUM_NOTAS (sizeof(notas) / sizeof(notas[0]))

// Frequências das notas C8, D8, E8, F8, G8
typedef struct {
    char nota[3];  // Nome da nota (C8, D8, E8, etc.)
    float frequencia;  // Frequência correspondente
} Nota;

Nota notas[] = {
    {"C8", 4186.01},
    {"D8", 4698.64},
    {"E8", 5274.04},
    {"F8", 5587.65},
    {"G8", 6271.93}
};

struct render_area frame_area = {
    start_column : 0,
    end_column : ssd1306_width - 1,
    start_page : 0,
    end_page : ssd1306_n_pages - 1
};

// Definição dos pinos usados para o joystick e LEDs
const int VRX = 26;          // Pino de leitura do eixo X do joystick (conectado ao ADC)
const int VRY = 27;          // Pino de leitura do eixo Y do joystick (conectado ao ADC)
const int ADC_CHANNEL_0 = 0; // Canal ADC para o eixo X do joystick
const int ADC_CHANNEL_1 = 1; // Canal ADC para o eixo Y do joystick
const int SW = 22;           // Pino de leitura do botão do joystick

uint8_t option;

Nota escolher_nota_aleatoria();
void pwm_init_buzzer(uint pin, float frequency);
void beep(uint pin, uint duration_ms, float frequency);
void setup_joystick();
void joystick_read_axis(uint16_t *vrx_value, uint16_t *vry_value);
int getIndex(int x, int y);
void print_matriz(uint8_t option);
void setup_display();
void print_display(char *text[]);
void metronomo(uint16_t *vry_value);

volatile bool lock = false;

//Função de interrupção
bool repeating_timer_callback(struct repeating_timer *t) {
    static absolute_time_t last_press_time = 0;
    static bool button_last_state_A = false;
    static bool button_last_state_B = false;

    // Lê o estado do botão A
    bool button_A_pressed = !gpio_get(BUTTON_A); // Botão A pressionado gera um nível baixo

    // Lê o estado do botão B
    bool button_B_pressed = !gpio_get(BUTTON_B); // Botão B pressionado gera um nível baixo

    // Lógica do botão A (alterar "lock" somente com o botão A)
    if (button_A_pressed && !button_last_state_A && absolute_time_diff_us(last_press_time, get_absolute_time()) > 300000) {
        last_press_time = get_absolute_time();
        button_last_state_A = true;
        lock = !lock;
        printf("Botão A pressionado: lock = %s\n", lock ? "true" : "false");
    } else if (!button_A_pressed) {
        button_last_state_A = false;
    }

    // Lógica para o botão B (aqui você pode definir outras ações para o botão B, por exemplo)
    if (button_B_pressed && !button_last_state_B && absolute_time_diff_us(last_press_time, get_absolute_time()) > 300000) {
        last_press_time = get_absolute_time();
        button_last_state_B = true;
        printf("Botão B pressionado.\n");
    } else if (!button_B_pressed) {
        button_last_state_B = false;
    }

    return true; // Retorna true para continuar o temporizador de repetição
}

// Função principal
int main() {
    // Inicializa a porta serial para saída de dados
    stdio_init_all();                                

    // Chama a função de configuração
    setup_joystick();  
    setup_display();

    srand(time(NULL));  // Inicializa o gerador de números aleatórios

    // Configuração do GPIO do Botão A como entrada com pull-up interno
    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);

    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);

    npInit(LED_PIN, LED_COUNT);
    npClear();
    
    uint16_t vrx_value, vry_value, sw_value; // Variáveis para armazenar os valores do joystick (eixos X e Y) e botão

    //Configura interrupção 
    struct repeating_timer timer;
    add_repeating_timer_ms(100, repeating_timer_callback, NULL, &timer);

    while (1) {
        for (int i = 0; i < 5; i++) {
            print_matriz(i);
            sleep_ms(400);
            print_matriz(i);
            beep(BUZZER_PIN, 800, notas[i].frequencia);
        }
        sleep_ms(1500);
     
        Nota nota_tocada = escolher_nota_aleatoria();
        
        // Tocar a nota escolhida
        printf("Tocando a nota: %s...\n", nota_tocada.nota);
        beep(BUZZER_PIN, 2000, nota_tocada.frequencia);
        sleep_ms(500);

        char *mensagem1[] = {
            "             ",
            "             ",
            "    Selecione   ",
            "     a nota    ",
            " e pressione B"
        };
        print_display(mensagem1);
        
        while (gpio_get(BUTTON_B)) {
            sleep_ms(100);
            joystick_read_axis(&vrx_value, &vry_value); // Lê os valores dos eixos do joystick

            // Zona Centro (opção 1)
            if ((vrx_value >= 1530 && vrx_value <= 2400) && (vry_value >= 1580 && vry_value <= 2800)) {
                printf("C \n");
                option = 0;
            }
            // Zona 2 (para a esquerda)
            else if ((vrx_value >= 2800) && (vry_value >= 530 && vry_value <= 3680)) {
                printf("E\n");
                option = 2;
            }
            // Zona 3 (para a direita)
            else if ((vrx_value >= 1190 && vrx_value <= 2900) && (vry_value <= 1580)) {
                printf("D \n");
                option = 1;
            }
            // Zona 4 (para cima)
            else if ((vrx_value <= 1530) && (vry_value >= 830 && vry_value <= 3500)) {
                printf("G \n");
                option = 4;
            }
            // Zona 5 (para baixo)
            else if ((vrx_value >= 1500 && vrx_value <= 3200) && (vry_value >= 2800)) {
                printf("F \n");
                option = 3;
            }
            // Se nenhum valor se encaixar, não há opção válida
            else {
                printf("Nenhuma opção válida selecionada.\n");
            }

            print_matriz(option);
        }
   
        sleep_ms(500);

        char *mensagem2[] = {
            "             ",
            "             ",
            "   Pressione B   ",
            "     para   ",
            "   recomecar"
        };
        print_display(mensagem2);

        while (gpio_get(BUTTON_B)) {
            if (notas[option].frequencia == nota_tocada.frequencia) {
                printf("Você acertou! A nota era %s.\n", nota_tocada.nota);
                sleep_ms(150);
                print_matriz(5);
            } else {
                printf("Você errou! A nota era %s, mas você escolheu %s.\n", nota_tocada.nota, notas[option].nota);
                sleep_ms(150);
                print_matriz(6);
            }
        }

        // Pequeno delay antes da próxima leitura
        npClear();
        sleep_ms(1000); // Espera 100 ms antes de repetir o ciclo
        if (lock == true) {
            npClear();
            npWrite();
            metronomo(&vry_value);
        }
    }
}

void setup_display() {
    // Inicialização do i2c
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Processo de inicialização completo do OLED SSD1306
    ssd1306_init();

    // Preparar área de renderização para o display (ssd1306_width pixels por ssd1306_n_pages páginas)
    char *text[] = {
        "             ",
        "             ",
        " TREINAMENTO   ",
        "  INICIADO   "
    };
    print_display(text);
}

void print_display(char *text[]) {
    calculate_render_area_buffer_length(&frame_area);

    // Zera o display inteiro
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, &frame_area);

    int y = 0;
    for (uint i = 0; i < 5; i++) {  // 5 é o número de linhas esperadas
        ssd1306_draw_string(ssd, 5, y, text[i]);
        y += 8;
    }

    render_on_display(ssd, &frame_area);
}

void pwm_init_buzzer(uint pin, float frequency) {
    // Configurar o pino como saída de PWM
    gpio_set_function(pin, GPIO_FUNC_PWM);

    // Obter o slice do PWM associado ao pino
    uint slice_num = pwm_gpio_to_slice_num(pin);

    // Configurar o PWM com frequência desejada
    pwm_config config = pwm_get_default_config();
    
    // Calcular o divisor de clock com base na frequência do buzzer
    pwm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (frequency * 4096)); // Divisor de clock
    pwm_init(slice_num, &config, true);

    // Iniciar o PWM no nível baixo
    pwm_set_gpio_level(pin, 0);
}

// Função para emitir um beep com duração especificada e frequência passada como parâmetro
void beep(uint pin, uint duration_ms, float frequency) {
    // Iniciar o PWM com a frequência especificada
    pwm_init_buzzer(pin, frequency);

    // Configurar o duty cycle para 50% (ativo)
    pwm_set_gpio_level(pin, 2048);

    // Temporização
    sleep_ms(duration_ms);

    // Desativar o sinal PWM (duty cycle 0)
    pwm_set_gpio_level(pin, 0);

    // Pausa entre os beeps
    sleep_ms(100); // Pausa de 100ms
}

// Função para configurar o joystick (pinos de leitura e ADC)
void setup_joystick() {
    // Inicializa o ADC e os pinos de entrada analógica
    adc_init();         // Inicializa o módulo ADC
    adc_gpio_init(VRX); // Configura o pino VRX (eixo X) para entrada ADC
    adc_gpio_init(VRY); // Configura o pino VRY (eixo Y) para entrada ADC

    // Inicializa o pino do botão do joystick
    gpio_init(SW);             // Inicializa o pino do botão
    gpio_set_dir(SW, GPIO_IN); // Configura o pino do botão como entrada
    gpio_pull_up(SW);          // Ativa o pull-up no pino do botão para evitar flutuações
}

// Função para ler os valores dos eixos do joystick (X e Y)
void joystick_read_axis(uint16_t *vrx_value, uint16_t *vry_value) {
    // Leitura do valor do eixo X do joystick
    adc_select_input(ADC_CHANNEL_0); // Seleciona o canal ADC para o eixo X
    sleep_us(2);                     // Pequeno delay para estabilidade
    *vrx_value = adc_read();         // Lê o valor do eixo X (0-4095)
    //printf("Eixo x: %d", *vrx_value);

    // Leitura do valor do eixo Y do joystick
    adc_select_input(ADC_CHANNEL_1); // Seleciona o canal ADC para o eixo Y
    sleep_us(10);                    // Pequeno delay para estabilidade
    *vry_value = adc_read();         // Lê o valor do eixo Y (0-4095)
    //printf("Eixo Y: %d", *vry_value);
}

Nota escolher_nota_aleatoria() {
    int index = rand() % NUM_NOTAS;  // Escolher aleatoriamente
    return notas[index];
}

// Função para converter a posição do matriz para uma posição do vetor.
int getIndex(int x, int y) {
    // Se a linha for par (0, 2, 4), percorremos da esquerda para a direita.
    // Se a linha for ímpar (1, 3), percorremos da direita para a esquerda.
    if (y % 2 == 0) {
        return 24 - (y * 5 + x); // Linha par (esquerda para direita).
    } else {
        return 24 - (y * 5 + (4 - x)); // Linha ímpar (direita para esquerda).
    }
}

void print_matriz(uint8_t option) {
    // Array do Sprite a ser exibido na matriz neopixel
    int matrizC[5][5][3] = {
        {{0, 0, 0}, {0, 40, 255}, {0, 40, 255}, {0, 40, 255}, {0, 0, 0}},
        {{0, 0, 0}, {0, 40, 255}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 40, 255}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 40, 255}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 40, 255}, {0, 40, 255}, {0, 40, 255}, {0, 0, 0}}
    };

    int matrizD[5][5][3] = {
        {{0, 0, 0}, {0, 40, 255}, {0, 40, 255}, {0, 40, 255}, {0, 0, 0}},
        {{0, 0, 0}, {0, 40, 255}, {0, 0, 0}, {0, 0, 0}, {0, 40, 255}},
        {{0, 0, 0}, {0, 40, 255}, {0, 0, 0}, {0, 0, 0}, {0, 40, 255}},
        {{0, 0, 0}, {0, 40, 255}, {0, 0, 0}, {0, 0, 0}, {0, 40, 255}},
        {{0, 0, 0}, {0, 40, 255}, {0, 40, 255}, {0, 40, 255}, {0, 0, 0}}
    };
    
    int matrizE[5][5][3] = {
        {{0, 0, 0}, {0, 40, 255}, {0, 40, 255}, {0, 40, 255}, {0, 0, 0}},
        {{0, 0, 0}, {0, 40, 255}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 40, 255}, {0, 40, 255}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 40, 255}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 40, 255}, {0, 40, 255}, {0, 40, 255}, {0, 0, 0}}
    };

    int matrizF[5][5][3] = {
        {{0, 0, 0}, {0, 40, 255}, {0, 40, 255}, {0, 40, 255}, {0, 0, 0}},
        {{0, 0, 0}, {0, 40, 255}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 40, 255}, {0, 40, 255}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 40, 255}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 40, 255}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}
    };
 
    int matrizG[5][5][3] = {
        {{0, 0, 0}, {0, 40, 255}, {0, 40, 255}, {0, 40, 255}, {0, 40, 255}},
        {{0, 0, 0}, {0, 40, 255}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 40, 255}, {0, 0, 0}, {0, 40, 255}, {0, 40, 255}},
        {{0, 0, 0}, {0, 40, 255}, {0, 0, 0}, {0, 0, 0}, {0, 40, 255}},
        {{0, 0, 0}, {0, 40, 255}, {0, 40, 255}, {0, 40, 255}, {0, 40, 255}}
    };

    int matrizV[5][5][3] = {
        {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 245, 85}},
        {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 245, 85}, {0, 0, 0}},
        {{0, 245, 85}, {0, 0, 0}, {0, 245, 85}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 245, 85}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}
    };

    int matrizX[5][5][3] = {
        {{255, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {255, 0, 0}},
        {{0, 0, 0}, {255, 0, 0}, {0, 0, 0}, {255, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 0, 0}, {255, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {255, 0, 0}, {0, 0, 0}, {255, 0, 0}, {0, 0, 0}},
        {{255, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {255, 0, 0}}
    };
    
    switch (option) {
        case 0:
            for (int linha = 0; linha < 5; linha++) {
                for (int coluna = 0; coluna < 5; coluna++) {
                    int posicao = getIndex(linha, coluna);
                    npSetLED(posicao, matrizC[coluna][linha][0], matrizC[coluna][linha][1], matrizC[coluna][linha][2]);
                }
                npWrite();
            }
            break;

        case 1:
            for (int linha = 0; linha < 5; linha++) {
                for (int coluna = 0; coluna < 5; coluna++) {
                    int posicao = getIndex(linha, coluna);
                    npSetLED(posicao, matrizD[coluna][linha][0], matrizD[coluna][linha][1], matrizD[coluna][linha][2]);
                }
                npWrite();
            }
            break;

        case 2:
            for (int linha = 0; linha < 5; linha++) {
                for (int coluna = 0; coluna < 5; coluna++) {
                    int posicao = getIndex(linha, coluna);
                    npSetLED(posicao, matrizE[coluna][linha][0], matrizE[coluna][linha][1], matrizE[coluna][linha][2]);
                }
                npWrite();
            }
            break;
        
        case 3:
            for (int linha = 0; linha < 5; linha++) {
                for (int coluna = 0; coluna < 5; coluna++) {
                    int posicao = getIndex(linha, coluna);
                    npSetLED(posicao, matrizF[coluna][linha][0], matrizF[coluna][linha][1], matrizF[coluna][linha][2]);
                }
                npWrite();
            }
            break;

        case 4:
            for (int linha = 0; linha < 5; linha++) {
                for (int coluna = 0; coluna < 5; coluna++) {
                    int posicao = getIndex(linha, coluna);
                    npSetLED(posicao, matrizG[coluna][linha][0], matrizG[coluna][linha][1], matrizG[coluna][linha][2]);
                }
                npWrite();
            }
            break;

        case 5:
            for (int linha = 0; linha < 5; linha++) {
                for (int coluna = 0; coluna < 5; coluna++) {
                    int posicao = getIndex(linha, coluna);
                    npSetLED(posicao, matrizV[coluna][linha][0], matrizV[coluna][linha][1], matrizV[coluna][linha][2]);
                }
                npWrite();
            }
            break;

        case 6:
            for (int linha = 0; linha < 5; linha++) {
                for (int coluna = 0; coluna < 5; coluna++) {
                    int posicao = getIndex(linha, coluna);
                    npSetLED(posicao, matrizX[coluna][linha][0], matrizX[coluna][linha][1], matrizX[coluna][linha][2]);
                }
                npWrite();
            }
            break;

        default:
            break;
    }
}

void metronomo(uint16_t *vry_value) {
    absolute_time_t tempo_inicial = get_absolute_time(); // Marca o tempo inicial
    uint16_t bpm;
    char *mensagem3[] = {
        "             ",
        " mova Joystick ",
        "   Pressione B   ",
        " para selecionar",
        "     BPM    "
    };
    print_display(mensagem3);
    while (gpio_get(BUTTON_B)) {
        printf("esperando botao");
        printf("BPM:  %d\n", bpm);
        *vry_value = adc_read(); 
        bpm = (*vry_value * (180 - 50)) / 4095 + 50; // Fórmula de mapeamento
        sleep_ms(500);
    }
    
    printf("%s", lock ? "true" : "false");
    while (lock == true) { // Loop infinito para o metrônomo contínuo
        printf("BPM:  %d\n", bpm);
        int intervalo_ms = 30000 / bpm;  // Tempo entre batidas (ms)
        absolute_time_t tempo_atual = get_absolute_time();  // Pega o tempo atual
        int tempo_passado_ms = absolute_time_diff_us(tempo_inicial, tempo_atual) / 1000;  // Calcula o tempo passado em milissegundos

        if (tempo_passado_ms >= intervalo_ms) {
            // Emite um beep curto
            beep(BUZZER_PIN, 100, 4000);
            
            // Aciona o LED central na cor verde
            npClear();
            npSetLED(12, 0, 50, 0); // LED central em verde
            npWrite();
            
            // Aguarda meio intervalo (LED aceso por metade do tempo)
            sleep_ms(intervalo_ms / 2);

            // Desliga o LED
            npClear();
            npWrite();
            
            tempo_inicial = get_absolute_time();  // Atualiza o tempo inicial para o próximo ciclo
        }
    }
    printf("%s", lock ? "true" : "false");
}