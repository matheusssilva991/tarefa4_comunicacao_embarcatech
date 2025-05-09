#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h" // Biblioteca para arquitetura Wi-Fi da Pico com CYW43
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"

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

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

#define CYW43_LED_PIN CYW43_WL_GPIO_LED_PIN // GPIO do CI CYW43

int init_cyw43_arch(); // Inicializa a arquitetura do cyw43
int init_webserver(struct tcp_pcb *server); // Inicializa o servidor web
void vWebServerTask(void *pvParameters); // Tarefa do servidor web
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err); // Função de callback ao aceitar conexões TCP
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err); // Função de callback para processar requisições HTTP
void user_request(char **request); // Tratamento do request do usuário

int main()
{
    stdio_init_all();

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
int init_webserver(struct tcp_pcb *server)
{
    // Configura o servidor TCP - cria novos PCBs TCP. É o primeiro passo para estabelecer uma conexão TCP.

    server = tcp_new();
    // Verifica se o PCB TCP foi criado com sucesso. Se não, imprime uma mensagem de erro e retorna.

    if (!server)
    {
        printf("Falha ao criar servidor TCP\n");
        return -1;
    }

    // vincula um PCB (Protocol Control Block) TCP a um endereço IP e porta específicos.
    if (tcp_bind(server, IP_ADDR_ANY, 80) != ERR_OK)
    {
        printf("Falha ao associar servidor TCP à porta 80\n");
        return -1;
    }

    // Coloca um PCB (Protocol Control Block) TCP em modo de escuta, permitindo que ele aceite conexões de entrada.
    server = tcp_listen(server);

    // Define uma função de callback para aceitar conexões TCP de entrada. É um passo importante na configuração de servidores TCP.
    tcp_accept(server, tcp_server_accept);
    printf("Servidor ouvindo na porta 80\n");

    return 0;
}

// Função de callback ao aceitar conexões TCP
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
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

    // Cria a resposta HTML
    char html[1024];

    // Instruções html do webserver
    snprintf(html, sizeof(html), // Formatar uma string e armazená-la em um buffer de caracteres
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html\r\n"
             "\r\n"
             "<!DOCTYPE html>\n"
             "<html>\n"
             "<head>\n"
             "<title> Embarcatech - LED Control </title>\n"
             "<style>\n"
             "body { background-color: #b5e5fb; font-family: Arial, sans-serif; text-align: center; margin-top: 50px; }\n"
             "h1 { font-size: 64px; margin-bottom: 30px; }\n"
             "button { background-color: LightGray; font-size: 36px; margin: 10px; padding: 20px 40px; border-radius: 10px; }\n"
             ".temperature { font-size: 48px; margin-top: 30px; color: #333; }\n"
             "</style>\n"
             "</head>\n"
             "<body>\n"
             "<h1>Embarcatech: LED Control</h1>\n"
             "<form action=\"./blue_on\"><button>Ligar Azul</button></form>\n"
             "<form action=\"./blue_off\"><button>Desligar Azul</button></form>\n"
             "<form action=\"./green_on\"><button>Ligar Verde</button></form>\n"
             "<form action=\"./green_off\"><button>Desligar Verde</button></form>\n"
             "<form action=\"./red_on\"><button>Ligar Vermelho</button></form>\n"
             "<form action=\"./red_off\"><button>Desligar Vermelho</button></form>\n"
             "<p class=\"temperature\">Temperatura Interna: %.2f &deg;C</p>\n"
             "</body>\n"
             "</html>\n",
             30.0); // Temperatura interna - valor fictício

    // Escreve dados para envio (mas não os envia imediatamente).
    tcp_write(tpcb, html, strlen(html), TCP_WRITE_FLAG_COPY);

    // Envia a mensagem
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

    if (init_webserver(server) != 0)
    {
        printf("Falha ao inicializar servidor web\n");
        vTaskDelete(NULL);
    }

    while (1)
    {
        cyw43_arch_poll(); // Necessário para manter o Wi-Fi ativo
        vTaskDelay(pdMS_TO_TICKS(100)); // Reduz a carga da CPU
    }
}
