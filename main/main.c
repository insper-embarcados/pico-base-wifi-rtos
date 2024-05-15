#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwipopts.h"
#include "lwip/dns.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#define WIFI_SSID "VIVOFIBRA-C891"
#define WIFI_PASSWORD "1aaaaaaaaa"
#define SERVER_IP "192.168.1.15"

ip_addr_t ipaddr_server;
int zapier_dns_f = 0;

#define TCP_PORT 5000
#define DEBUG_printf printf
#define BUF_SIZE 2048
#define POLL_TIME_S 5

typedef struct TCP_CLIENT_T_ {
    struct tcp_pcb *tcp_pcb;
    ip_addr_t remote_addr;
    uint8_t buffer[BUF_SIZE];
    int buffer_len;
    int sent_len;
    bool complete;
    int run_count;
    bool connected;
} TCP_CLIENT_T;

void get_wifi_status(int status) {
    if (status == CYW43_LINK_UP) {
        printf("wifi on\n");

    } else {
        printf("wifi off\n");
    }
}

static void zapier_dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg) {
    if (ipaddr) {
        zapier_dns_f = 1;
        memcpy(&ipaddr_server, ipaddr, sizeof(ip_addr_t));
    } else {
        printf("zapier dns request failed\n");
    }
}

static void dump_bytes(const uint8_t *bptr, uint32_t len) {
    unsigned int i = 0;
    for (i = 0; i < len;) {
        printf("%c", bptr[i++]);
    }
    printf("\n");
}

static err_t tcp_client_close(void *arg) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T *)arg;
    err_t err = ERR_OK;
    if (state->tcp_pcb != NULL) {
        tcp_arg(state->tcp_pcb, NULL);
        tcp_poll(state->tcp_pcb, NULL, 0);
        tcp_sent(state->tcp_pcb, NULL);
        tcp_recv(state->tcp_pcb, NULL);
        tcp_err(state->tcp_pcb, NULL);
        err = tcp_close(state->tcp_pcb);
        if (err != ERR_OK) {
            DEBUG_printf("close failed %d, calling abort\n", err);
            tcp_abort(state->tcp_pcb);
            err = ERR_ABRT;
        }
        state->tcp_pcb = NULL;
    }
    return err;
}

static err_t tcp_client_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T *)arg;
    DEBUG_printf("tcp_client_sent %u\n", len);
    state->sent_len += len;

    // Print ACK details
    DEBUG_printf("ACK received - Sequence Number: %u, Acknowledgment Number: %u\n",
                 tpcb->snd_nxt, tpcb->rcv_nxt);

    if (state->sent_len >= BUF_SIZE) {
        state->run_count++;

        // We should receive a new buffer from the server
        state->buffer_len = 0;
        state->sent_len = 0;
        DEBUG_printf("Waiting for buffer from server\n");
    }

    return ERR_OK;
}

static err_t tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T *)arg;
    if (err != ERR_OK) {
        printf("connect failed %d\n", err);
        // return tcp_result(arg, err);
    }
    state->connected = true;
    DEBUG_printf("Waiting for buffer from server\n");
    return ERR_OK;
}

static err_t tcp_client_poll(void *arg, struct tcp_pcb *tpcb) {
    DEBUG_printf("tcp_client_poll\n");
    // return tcp_result(arg, -1); // no response is an error?
}

static void tcp_client_err(void *arg, err_t err) {
    if (err != ERR_ABRT) {
        DEBUG_printf("tcp_client_err %d\n", err);
        // tcp_result(arg, err);
    }
}

err_t tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T *)arg;
    if (!p) {
        return ERR_TIMEOUT;
    }
    // this method is callback from lwIP, so cyw43_arch_lwip_begin is not required, however you
    // can use this method to cause an assertion in debug mode, if this method is called when
    // cyw43_arch_lwip_begin IS needed
    cyw43_arch_lwip_check();
    if (p->tot_len > 0) {
        DEBUG_printf("recv %d err %d\n", p->tot_len, err);
        for (struct pbuf *q = p; q != NULL; q = q->next) {
            dump_bytes(q->payload, q->len);
        }
        // Receive the buffer
        const uint16_t buffer_left = BUF_SIZE - state->buffer_len;
        state->buffer_len += pbuf_copy_partial(p, state->buffer + state->buffer_len,
                                               p->tot_len > buffer_left ? buffer_left : p->tot_len, 0);
        tcp_recved(tpcb, p->tot_len);
    }
    pbuf_free(p);

    if (state->buffer_len == BUF_SIZE) {
        DEBUG_printf("Writing %d bytes to server\n", state->buffer_len);
        err_t err = tcp_write(tpcb, state->buffer, state->buffer_len, TCP_WRITE_FLAG_COPY);
        if (err != ERR_OK) {
            DEBUG_printf("Failed to write data %d\n", err);
            return -1;
        }
    }
    return ERR_OK;
}

