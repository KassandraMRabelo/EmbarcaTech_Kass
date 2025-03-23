#include <stdio.h>  // Biblioteca padrão para entrada e saída
#include "pico/stdlib.h"  // Biblioteca padrão da Raspberry Pi Pico
#include "hardware/adc.h"  // Biblioteca para manipulação do ADC (leitura do sensor de temperatura)
#include "pico/cyw43_arch.h"  // Biblioteca para utilizar o módulo Wi-Fi da Pico W
#include "lwip/tcp.h"  // Biblioteca para comunicação via protocolo TCP (necessária para o servidor web)

// Definições de Wi-Fi
#define WIFI_SSID "Megalink_sonia"  // Nome da rede Wi-Fi (SSID)
#define WIFI_PASS "kally#*ww"  // Senha da rede Wi-Fi

// Função para medir a temperatura do sensor interno da Pico W
float temperatura_ambiente() {
    uint16_t raw = adc_read();  // Lê o valor bruto do sensor de temperatura (ADC)
    const float conversao = 3.3f / ((1 << 12) - 1);  // Converte o valor ADC para tensão (3.3V é a referência)
    float voltage = raw * conversao;  // Calcula a voltagem correspondente ao valor lido
    float temperature = 27 - (voltage - 0.706) / 0.001721;  // Conversão para temperatura em graus Celsius
    return temperature - 4;  // Ajuste do sensor, já que ele pode apresentar uma pequena diferença na leitura
}

// Função que gera e envia a página HTML com os dados dos sensores
void http_server(struct tcp_pcb *pcb, struct tcp_pcb *newpcb) {
    char response[512];  // Buffer para armazenar a resposta HTTP
    float temperatura = temperatura_ambiente();  // Obtém a temperatura atual do sensor
    int umidade = 60;  // Valor fixo para umidade (pode ser substituído por um sensor real)
    int qualidade_ar = 250;  // Valor fixo para qualidade do ar (pode ser substituído por um sensor real)

    // Criação da resposta HTTP (cabeçalho + corpo HTML)
    snprintf(response, sizeof(response),
        "HTTP/1.1 200 OK\r\n"  // Cabeçalho da resposta HTTP indicando sucesso
        "Content-Type: text/html\r\n\r\n"  // Indica que a resposta será em HTML
        "<html><head><meta http-equiv='refresh' content='2'></head>"  // Atualiza a página a cada 2 segundos
        "<body><h1>Dados do Sensor</h1>"  // Título da página
        "<p>Temperatura: %.1f C</p>"  // Exibe a temperatura
        "<p>Umidade: %d%%</p>"  // Exibe a umidade
        "<p>Qualidade do Ar: %d ppm</p>"  // Exibe a qualidade do ar
        "</body></html>\r\n",
        temperatura, umidade, qualidade_ar
    );

    // Envia a resposta HTTP para o cliente (navegador)
    tcp_write(newpcb, response, strlen(response), TCP_WRITE_FLAG_COPY);
    tcp_output(newpcb);  // Garante que os dados sejam enviados imediatamente
}

// Função que é chamada quando o servidor aceita uma conexão
err_t server_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, (tcp_recv_fn)http_server);  // Define que a função http_server tratará as requisições recebidas
    return ERR_OK;  // Retorna sucesso
}

int main() {
    stdio_init_all();  // Inicializa o Monitor Serial
    printf("Inicializando...\n");  // Mensagem de inicialização

    // Inicialização do sensor de temperatura interno (ADC)
    adc_init();  
    adc_set_temp_sensor_enabled(true);  // Habilita o sensor interno
    adc_select_input(4);  // Define o canal correto do ADC para leitura da temperatura

    // Inicializa o Wi-Fi
    if (cyw43_arch_init()) {  // Verifica se a inicialização do Wi-Fi falhou
        printf("Falha ao iniciar Wi-Fi\n");
        return 1;  // Sai do programa com erro
    }

    printf("Wi-Fi inicializado. Conectando...\n");  
    cyw43_arch_enable_sta_mode();  // Habilita o modo estação (conectar a uma rede Wi-Fi)

    printf("Tentando conectar ao Wi-Fi: %s...\n", WIFI_SSID);
    
    // Conecta ao Wi-Fi e verifica se ocorreu erro
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
        printf("Falha ao conectar ao Wi-Fi\n");
        return 1;
    } else {
        printf("Conectado ao Wi-Fi.\n");

        // Obtém o endereço IP da placa e exibe no monitor serial
        uint8_t *ip_address = (uint8_t*)&(cyw43_state.netif[0].ip_addr.addr);  
        printf("Endereço IP: %d.%d.%d.%d\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);
    }

    // Criação do servidor TCP
    struct tcp_pcb *pcb = tcp_new();  // Cria um novo bloco de controle TCP
    if (pcb == NULL) {  // Verifica se houve falha na criação
        printf("Falha ao criar PCB TCP!\n");
        return 1;
    }

    // Associa o servidor ao porto 80 (HTTP)
    tcp_bind(pcb, IP_ADDR_ANY, 80);  // IP_ADDR_ANY permite receber conexões de qualquer endereço IP

    // Coloca o servidor em modo de escuta
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, server_accept);  // Define a função que tratará conexões recebidas

    printf("Servidor rodando! Acesse via navegador o IP da placa.\n");

    // Loop infinito para manter o servidor ativo
    while (true) {
        sleep_ms(5000);  // Aguarda 5 segundos para evitar sobrecarga do processador
        stdio_flush();  // Limpa o buffer de saída do monitor serial
    }

    return 0;  // Código nunca chega aqui, pois o loop é infinito
}
