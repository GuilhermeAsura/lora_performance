#include "LoRa_E220.h"
//#include <SoftwareSerial.h>
#include "FS.h"
#include "SPIFFS.h"

// Definição dos pinos para ESP32 (ajuste conforme sua conexão)
//SoftwareSerial mySerial(2, 3); // RX, TX
//LoRa_E220 e22(&mySerial, 4, 5, 6); // M0, M1, AUX


// Usando Serial2 para o ESP32 (pinos padrão: RX=16, TX=17, ajuste se necessário)
#define RX_PIN 16
#define TX_PIN 17
LoRa_E220 e22(&Serial2, 4, 5, 6); // M0, M1, AUX (Corrigido de Serial12 para Serial2)


#define MAX_RETRANSMISSIONS 3  // Número máximo de retransmissões
#define ACK_TIMEOUT 3000       // Timeout para ACK em milissegundos (3 segundos)

// Estrutura do pacote de dados
struct DataPacket {
  uint8_t type = 0x01;  // Tipo: 0x01 para dados
  uint16_t seq_num;     // Número de sequência
  char data[240];       // Dados (máximo 240 bytes conforme E220)
};

// Estrutura do pacote de ACK
struct AckPacket {
  uint8_t type = 0x02;  // Tipo: 0x02 para ACK
  uint16_t seq_num;     // Número de sequência do pacote confirmado
};

uint16_t contador = 0;  // Contador para sequência de pacotes

void setup() {
  Serial.begin(9600); // Para depuração
//  mySerial.begin(9600);
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN); // Inicializa Serial2 com pinos personalizados
  e22.begin();

  // Inicializa o SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("Erro ao montar o SPIFFS");
    while (1);
  }
  Serial.println("Transmissor iniciado e SPIFFS montado");
}

void loop() {
  // Cria um pacote de dados
  DataPacket packet;
  packet.seq_num = contador;
  String mensagem = "Pacote " + String(contador);
  strncpy(packet.data, mensagem.c_str(), sizeof(packet.data) - 1);
  packet.data[sizeof(packet.data) - 1] = '\0'; // Garante terminação nula

  // Envia o pacote e tenta receber ACK
  bool ack_received = false;
  int attempts = 0;

  while (!ack_received && attempts <= MAX_RETRANSMISSIONS) {
    // Envia o pacote
    e22.sendMessage((uint8_t*)&packet, sizeof(packet.type) + sizeof(packet.seq_num) + strlen(packet.data) + 1);
    Serial.println("Enviado: Seq " + String(packet.seq_num) + ", Tentativa " + String(attempts + 1));

    // Salva log no SPIFFS
    File file = SPIFFS.open("/sent_packets.txt", "a");
    if (file) {
      file.println("Enviado: Seq " + String(packet.seq_num) + ", Tentativa " + String(attempts + 1));
      file.close();
    }

    // Espera por ACK
    unsigned long start_time = millis();
    while (millis() - start_time < ACK_TIMEOUT) {
      if (e22.available() > 0) {
        ResponseStructContainer rsc = e22.receiveMessage(sizeof(AckPacket));
        if (rsc.status.code == 1) { // Sucesso na recepção
          AckPacket ack;
          memcpy(&ack, rsc.data, sizeof(AckPacket));
          if (ack.type == 0x02 && ack.seq_num == packet.seq_num) {
            ack_received = true;
            Serial.println("ACK recebido para Seq " + String(packet.seq_num));
            file = SPIFFS.open("/sent_packets.txt", "a");
            if (file) {
              file.println("ACK recebido para Seq " + String(packet.seq_num));
              file.close();
            }
            break;
          }
        }
        rsc.close();
      }
    }

    if (!ack_received) {
      attempts++;
      if (attempts <= MAX_RETRANSMISSIONS) {
        Serial.println("Nenhum ACK, retransmitindo...");
      } else {
        Serial.println("Falha após " + String(MAX_RETRANSMISSIONS + 1) + " tentativas para Seq " + String(packet.seq_num));
        file = SPIFFS.open("/sent_packets.txt", "a");
        if (file) {
          file.println("Falha após " + String(MAX_RETRANSMISSIONS + 1) + " tentativas para Seq " + String(packet.seq_num));
          file.close();
        }
      }
    }
  }

  // Incrementa o contador para o próximo pacote
  contador++;
  delay(1000); // Intervalo entre envios (ajuste conforme necessário)
}
