#include "LoRa_E220.h"
//#include <SoftwareSerial.h>
#include "Arduino.h"
#include "LittleFS.h"

// Definição dos pinos para ESP32 (ajuste conforme o circuito)
//SoftwareSerial mySerial(2, 3); // RX, TX
//LoRa_E220 e220(&mySerial, 4, 5, 6); // M0, M1, AUX

// Usando Serial2 para o ESP32 (pinos padrão: RX=16, TX=17, ajuste se necessário)
#define RX_PIN 16
#define TX_PIN 17
LoRa_E220 e220(&Serial2, 4, 5, 6); // M0, M1, AUX (Corrigido de Serial12 para Serial2)

uint16_t contador = 0; // Contador para sequência de pacotes

void setup() {
  Serial.begin(115200); // Para depuração básica
//  mySerial.begin(9600);
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN); // Inicializa Serial2 com pinos personalizados
  e220.begin();

  // Inicializa o LittleFS
  if (!LittleFS.begin()) {
    Serial.println("Falha ao montar o LittleFS");
    while (1); // Para o programa em caso de erro
  }
  Serial.println("Transmissor iniciado e LittleFS montado");
}

void loop() {
  // Cria a mensagem com contador
  String mensagem = "Pacote " + String(contador);

  // Envia o pacote
  ResponseStatus rs = e220.sendMessage(mensagem);
  if (rs.code == 1) { // Sucesso no envio
    // Salva o pacote enviado no LittleFS
    salvarDados("Enviado: Seq " + String(contador) + ", Mensagem: " + mensagem);
    Serial.println("Enviado: Seq " + String(contador)); // Depuração mínima
  } else {
    Serial.println("Erro ao enviar: " + rs.getResponseDescription());
  }

  contador++; // Incrementa o contador
  delay(1000); // Intervalo entre envios (ajuste conforme necessário)
}

void salvarDados(const String& dados) {
  File arquivo = LittleFS.open("/sent_packets.txt", FILE_APPEND);
  if (!arquivo) {
    Serial.println("Falha ao abrir o arquivo");
    return;
  }
  arquivo.println(dados);
  arquivo.close();
}
