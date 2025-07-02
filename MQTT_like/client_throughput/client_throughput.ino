// Implementação de um client MQTT-like com medição de throughput
// Publica mensagens em tópicos e mede a taxa de transferência

#include "LoRa_E22.h"
#include "Arduino.h"
#include <Preferences.h>

// --- Pinout para ESP32 ---
#define RX_PIN 16
#define TX_PIN 17
#define AUX_PIN 15
#define M0_PIN 19
#define M1_PIN 21

// --- Configuração LoRa ---
#define BROKER_ADDRESS 0x0001 // Endereço do broker
#define LORA_CHANNEL 23      // Canal LoRa
#define LORA_BAUD 9600       // Baud rate da Serial2

LoRa_E22 lora(&Serial2, AUX_PIN, M0_PIN, M1_PIN);

// --- Estruturas de Pacotes (MQTT-like) ---
struct ConnectPacket {
  uint8_t type = 0x01;
  uint16_t client_id;
  uint16_t keep_alive;
};

struct ConnAckPacket {
  uint8_t type = 0x02;
  uint8_t return_code;
};

struct SubscribePacket {
  uint8_t type = 0x03;
  uint16_t packet_id;
  char topic[50];
};

struct SubAckPacket {
  uint8_t type = 0x04;
  uint16_t packet_id;
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

// --- Variáveis Globais ---
uint16_t packet_id = 0;
bool connected = false;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial2.begin(LORA_BAUD, SERIAL_8N1, RX_PIN, TX_PIN);
  lora.begin();

  Serial.println("Client LoRa iniciado. Tentando conectar ao broker...");
}

void loop() {
  if (!connected) {
    connectToBroker();
    delay(2000); // Aguarda antes de tentar novamente
    return;
  }

  // --- Lógica de Envio e Medição de Throughput ---
  PublishPacket publish;
  publish.type = 0x05;
  publish.packet_id = packet_id;
  strncpy(publish.topic, "sensors/throughput", sizeof(publish.topic) - 1);
  publish.topic[sizeof(publish.topic) - 1] = '\0';
  
  String payload = "Packet #" + String(packet_id);
  strncpy(publish.payload, payload.c_str(), sizeof(publish.payload) - 1);
  publish.payload[sizeof(publish.payload) - 1] = '\0';

  // **1. Armazena o tempo de início (start_time)**
  unsigned long start_time = millis();

  // **2. Envia o pacote PUBLISH para o broker**
  lora.sendFixedMessage(highByte(BROKER_ADDRESS), lowByte(BROKER_ADDRESS), LORA_CHANNEL, &publish, sizeof(publish));
  Serial.println("Enviado PUBLISH, packet_id=" + String(publish.packet_id));

  // **3. Aguarda o pacote de confirmação (PUBACK)**
  bool ack_received = false;
  unsigned long timeout = 3000; // Timeout de 3 segundos
  unsigned long wait_start_time = millis();

  while (millis() - wait_start_time < timeout) {
    if (lora.available() > 0) {
      ResponseStructContainer rsc = lora.receiveMessage(sizeof(PubAckPacket));
      if (rsc.status.code == 1) {
        PubAckPacket pubAck;
        memcpy(&pubAck, rsc.data, sizeof(pubAck));
        
        if (pubAck.type == 0x06 && pubAck.packet_id == publish.packet_id) {
          // **4. Confirmação recebida, armazena o tempo final (end_time)**
          unsigned long end_time = millis();
          ack_received = true;
          
          Serial.println("Recebido PUBACK para packet_id=" + String(pubAck.packet_id));

          // **5. Calcula o tempo de transmissão**
          float transmission_time_s = (end_time - start_time) / 1000.0;
          
          // **6. Calcula o throughput**
          // O tamanho do pacote é o tamanho da estrutura `PublishPacket` em bits.
          float packet_length_bits = sizeof(PublishPacket) * 8.0;
          float throughput_bps = 0;
          if (transmission_time_s > 0) {
              throughput_bps = packet_length_bits / transmission_time_s;
          }
          
          // **7. Imprime o resultado do throughput**
          Serial.print("Tempo de Transmissão (RTT): ");
          Serial.print(transmission_time_s, 3);
          Serial.println(" s");
          
          Serial.print("Tamanho do Pacote: ");
          Serial.print(packet_length_bits);
          Serial.println(" bits");
          
          Serial.print("Throughput: ");
          Serial.print(throughput_bps, 2);
          Serial.println(" bps");
          Serial.println("--------------------");

          rsc.close();
          break; // Sai do loop de espera
        }
      }
      rsc.close();
    }
  }

  if (ack_received) {
    packet_id++;
  } else {
    Serial.println("Nenhum PUBACK recebido para packet_id=" + String(publish.packet_id) + ". Timeout.");
    Serial.println("--------------------");
  }

  delay(5000); // Intervalo de 5 segundos entre os envios
}

void connectToBroker() {
  ConnectPacket connect = {0x01, 0x0002, 60};
  lora.sendFixedMessage(highByte(BROKER_ADDRESS), lowByte(BROKER_ADDRESS), LORA_CHANNEL, &connect, sizeof(connect));
  Serial.println("Enviado CONNECT...");

  unsigned long start_time = millis();
  while (millis() - start_time < 3000) {
    if (lora.available() > 0) {
      ResponseStructContainer rsc = lora.receiveMessage(sizeof(ConnAckPacket));
      if (rsc.status.code == 1) {
        ConnAckPacket connAck;
        memcpy(&connAck, rsc.data, sizeof(connAck));
        if (connAck.type == 0x02 && connAck.return_code == 0) {
          connected = true;
          Serial.println("Conexão com o broker estabelecida com sucesso!");
          rsc.close();
          return;
        }
      }
      rsc.close();
    }
  }
  Serial.println("Falha ao conectar com o broker. Tentando novamente...");
}