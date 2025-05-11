#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h" // Biblioteca para arquitetura Wi-Fi da Pico com CYW43
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
// #include "pico/bootrom.h" // Biblioteca para inicialização do bootrom

#include "lwip/pbuf.h"  // Lightweight IP stack - manipulação de buffers de pacotes de rede
#include "lwip/tcp.h"   // Lightweight IP stack - fornece funções e estruturas para trabalhar com o protocolo TCP
#include "lwip/netif.h" // Lightweight IP stack - fornece funções e estruturas para trabalhar com interfaces de rede (netif)
#include "lwipopts.h"   // Lightweight IP stack - O lwIP é uma implementação independente do conjunto de protocolos TCP/IP

#include "lib/ssd1306/ssd1306.h"
#include "lib/ssd1306/display.h"
#include "lib/led/led.h"
#include "lib/button/button.h"
#include "lib/ws2812b/ws2812b.h"
// #include "lib/buzzer/buzzer.h"
#include "config/wifi_config.h"
#include "public/html_data.h"

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

#define CYW43_LED_PIN CYW43_WL_GPIO_LED_PIN // GPIO do CI CYW43
#define PARKING_LOT_SIZE 4                  // Tamanho do estacionamento
#define LED_MATRIX_PIN 7                    // GPIO da matriz de LEDs

typedef struct parking_lot
{
    uint8_t id;                             // ID do estacionamento
    uint8_t status;                         // Status do estacionamento (0 - livre, 1 - ocupado, 2 - reservado)
    TickType_t reservation_start_time; // Hora de início da reserva
    bool is_pcd;                            // Se o estacionamento é PCD (Pessoa com Deficiência)
} parking_lot_t;

int init_cyw43_arch();                                                                    // Inicializa a arquitetura do cyw43
int init_webserver(struct tcp_pcb **server);                                              // Inicializa o servidor web
void init_parking_lots();                                                                 // Inicializa o estacionamento
void vWebServerTask(void *pvParameters);                                                  // Tarefa do servidor web
void vInputControlTask(void *pvParameters);                                               // Tarefa da interface do usuário
void vLedMatrixTask(void *pvParameters);                                                  // Tarefa da matriz de LEDs
void vReservationTimeoutTask(void *pvParameters);                                         // Tarefa de reserva
void vDisplayTask(void *pvParameters);                                                  // Tarefa do display
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err);             // Função de callback ao aceitar conexões TCP
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err); // Função de callback para processar requisições HTTP
void user_request(char **request);                                                        // Tratamento do request do usuário

static volatile parking_lot_t parking_lots[PARKING_LOT_SIZE]; // Array de estruturas para armazenar o status do estacionamento
static volatile int8_t current_parking_lot = 0;               // Vaga de estacionamento atual

