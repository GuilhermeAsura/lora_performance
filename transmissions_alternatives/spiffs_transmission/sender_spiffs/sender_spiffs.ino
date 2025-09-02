#include "LoRa_E220.h"
//#include <SoftwareSerial.h>
#include "FS.h"
#include "SPIFFS.h"

// Definição dos pinos para ESP32 (ajuste conforme o circuito)
//SoftwareSerial mySerial(2, 3); // RX, TX
#define RX_PIN 16
#define TX_PIN 17
//LoRa_E220 e220(&mySerial, 4, 5, 6); // M0, M1, AUX
LoRa_E220 e220(&Serial2, 4, 5, 6); // M0, M1, AUX

uint16_t contador = 0; // Contador para sequência de pacotes

void setup() {
  Serial.begin(9600); // Para depuração básica
//  mySerial.begin(9600);
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  e220.begin();

  // Inicializa o SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("Erro ao montar o SPIFFS");
    while (1); // Para o programa em caso de erro
  }
  Serial.println("Transmissor iniciado e SPIFFS montado");
}

void loop() {
  // Cria a mensagem com contador
  String mensagem = "Pacote " + String(contador);

  // Envia o pacote
  ResponseStatus rs = e220.sendMessage(mensagem);
  if (rs.code == 1) { // Sucesso no envio
    // Salva o pacote enviado no SPIFFS
    File file = SPIFFS.open("/sent_packets.txt", "a"); // "a" para append
    if (file) {
      file.println("Enviado: Seq " + String(contador) + ", Mensagem: " + mensagem);
      file.close();
    } else {
      Serial.println("Falha ao abrir o arquivo para escrita");
    }
    Serial.println("Enviado: Seq " + String(contador)); // Depuração mínima
  } else {
    Serial.println("Erro ao enviar: " + rs.getResponseDescription());
  }

  contador++; // Incrementa o contador
  delay(1000); // Intervalo entre envios (ajuste conforme necessário)
}
