// Implementação de um broker MQTT-like
// Recebe mensagens e envia confirmações (acknowledgments)
#include "LoRa_E22.h"
#include "Arduino.h"

// Pinout & Setup
#define RX_PIN 16
#define TX_PIN 17
LoRa_E22 e22(&Serial2, 15, 21, 19); // AUX, M0, M1

// Endereçamento
#define MY_ADDRESS 0x0002
#define LORA_CHANNEL 0x12

// Tipos de Pacote
enum PacketType : uint8_t {
  CONNECT = 0x01,
  CONNACK = 0x02,
  DATA    = 0x03,
  ACK     = 0x04,
  CONTROL = 0x05
};

// Tipos de Controle 
enum ControlType : uint8_t {
  END_SESSION = 0x01
};

// Estruturas de Pacote
const int PACKET_SIZE = 32;

struct ConnectPacket {
  uint8_t type;
  uint16_t client_id;
  uint8_t padding[PACKET_SIZE - 3];
} __attribute__((packed));

struct ConnAckPacket {
  uint8_t type = CONNACK;
  uint8_t return_code = 0;
} __attribute__((packed));

struct DataPacket {
  uint8_t type;
  uint16_t id;
  char payload[PACKET_SIZE - 3];
} __attribute__((packed));

struct AckPacket {
  uint8_t type = ACK;
  uint16_t id;
} __attribute__((packed));

struct ControlPacket {
  uint8_t type;
  uint8_t control_type;
  uint8_t padding[PACKET_SIZE - 2];
} __attribute__((packed));

// Variáveis para Medição de Throughput 
unsigned long session_start_time = 0;
unsigned long total_bytes_received = 0;
uint16_t expected_packet_id = 0;

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  e22.begin();
  delay(500);

  // Configuração do rádio 
  ResponseStructContainer c = e22.getConfiguration();
  if (c.status.code == E22_SUCCESS) {
    Configuration config = *(Configuration*) c.data;
    config.ADDH = highByte(MY_ADDRESS);
    config.ADDL = lowByte(MY_ADDRESS);
    config.TRANSMISSION_MODE.fixedTransmission = FT_FIXED_TRANSMISSION;
    e22.setConfiguration(config, WRITE_CFG_PWR_DWN_SAVE);
    Serial.println("Módulo Receptor LoRa configurado. Aguardando conexão...");
    c.close();
  }
}

void loop() {
  if (e22.available() > 0) {
    byte received_packet[PACKET_SIZE];
    ResponseStructContainer rsc = e22.receiveMessage(sizeof(received_packet));

    if (rsc.status.code == E22_SUCCESS && rsc.data != nullptr) {
      memcpy(received_packet, rsc.data, sizeof(received_packet));
      uint8_t packet_type = received_packet[0];

      switch (packet_type) {
        case CONNECT: {
          ConnectPacket connect;
          memcpy(&connect, received_packet, sizeof(connect));
          Serial.println("Recebido CONNECT do client ID: " + String(connect.client_id));
          
          // Envia CONNACK de volta
          ConnAckPacket connack_packet;
          e22.sendFixedMessage(highByte(connect.client_id), lowByte(connect.client_id), LORA_CHANNEL, &connack_packet, sizeof(connack_packet));
          Serial.println("CONNACK enviado.");
          break;
        }
        case DATA: {
          DataPacket data;
          memcpy(&data, received_packet, sizeof(data));

          // Lógica para garantir a ordem dos pacotes
          if (data.id == expected_packet_id) {
            Serial.println("Recebido pacote de dados ID: " + String(data.id));

            // Inicia o timer na chegada do primeiro pacote
            if (data.id == 0) {
              session_start_time = millis();
              total_bytes_received = 0;
              Serial.println("Timer de throughput iniciado.");
            }

            total_bytes_received += sizeof(DataPacket);

            // Envia o ACK para o pacote recebido
            AckPacket ack;
            ack.id = data.id;
            e22.sendFixedMessage(highByte(0x0001), lowByte(0x0001), LORA_CHANNEL, &ack, sizeof(ack)); // Endereço do cliente é 0x0001
            expected_packet_id++;

          } else {
            Serial.println("Pacote fora de ordem. Esperado: " + String(expected_packet_id) + ", Recebido: " + String(data.id) + ". Descartado.");
          }
          break;
        }
        case CONTROL: {
          ControlPacket control;
          memcpy(&control, received_packet, sizeof(control));

          if (control.control_type == END_SESSION) {
            Serial.println("Recebido sinal de FIM DE SESSÃO.");
            calculate_throughput();
            // Reseta para a próxima sessão
            session_start_time = 0;
            total_bytes_received = 0;
            expected_packet_id = 0;
          }
          break;
        }
      }
      rsc.close();
    }
  }
}

void calculate_throughput() {
  if (session_start_time == 0 || total_bytes_received == 0) {
    Serial.println("Nenhum dado recebido para calcular throughput.");
    return;
  }
  unsigned long session_end_time = millis();
  float duration_s = (session_end_time - session_start_time) / 1000.0;
  float throughput_bps = (total_bytes_received * 8) / duration_s;

  Serial.println("--- RESULTADO DO THROUGHPUT ---");
  Serial.println("Total de Bytes Recebidos: " + String(total_bytes_received));
  Serial.println("Tempo Total da Sessão: " + String(duration_s, 2) + " s");
  Serial.println("Throughput: " + String(throughput_bps, 2) + " bps");
  Serial.println("------------------------------");
}