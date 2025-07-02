// Implementação de um broker MQTT-like
// Recebe mensagens e envia confirmações (acknowledgments)

#include "Arduino.h"
#include "LoRa_E22.h"

// --- Pinout para ESP32 ---
#define RX_PIN 16
#define TX_PIN 17
#define AUX_PIN 15
#define M0_PIN 19
#define M1_PIN 21

// --- Configuração LoRa ---
#define LORA_CHANNEL 23      // Canal LoRa
#define LORA_BAUD 9600       // Baud rate da Serial2

LoRa_E22 lora(&Serial2, AUX_PIN, M0_PIN, M1_PIN);

// --- Estruturas de Pacotes (MQTT-like) ---
// As estruturas definem o formato dos pacotes trocados.
struct ConnectPacket {
  uint8_t type = 0x01;
  uint16_t client_id;
  uint16_t keep_alive;
};

struct ConnAckPacket {
  uint8_t type = 0x02;
  uint8_t return_code;
};

struct PublishPacket {
  uint8_t type = 0x05;
  uint16_t packet_id;
  char topic[50];
  char payload[190];
};

struct PubAckPacket {
  uint8_t type = 0x06;
  uint16_t packet_id;
};

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial2.begin(LORA_BAUD, SERIAL_8N1, RX_PIN, TX_PIN);
  lora.begin();
  
  Serial.println("Broker LoRa iniciado. Aguardando mensagens...");
}

void loop() {
  if (lora.available() > 0) {
    // Tenta receber um pacote de qualquer tipo, verificando o campo 'type'
    ResponseStructContainer rsc = lora.receiveMessage(sizeof(PublishPacket)); // Recebe o maior pacote possível
    if (rsc.status.code == 1) {
      uint8_t packet_type;
      memcpy(&packet_type, rsc.data, sizeof(packet_type)); // Lê apenas o primeiro byte para identificar o tipo
      
      if (packet_type == 0x01) { // CONNECT
        ConnectPacket connect;
        memcpy(&connect, rsc.data, sizeof(connect));
        
        Serial.println("Recebido CONNECT, client_id=" + String(connect.client_id));
        
        // Envia CONNACK de volta para o cliente que enviou a mensagem
        ConnAckPacket connAck = {0x02, 0}; // 0 = Sucesso
        lora.sendFixedMessage(highByte(connect.client_id), lowByte(connect.client_id), LORA_CHANNEL, &connAck, sizeof(connAck));
        Serial.println("Enviado CONNACK.");
        
      } else if (packet_type == 0x05) { // PUBLISH
        PublishPacket publish;
        memcpy(&publish, rsc.data, sizeof(publish));
        
        Serial.println("Recebido PUBLISH, packet_id=" + String(publish.packet_id));
        Serial.println("  > Topic: " + String(publish.topic));
        Serial.println("  > Payload: " + String(publish.payload));
        
        // **1. Ao receber um PUBLISH, envia um PUBACK de confirmação**
        // O endereço de destino do PUBACK deve ser o endereço do client (remetente do PUBLISH).
        // Neste exemplo, o client tem o endereço 0x0002.
        PubAckPacket pubAck = {0x06, publish.packet_id};
        lora.sendFixedMessage(highByte(0x0002), lowByte(0x0002), LORA_CHANNEL, &pubAck, sizeof(pubAck));
        Serial.println("Enviado PUBACK de confirmação para packet_id=" + String(publish.packet_id));
        Serial.println("--------------------");
      }
      
      rsc.close();
    }
  }
}