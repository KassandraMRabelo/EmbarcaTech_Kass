#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"

#define WIFI_SSID "Megalink_sonia"
#define WIFI_PASS "kally#*ww"

// Função para ler a temperatura
float temperatura_ambiente() {
    uint16_t raw = adc_read();
    const float conversao = 3.3f / ((1 << 12) - 1);
    float voltage = raw * conversao;
    float temperature = 27 - (voltage - 0.706) / 0.001721;
    return temperature - 4;  // Correção do sensor de temperatura
}

// Função para enviar resposta HTTP
void http_server(struct tcp_pcb *pcb, struct tcp_pcb *newpcb) {
    char response[512];
    float temperatura = temperatura_ambiente();
    int umidade = 60;  // Placeholder para umidade
    int qualidade_ar = 250;  // Placeholder para qualidade do ar

    snprintf(response, sizeof(response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n\r\n"
        "<html><head><meta http-equiv='refresh' content='2'></head>"
        "<body><h1>Dados do Sensor</h1>"
        "<p>Temperatura: %.1f C</p>"
        "<p>Umidade: %d%%</p>"
        "<p>Qualidade do Ar: %d ppm</p>"
        "</body></html>\r\n",
        temperatura, umidade, qualidade_ar
    );

    // Enviar resposta
    tcp_write(newpcb, response, strlen(response), TCP_WRITE_FLAG_COPY);
    tcp_output(newpcb);
}

// Função chamada quando a conexão é aceita
err_t server_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, (tcp_recv_fn)http_server);
    return ERR_OK;
}

int main() {
    stdio_init_all();  // Inicializar o monitor serial
    printf("Inicializando...\n");

    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4);  // Canal correto para o sensor de temperatura

    // Inicializar o Wi-Fi
    if (cyw43_arch_init()) {
        printf("Falha ao iniciar Wi-Fi\n");
        return 1;
    }

    printf("Wi-Fi inicializado. Conectando...\n");
    cyw43_arch_enable_sta_mode();

    printf("Tentando conectar ao Wi-Fi: %s...\n", WIFI_SSID);

    // Conectar ao Wi-Fi (corrigido a verificação de erro)
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
        printf("Falha ao conectar ao Wi-Fi\n");
        return 1;
    } else {
        printf("Conectado ao Wi-Fi.\n");

        // Obter o IP da placa
        uint8_t *ip_address = (uint8_t*)&(cyw43_state.netif[0].ip_addr.addr); // Formatando o Endereço IP
        printf("Endereço IP: %d.%d.%d.%d\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);
    }

    // Inicializar o servidor TCP
    struct tcp_pcb *pcb = tcp_new();
    if (pcb == NULL) {
        printf("Falha ao criar PCB TCP!\n");
        return 1;
    }

    // Associar o servidor ao porto 80 (HTTP)
    tcp_bind(pcb, IP_ADDR_ANY, 80);

    // Colocar o servidor em modo de escuta
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, server_accept);

    printf("Servidor rodando! Acesse via navegador o IP da placa.\n");

    // Loop principal para manter o servidor ativo
    while (true) {
        sleep_ms(5000);  // Aguardar para evitar sobrecarga da CPU
        stdio_flush();  // Garantir que a saída seja impressa no terminal
    }

    return 0;
}
