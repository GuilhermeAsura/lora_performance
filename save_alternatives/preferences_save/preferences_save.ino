#include <Preferences.h>
#include "Arduino.h"
#include "LoRa_E22.h"

// Configuração dos pinos para o módulo LoRa
LoRa_E22 lora(&Serial2, 18, 21, 19);
Preferences preferencias;

int contador = 0;

void setup() {
    Serial.begin(115200);

    // Inicializa a memória Preferences
    preferencias.begin("loraDados", false);

    // Inicializa o LoRa
    lora.begin();
}

void loop() {
    ResponseContainer resposta = lora.receiveMessage();
    if (resposta.status.code == 1) {
        String mensagemRecebida = resposta.data;
        
        // Salvar mensagem na memória Preferences
        salvarDados(mensagemRecebida);
    }
}

void salvarDados(const String& dados) {
    String chave = "msg" + String(contador);
    preferencias.putString(chave.c_str(), dados);
    contador++;
}