static bool tcp_client_open(void *arg) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T *)arg;
    DEBUG_printf("Connecting to %s port %u\n", ip4addr_ntoa(&state->remote_addr), TCP_PORT);
    state->tcp_pcb = tcp_new_ip_type(IP_GET_TYPE(&state->remote_addr));

    if (!state->tcp_pcb) {
        DEBUG_printf("failed to create pcb\n");
        return false;
    }

    tcp_arg(state->tcp_pcb, state);
    tcp_poll(state->tcp_pcb, tcp_client_poll, POLL_TIME_S * 2);
    tcp_sent(state->tcp_pcb, tcp_client_sent);
    tcp_recv(state->tcp_pcb, tcp_client_recv);
    tcp_err(state->tcp_pcb, tcp_client_err);
    state->tcp_pcb->so_options |= SOF_KEEPALIVE;
    state->buffer_len = 0;

    cyw43_arch_lwip_begin();
    err_t err = tcp_connect(state->tcp_pcb, &state->remote_addr, TCP_PORT, tcp_client_connected);
    cyw43_arch_lwip_end();

    return err == ERR_OK;
}

static TCP_CLIENT_T *tcp_client_init(void) {
    cyw43_arch_lwip_begin();
    int err = dns_gethostbyname("hooks.zapier.com", &ipaddr_server, zapier_dns_found, NULL);
    cyw43_arch_lwip_end();

    while (zapier_dns_f == 0 && err != ERR_OK) {
        sleep_ms(10);
    }
    printf("zapier address %s\n", ipaddr_ntoa(&ipaddr_server));

    TCP_CLIENT_T *state = calloc(1, sizeof(TCP_CLIENT_T));
    if (!state) {
        DEBUG_printf("failed to allocate state\n");
        return NULL;
    }

    // ip4addr_aton(ipaddr_ntoa(&ipaddr_server), &state->remote_addr);
    ip4addr_aton(SERVER_IP, &state->remote_addr);
    return state;
}

void run_tcp_client_test(void) {
    TCP_CLIENT_T *state = tcp_client_init();
    if (!state) {
        return;
    }
    if (!tcp_client_open(state)) {
        return;
    }

    const char http_request[] =
        "POST / HTTP/1.1\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Content-Length: 11\r\n"
        "\r\n"
        "batata=999";

    int len = strlen(http_request);
    for (int i = 0; i < 5; i++) {
        cyw43_arch_lwip_begin();
        int err = tcp_write(state->tcp_pcb, http_request, len, 0);
        cyw43_arch_lwip_end();
        printf("err: %d \n", err);
        sleep_ms(1000);
    }

    tcp_client_close(state);
    while (1) {
        sleep_ms(100);
    }

    free(state);
}

int main() {
    stdio_init_all();

    // Inicializa o módulo Wi-Fi
    if (cyw43_arch_init()) {
        printf("Falha na inicialização do Wi-Fi\n");
        return -1;
    }
    printf("Wi-Fi inicializado com sucesso\n");

    // Ativa o modo de estação (STA)
    cyw43_arch_enable_sta_mode();

    // Tenta conectar ao Wi-Fi
    int result = -1;

    // Verifica o resultado da conexão
    while (result != 0) {
        result = cyw43_arch_wifi_connect_blocking(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_MIXED_PSK);
        printf("Conexão Wi-Fi falhou\n");
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0); // Desliga o LED
        sleep_ms(500);
    }

    // Se conectado com sucesso, acende o LED
    printf("Conectado ao Wi-Fi com sucesso\n");
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1); // Acende o LED

    // Mantém o programa rodando
    while (true) {
        get_wifi_status(cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA));
        char sIP[] = "xxx.xxx.xxx.xxx";
        strcpy(sIP, ip4addr_ntoa(netif_ip4_addr(netif_list)));
        printf("Conectado, IP %s\n", sIP);

        run_tcp_client_test();

        while (1) {
            sleep_ms(10);
        }
    }
}
