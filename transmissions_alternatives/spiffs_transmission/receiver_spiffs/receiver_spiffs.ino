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

uint16_t ultimo_contador = 0;
bool primeiro_pacote = true;

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
  Serial.println("Receptor iniciado e SPIFFS montado");
}

void loop() {
  if (e220.available() > 0) {
    ResponseContainer rc = e220.receiveMessage();
    if (rc.status.code == 1) { // Sucesso na recepção
      String mensagem = rc.data;

      // Salva o pacote recebido no SPIFFS
      File file = SPIFFS.open("/received_packets.txt", "a"); // "a" para append
      if (file) {
        file.println("Recebido: " + mensagem);
        file.close();
      } else {
        Serial.println("Falha ao abrir o arquivo para escrita");
      }

      // Extrai o número do pacote da mensagem para detecção de perda
      int pos = mensagem.lastIndexOf(' ');
      uint16_t contador_recebido = mensagem.substring(pos + 1).toInt();

      // Verifica perda de pacotes
      if (primeiro_pacote) {
        ultimo_contador = contador_recebido;
        primeiro_pacote = false;
      } else {
        if (contador_recebido != ultimo_contador + 1) {
          String perda = "Pacote(s) perdido(s) entre " + String(ultimo_contador) + " e " + String(contador_recebido);
          Serial.println(perda); // Imprime apenas perdas para depuração mínima
          file = SPIFFS.open("/received_packets.txt", "a");
          if (file) {
            file.println(perda);
            file.close();
          }
        }
        ultimo_contador = contador_recebido;
      }
    } else {
      Serial.println("Erro ao receber: " + rc.status.getResponseDescription());
    }
  }
}

// Função opcional para ler os pacotes salvos (chamar manualmente após o experimento)
void lerPacotesSalvos() {
  File file = SPIFFS.open("/received_packets.txt", "r"); // "r" para leitura
  if (!file) {
    Serial.println("Falha ao abrir o arquivo para leitura");
    return;
  }
  Serial.println("Conteúdo do arquivo /received_packets.txt:");
  while (file.available()) {
    String linha = file.readStringUntil('\n');
    Serial.println(linha);
  }
  file.close();
}