int main()
{
    stdio_init_all();
    init_parking_lots(); // Inicializa o estacionamento

    xTaskCreate(vWebServerTask, "WebServerTask", 2*configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(vInputControlTask, "InputControlTask", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vLedMatrixTask, "LedMatrixTask", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(vReservationTimeoutTask, "ReservationTimeoutTask", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(vDisplayTask, "DisplayTask", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY + 1, NULL);

    vTaskStartScheduler();
    panic_unsupported();
}

// Inicializa a arquitetura do cyw43
int init_cyw43_arch()
{
    while (cyw43_arch_init())
    {
        //printf("Falha ao inicializar Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }

    // GPIO do CI CYW43 em nível baixo
    cyw43_arch_gpio_put(CYW43_LED_PIN, 0);

    // Ativa o Wi-Fi no modo Station, de modo a que possam ser feitas ligações a outros pontos de acesso Wi-Fi.
    cyw43_arch_enable_sta_mode();

    // Conectar à rede WiFI - fazer um loop até que esteja conectado
    //printf("Conectando ao Wi-Fi...\n");
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 20000))
    {
        //printf("Falha ao conectar ao Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }
    //printf("Conectado ao Wi-Fi\n");

    // Caso seja a interface de rede padrão - imprimir o IP do dispositivo.
    if (netif_default)
    {
        printf("IP do dispositivo: %s\n", ipaddr_ntoa(&netif_default->ip_addr));
    }

    return 0;
}

// Inicializa o servidor web
int init_webserver(struct tcp_pcb **server)
{
    // vincula um PCB TCP a um endereço IP e porta específicos
    if (tcp_bind(*server, IP_ADDR_ANY, 80) != ERR_OK)
    {
        //printf("Falha ao associar servidor TCP à porta 80\n");
        return -1;
    }

    // Coloca o PCB TCP em modo de escuta
    *server = tcp_listen(*server); // Atualiza o ponteiro original
    if (*server == NULL)
    {
        //printf("Falha ao criar servidor de escuta\n");
        return -1;
    }

    // Define função de callback para aceitar conexões
    tcp_accept(*server, tcp_server_accept);
    //printf("Servidor ouvindo na porta 80\n");

    return 0;
}

// Inicializa o estacionamento
void init_parking_lots()
{
    for (int i = 0; i < PARKING_LOT_SIZE; i++)
    {
        parking_lots[i].id = i + 1;                 // ID do estacionamento
        parking_lots[i].status = 0;                 // Status do estacionamento (0 - livre)
        parking_lots[i].reservation_start_time = 0; // Hora de início da reserva
        parking_lots[i].is_pcd = false;             // Se o estacionamento é PCD (Pessoa com Deficiência)
    }
    parking_lots[3].is_pcd = true; // o estacionamento 3 é PCD
}

// Função de callback ao aceitar conexões TCP
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    // printf("Conexão aceita\n");
    tcp_recv(newpcb, tcp_server_recv);
    return ERR_OK;
}

// Tratamento do request do usuário - digite aqui
void user_request(char **request)
{
    for (int i = 0; i < PARKING_LOT_SIZE; i++)
    {
        char endpoint[25];
        snprintf(endpoint, sizeof(endpoint), "GET /reservar-vaga-%d", i + 1);
        // printf("Endpoint: %s\n", endpoint);

        if (strstr(*request, endpoint) != NULL)
        {
            absolute_time_t current_time = pdTICKS_TO_MS(xTaskGetTickCount());

            parking_lots[i].status = 2;                            // Vaga reservada
            parking_lots[i].reservation_start_time = current_time; // Hora de início da reserva

            //printf("Reservando vaga %d\n", i + 1);
            break; // Sai do loop após encontrar a vaga correspondente
        }
    }
}

// Função de callback para processar requisições HTTP
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if (!p)
    {
        tcp_close(tpcb);
        tcp_recv(tpcb, NULL);
        return ERR_OK;
    }

    // Alocação do request na memória dinâmica
    char *request = (char *)malloc(p->len + 1);
    memcpy(request, p->payload, p->len);
    request[p->len] = '\0';

    // printf("Request: %s\n", request);

    // Tratamento de request - Controle dos LEDs
    user_request(&request);

    char html[3000];

    const char *status_class[] = {"disponivel", "ocupada", "reservada"};
    const char *status_text[] = {"Disponível", "Ocupada", "Reservada"};
    const char *disabled_btn[] = {"", "disabled", "disabled"};

    snprintf(html, sizeof(html), html_data,
             // Vaga 1
             parking_lots[0].is_pcd ? "pcd" : "",
             status_class[parking_lots[0].status],
             parking_lots[0].is_pcd ? " - PCD" : "",
             parking_lots[0].is_pcd ? "Vaga exclusiva para PCD" : "Vaga comum",
             status_text[parking_lots[0].status],
             disabled_btn[parking_lots[0].status],

             // Vaga 2
             parking_lots[1].is_pcd ? "pcd" : "",
             status_class[parking_lots[1].status],
             parking_lots[1].is_pcd ? " - PCD" : "",
             parking_lots[1].is_pcd ? "Vaga exclusiva para PCD" : "Vaga comum",
             status_text[parking_lots[1].status],
             disabled_btn[parking_lots[1].status],

             // Vaga 3
             parking_lots[2].is_pcd ? "pcd" : "",
             status_class[parking_lots[2].status],
             parking_lots[2].is_pcd ? " - PCD" : "",
             parking_lots[2].is_pcd ? "Vaga exclusiva para PCD" : "Vaga comum",
             status_text[parking_lots[2].status],
             disabled_btn[parking_lots[2].status],

             // Vaga 4
             parking_lots[3].is_pcd ? "pcd" : "",
             status_class[parking_lots[3].status],
             parking_lots[3].is_pcd ? " - PCD" : "",
             parking_lots[3].is_pcd ? "Vaga exclusiva para PCD" : "Vaga comum",
             status_text[parking_lots[3].status],
             disabled_btn[parking_lots[3].status]);

    tcp_write(tpcb, html, strlen(html), TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);

    // libera memória alocada dinamicamente
    free(request);

    // libera um buffer de pacote (pbuf) que foi alocado anteriormente
    pbuf_free(p);

    return ERR_OK;
}

// Tarefa do servidor web
void vWebServerTask(void *pvParameters)
{
    struct tcp_pcb *server; // Ponteiro para o PCB (Protocol Control Block) TCP

    if (init_cyw43_arch() != 0)
    {
        //printf("Falha ao inicializar Wi-Fi\n");
        vTaskDelete(NULL);
    }

    server = tcp_new(); // Cria um novo PCB TCP
    if (!server || init_webserver(&server) != 0)
    {
        //printf("Falha ao inicializar servidor web\n");
        vTaskDelete(NULL);
    }

    while (1)
    {
        cyw43_arch_poll();              // Necessário para manter o Wi-Fi ativo
        vTaskDelay(pdMS_TO_TICKS(100)); // Reduz a carga da CPU
    }
}

// Tarefa do controle de entrada
void vInputControlTask(void *pvParameters)
{
    init_btns();          // Inicializa os botões
    init_btn(BTN_SW_PIN); // Inicializa o botão do joystick

    TickType_t last_a = 0, last_b = 0, last_sw = 0;
    const TickType_t debounce = pdMS_TO_TICKS(270);

    while (1)
    {
        TickType_t now = xTaskGetTickCount();

        // Verifica se o botão A está pressionado
        if (btn_is_pressed(BTN_A_PIN) && (now - last_a) > debounce)
        {
            last_a = now; // Atualiza o último tempo em que o botão A foi pressionado
            // Ação para o botão A
            //printf("Botão A pressionado\n");

            if (current_parking_lot > 0)
            {
                current_parking_lot--;
                //printf("Vaga atual: %d\n", current_parking_lot + 1);
            }
        }
        else if (btn_is_pressed(BTN_B_PIN) && (now - last_b) > debounce)
        {
            last_b = now; // Atualiza o último tempo em que o botão B foi pressionado
            // Ação para o botão B
            //printf("Botão B pressionado\n");

            if (current_parking_lot < PARKING_LOT_SIZE - 1)
            {
                current_parking_lot++;
                //printf("Vaga atual: %d\n", current_parking_lot + 1);
            }
        }
        else if (btn_is_pressed(BTN_SW_PIN) && (now - last_sw) > debounce)
        {
            last_sw = now; // Atualiza o último tempo em que o botão do joystick foi pressionado
            // Ação para o botão do joystick
            //printf("Botão do joystick pressionado\n");

            if (parking_lots[current_parking_lot].status == 0 || parking_lots[current_parking_lot].status == 2)
            {
                parking_lots[current_parking_lot].status = 1; // Vaga ocupada
                //printf("Vaga %d ocupada\n", current_parking_lot + 1);
            }
            else if (parking_lots[current_parking_lot].status == 1)
            {
                parking_lots[current_parking_lot].status = 0; // Vaga livre
                //printf("Vaga %d livre\n", current_parking_lot + 1);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// Tarefa da matriz de LEDs
void vLedMatrixTask(void *pvParameters)
{
    int parking_lot_positions[PARKING_LOT_SIZE][4] = {
        {15, 16, 23, 24},
        {18, 19, 20, 21},
        {3, 4, 5, 6},
        {0, 1, 8, 9},
    };

    uint8_t last_status[PARKING_LOT_SIZE] = {255, 255, 255, 255};
    bool first_run = true;
    bool changed = false;
    int color[3] = {0, 0, 0};

    ws2812b_init(LED_MATRIX_PIN);

    while (1)
    {
        changed = false;

        for (int i = 0; i < PARKING_LOT_SIZE; i++)
        {
            if (first_run || last_status[i] != parking_lots[i].status)
            {
                color[0] = 0; // Vermelho
                color[1] = 0; // Verde
                color[2] = 0; // Azul

                if (parking_lots[i].is_pcd && parking_lots[i].status == 0)
                    color[2] = 8; // Azul
                else if (parking_lots[i].status == 0)
                    color[1] = 8; // Verde
                else if (parking_lots[i].status == 1)
                    color[0] = 8; // Vermelho
                else if (parking_lots[i].status == 2)
                {
                    color[0] = 4; // Amarelo
                    color[1] = 8;
                }

                for (int j = 0; j < 4; j++)
                    ws2812b_draw_point(parking_lot_positions[i][j], color);

                last_status[i] = parking_lots[i].status;
                changed = true;
            }
        }

        if (changed)
            ws2812b_write();

        first_run = false;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Tarefa de reserva
void vReservationTimeoutTask(void *pvParameters)
{
    while (1)
    {
        TickType_t now = xTaskGetTickCount();

        for (int i = 0; i < PARKING_LOT_SIZE; i++)
        {
            if (parking_lots[i].status == 2)
            { // Se a vaga estiver reservada
                if ((now - parking_lots[i].reservation_start_time) > pdMS_TO_TICKS(10000))
                {
                    parking_lots[i].status = 0; // Libera a vaga
                    //printf("Reserva da vaga %d expirada\n", i + 1);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(250)); // Verifica a cada segundo
    }
}

void vDisplayTask(void *pvParameters)
{
    ssd1306_t ssd;
    init_display(&ssd);

    int current_status[PARKING_LOT_SIZE] = {0};
    int old_status[PARKING_LOT_SIZE] = {255}; // Força a atualização inicial
    bool is_changed = true;

    while (1)
    {
        // Verifica se houve mudança no status do estacionamento
        for (int i = 0; i < PARKING_LOT_SIZE; i++)
        {
            current_status[i] = parking_lots[i].status;

            if (current_status[i] != old_status[i])
            {
                is_changed = true;
                old_status[i] = current_status[i];
            }
        }

        // Atualiza o display apenas se houve mudança
        if (!is_changed)
        {
            vTaskDelay(pdMS_TO_TICKS(500)); // Atualiza a cada 500ms
            continue;
        }

        // Atualiza o display
        is_changed = false;
        ssd1306_fill(&ssd, false); // Limpa a tela
        draw_centered_text(&ssd, "Estacionamento", 0);
        ssd1306_draw_string(&ssd, "Vagas:", 0, 15);

        for (int i = 0; i < PARKING_LOT_SIZE; i++)
        {
            const char *status_text = (parking_lots[i].status == 0) ? "Livre" :
                                       (parking_lots[i].status == 1) ? "Ocupada" :
                                       (parking_lots[i].status == 2) ? "Reservada" : "Indefinida";

            char buffer[20];

            snprintf(buffer, sizeof(buffer), "%d: %s", i + 1, status_text);
            ssd1306_draw_string(&ssd, buffer, 5, (i * 10) + 25);
        }

        ssd1306_send_data(&ssd); // Envia os dados para o display
        vTaskDelay(pdMS_TO_TICKS(500)); // Atualiza a cada 500ms
    }
}
