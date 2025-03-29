#include <LittleFS.h>
#include "Arduino.h"
#include "LoRa_E22.h"

// Configuração dos pinos para o módulo LoRa
LoRa_E22 lora(&Serial2, 18, 21, 19);

void setup() {
    Serial.begin(115200);

    // Inicializa o LittleFS
    if (!LittleFS.begin()) {
        Serial.println("Falha ao montar o sistema de arquivos");
        return;
    }

    // Inicializa o LoRa
    lora.begin();
}

void loop() {
    ResponseContainer resposta = lora.receiveMessage();
    if (resposta.status.code == 1) {
        String mensagemRecebida = resposta.data;
        
        // Salvar mensagem na memória LittleFS
        salvarDados(mensagemRecebida);
    }
}

void salvarDados(const String& dados) {
    File arquivo = LittleFS.open("/dados.txt", FILE_APPEND);
    if (!arquivo) {
        Serial.println("Falha ao abrir o arquivo");
        return;
    }
    arquivo.println(dados);
    arquivo.close();
}
