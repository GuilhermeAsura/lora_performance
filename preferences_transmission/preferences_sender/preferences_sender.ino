#include "LoRa_E220.h"
//#include <SoftwareSerial.h>
#include "Arduino.h"
#include <Preferences.h>

// Definição dos pinos para ESP32 (ajuste conforme o circuito)
//SoftwareSerial mySerial(2, 3); // RX, TX
//LoRa_E220 e220(&mySerial, 4, 5, 6); // M0, M1, AUX
#define RX_PIN 16
#define TX_PIN 17
LoRa_E220 e220(&Serial2, 4, 5, 6); // M0, M1, AUX (Corrigido de Serial12 para Serial2)
Preferences preferencias;

uint16_t contador = 0; // Contador para sequência de pacotes

void setup() {
  Serial.begin(115200); // Para depuração básica
//  mySerial.begin(9600);
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN); // Inicializa Serial2 com pinos personalizados
  e220.begin();

  // Inicializa a memória Preferences
  preferencias.begin("loraDados", false); // Namespace "loraDados", modo read/write
  Serial.println("Transmissor iniciado e Preferences montado");
}

void loop() {
  // Cria a mensagem com contador
  String mensagem = "Pacote " + String(contador);

  // Envia o pacote
  ResponseStatus rs = e220.sendMessage(mensagem);
  if (rs.code == 1) { // Sucesso no envio
    // Salva o pacote enviado na memória Preferences
    salvarDados(mensagem);
    Serial.println("Enviado: Seq " + String(contador)); // Depuração mínima
  } else {
    Serial.println("Erro ao enviar: " + rs.getResponseDescription());
  }

  contador++; // Incrementa o contador
  delay(1000); // Intervalo entre envios (ajuste conforme necessário)
}

void salvarDados(const String& dados) {
  String chave = "sent" + String(contador);
  preferencias.putString(chave.c_str(), dados); // Salva como chave-valor
}
