#include "SPIFFS.h"
#include "Arduino.h"
#include "LoRa_E22.h"

// Configuração dos pinos para o módulo LoRa
LoRa_E22 e22ttl(&Serial2, 18, 21, 19); 
void setup() {
    Serial.begin(115200);

    // Inicializa o SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("Falha ao montar o sistema de arquivos");
        return;
    }

    // Inicializa o LoRa
    e22ttl.begin();
}

void loop() {
    ResponseContainer resposta = e22ttl.receiveMessage();
    if (resposta.status.code == 1) {
        String mensagemRecebida = resposta.data;
        
        // Salvar mensagem na memória SPIFFS
        salvarDados(mensagemRecebida);
    }
}

void salvarDados(const String& dados) {
    File arquivo = SPIFFS.open("/dados.txt", FILE_APPEND);
    if (!arquivo) {
        Serial.println("Falha ao abrir o arquivo");
        return;
    }
    arquivo.println(dados);
    arquivo.close();
}
