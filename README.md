# RP2040 wifi demo

Para executar esse exemplo você deve ter o seu computador e a pico conectados na mesma rede wifi. 

Primeiro execute o server:

1. instale o `flask` (`pip3 install flask`) 
2. execute o `python python/server`

Anote o IP que o flask fornece:

``` bash
Local IP Address: 10.100.27.95
```

Modifique o cabecalho do código `main/main.c`:

``` c
#define WIFI_SSID "VIVOFIBRA-C891"
#define WIFI_PASSWORD "1aaaaaaaaa"
#define SERVER_IP "192.168.1.15"
```

Coloque a sua rede wifi e a senha, e no `SERVER_IP` use o IP anotado anteriormente.

Agora execute o firmware e o código deve realizar um `POST`, a parte do código que realiza isso é:
  
```c
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

  ```
