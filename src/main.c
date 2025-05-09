#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h" // Biblioteca para arquitetura Wi-Fi da Pico com CYW43
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include "pico/bootrom.h" // Biblioteca para inicialização do bootrom

#include "lwip/pbuf.h"  // Lightweight IP stack - manipulação de buffers de pacotes de rede
#include "lwip/tcp.h"   // Lightweight IP stack - fornece funções e estruturas para trabalhar com o protocolo TCP
#include "lwip/netif.h" // Lightweight IP stack - fornece funções e estruturas para trabalhar com interfaces de rede (netif)
#include "lwipopts.h"   // Lightweight IP stack - O lwIP é uma implementação independente do conjunto de protocolos TCP/IP

#include "lib/ssd1306/ssd1306.h"
#include "lib/ssd1306/display.h"
#include "lib/led/led.h"
#include "lib/button/button.h"
#include "lib/ws2812b/ws2812b.h"
#include "lib/buzzer/buzzer.h"
#include "config/wifi_config.h"
#include "public/html_data.h"

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

#define CYW43_LED_PIN CYW43_WL_GPIO_LED_PIN // GPIO do CI CYW43
#define PARKING_LOT_SIZE 4                  // Tamanho do estacionamento

typedef struct parking_lot
{
    uint8_t id;                             // ID do estacionamento
    uint8_t status;                         // Status do estacionamento (0 - livre, 1 - ocupado, 2 - reservado)
    absolute_time_t reservation_start_time; // Hora de início da reserva
    bool is_pcd;                            // Se o estacionamento é PCD (Pessoa com Deficiência)
} parking_lot_t;

int init_cyw43_arch();                                                                    // Inicializa a arquitetura do cyw43
int init_webserver(struct tcp_pcb **server);                                              // Inicializa o servidor web
void init_parking_lots();                                                                 // Inicializa o estacionamento
void vWebServerTask(void *pvParameters);                                                  // Tarefa do servidor web
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err);             // Função de callback ao aceitar conexões TCP
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err); // Função de callback para processar requisições HTTP
void user_request(char **request);                                                        // Tratamento do request do usuário
void gpio_irq_callback(uint gpio, uint32_t events);                                       // Função de callback para interrupções GPIO

parking_lot_t parking_lots[PARKING_LOT_SIZE]; // Array de estruturas para armazenar o status do estacionamento

int main()
{
    stdio_init_all();
    init_parking_lots(); // Inicializa o estacionamento

    init_btn(BUTTON_B_PIN); // Inicializa os botões

    gpio_set_irq_enabled_with_callback(BUTTON_B_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_callback); // Configura interrupção no botão B

    xTaskCreate(vWebServerTask, "WebServerTask", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY + 1, NULL);

    vTaskStartScheduler();
    panic_unsupported();
}

// Inicializa a arquitetura do cyw43
int init_cyw43_arch()
{
    while (cyw43_arch_init())
    {
        printf("Falha ao inicializar Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }

    // GPIO do CI CYW43 em nível baixo
    cyw43_arch_gpio_put(CYW43_LED_PIN, 0);

    // Ativa o Wi-Fi no modo Station, de modo a que possam ser feitas ligações a outros pontos de acesso Wi-Fi.
    cyw43_arch_enable_sta_mode();

    // Conectar à rede WiFI - fazer um loop até que esteja conectado
    printf("Conectando ao Wi-Fi...\n");
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 20000))
    {
        printf("Falha ao conectar ao Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }
    printf("Conectado ao Wi-Fi\n");

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
        printf("Falha ao associar servidor TCP à porta 80\n");
        return -1;
    }

    // Coloca o PCB TCP em modo de escuta
    *server = tcp_listen(*server); // Atualiza o ponteiro original
    if (*server == NULL)
    {
        printf("Falha ao criar servidor de escuta\n");
        return -1;
    }

    // Define função de callback para aceitar conexões
    tcp_accept(*server, tcp_server_accept);
    printf("Servidor ouvindo na porta 80\n");

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
    parking_lots[1].status = 1;   // o estacionamento 2 está ocupado
    parking_lots[2].status = 2;   // o estacionamento 3 está reservado
}

// Função de callback ao aceitar conexões TCP
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    printf("Conexão aceita\n");
    tcp_recv(newpcb, tcp_server_recv);
    return ERR_OK;
}

// Tratamento do request do usuário - digite aqui
void user_request(char **request)
{

    if (strstr(*request, "GET /blue_on") != NULL)
    {
        printf("Ligando LED azul\n");
    }
    else if (strstr(*request, "GET /blue_off") != NULL)
    {
        printf("Desligando LED azul\n");
    }
    else if (strstr(*request, "GET /green_on") != NULL)
    {
        printf("Ligando LED verde\n");
    }
    else if (strstr(*request, "GET /green_off") != NULL)
    {
        printf("Desligando LED verde\n");
    }
    else if (strstr(*request, "GET /red_on") != NULL)
    {
        printf("Ligando LED vermelho\n");
    }
    else if (strstr(*request, "GET /red_off") != NULL)
    {
        printf("Desligando LED vermelho\n");
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

    // Alocação do request na memória dinámica
    char *request = (char *)malloc(p->len + 1);
    memcpy(request, p->payload, p->len);
    request[p->len] = '\0';

    printf("Request: %s\n", request);

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
        printf("Falha ao inicializar Wi-Fi\n");
        vTaskDelete(NULL);
    }

    server = tcp_new(); // Cria um novo PCB TCP
    if (!server || init_webserver(&server) != 0)
    {
        printf("Falha ao inicializar servidor web\n");
        vTaskDelete(NULL);
    }

    while (1)
    {
        cyw43_arch_poll();              // Necessário para manter o Wi-Fi ativo
        vTaskDelay(pdMS_TO_TICKS(100)); // Reduz a carga da CPU
    }
}

// Função de callback para interrupções GPIO
void gpio_irq_callback(uint gpio, uint32_t events)
{
    reset_usb_boot(0, 0); // Reinicia o dispositivo e entra no modo de bootloader
}
